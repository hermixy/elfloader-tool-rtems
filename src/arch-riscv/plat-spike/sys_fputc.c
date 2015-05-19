/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(NICTA_GPL)
 */

/*
 * Platform-specific putchar implementation.
 */

#include "../stdint.h"
#include "../stdio.h"
#include "platform.h"

#define swap_csr(reg, val) ({ long __tmp; \
  asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "r"(val)); \
  __tmp; })

#define write_csr(reg, val) \
  asm volatile ("csrw " #reg ", %0" :: "r"(val))

volatile uint64_t magic_mem[8] __attribute__((aligned(64)));

static long syscall(long num, long arg0, long arg1, long arg2)
{
  register long a7 asm("a7") = num;
  register long a0 asm("a0") = arg0;
  register long a1 asm("a1") = arg1;
  register long a2 asm("a2") = arg2;
  asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7));
  return a0;
}

static long handle_frontend_syscall(long which, long arg0, long arg1, long arg2)
{
  magic_mem[0] = which;
  magic_mem[1] = arg0;
  magic_mem[2] = arg1;
  magic_mem[3] = arg2;
  __sync_synchronize();
  write_csr(mtohost, (long)magic_mem);
  while (swap_csr(mfromhost, 0) == 0);
  return magic_mem[0];
}

long handle_trap(uint32_t cause, uint32_t epc, uint64_t regs[32])
{
  long sys_ret = 0;

  if(cause == 7)
  {
    return 0; 
  } 

  sys_ret = handle_frontend_syscall(regs[17], regs[10], regs[11], regs[12]);

  regs[10] = sys_ret;
  return epc+4;
}

int
__fputc(int c, FILE *stream __attribute__((unused)))
{
  static __thread char buf[64] __attribute__((aligned(64)));
  static __thread int buflen = 0;
  buf[buflen++] = c;
  if (c == '\n' || buflen == sizeof(buf))
  {
    syscall(SYS_write, 1, (long)buf, buflen);
    buflen = 0;
  }
  return 0;
}
