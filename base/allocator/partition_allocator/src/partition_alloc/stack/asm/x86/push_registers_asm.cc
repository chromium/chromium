// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// See asm/x64/push_registers_clang.cc for why the function is not generated
// using clang.

// We maintain 16-byte alignment at calls. There is an 4-byte return address
// on the stack and we push 28 bytes which maintains 16-byte stack alignment
// at the call.
//
// The following assumes cdecl calling convention.
// Source: https://en.wikipedia.org/wiki/X86_calling_conventions#cdecl
asm(
#ifdef __APPLE__
    ".globl _PAPushAllRegistersAndIterateStack            \n"
    ".private_extern _PAPushAllRegistersAndIterateStack   \n"
    "_PAPushAllRegistersAndIterateStack:                  \n"
#elif defined(_WIN32)
    ".att_syntax                                          \n"
    ".globl _PAPushAllRegistersAndIterateStack            \n"
    "_PAPushAllRegistersAndIterateStack:                  \n"
#else  // !__APPLE__ && !_WIN32
    ".globl PAPushAllRegistersAndIterateStack             \n"
    ".type PAPushAllRegistersAndIterateStack, %function   \n"
    ".hidden PAPushAllRegistersAndIterateStack            \n"
    "PAPushAllRegistersAndIterateStack:                   \n"
#endif
    // [ IterateStackCallback ]
    // [ StackVisitor*        ]
    // [ Stack*               ]
    // [ ret                  ]
    // ebp is callee-saved. Maintain proper frame pointer for debugging.
    "  push %ebp                                          \n"
    "  movl %esp, %ebp                                    \n"
    "  push %ebx                                          \n"
    "  push %esi                                          \n"
    "  push %edi                                          \n"
    // Save 3rd parameter (IterateStackCallback).
    "  movl 28(%esp), %ecx                                \n"
    // Pass 3rd parameter as esp (stack pointer).
    "  push %esp                                          \n"
    // Pass 2nd parameter (StackVisitor*).
    "  push 28(%esp)                                      \n"
    // Pass 1st parameter (Stack*).
    "  push 28(%esp)                                      \n"
    "  call *%ecx                                         \n"
    // Pop the callee-saved registers.
    "  addl $24, %esp                                     \n"
    // Restore rbp as it was used as frame pointer.
    "  pop %ebp                                           \n"
    "  ret                                                \n");
