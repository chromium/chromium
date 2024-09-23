// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// See asm/x64/push_registers_clang.cc for why the function is not generated
// using clang.

// We maintain 16-byte alignment.
//
// Calling convention source:
// https://en.wikipedia.org/wiki/Calling_convention#ARM_(A64)

asm(
#if defined(__APPLE__)
    ".globl _PAPushAllRegistersAndIterateStack            \n"
    ".private_extern _PAPushAllRegistersAndIterateStack   \n"
    ".p2align 2                                           \n"
    "_PAPushAllRegistersAndIterateStack:                  \n"
#else  // !defined(__APPLE__)
    ".globl PAPushAllRegistersAndIterateStack             \n"
#if !defined(_WIN64)
    ".type PAPushAllRegistersAndIterateStack, %function   \n"
    ".hidden PAPushAllRegistersAndIterateStack            \n"
#endif  // !defined(_WIN64)
    ".p2align 2                                           \n"
    "PAPushAllRegistersAndIterateStack:                   \n"
#endif  // !defined(__APPLE__)
    // x19-x29 are callee-saved.
    "  stp x19, x20, [sp, #-16]!                          \n"
    "  stp x21, x22, [sp, #-16]!                          \n"
    "  stp x23, x24, [sp, #-16]!                          \n"
    "  stp x25, x26, [sp, #-16]!                          \n"
    "  stp x27, x28, [sp, #-16]!                          \n"
    "  stp fp, lr,   [sp, #-16]!                          \n"
    // Maintain frame pointer.
    "  mov fp, sp                                         \n"
    // Pass 1st parameter (x0) unchanged (Stack*).
    // Pass 2nd parameter (x1) unchanged (StackVisitor*).
    // Save 3rd parameter (x2; IterateStackCallback)
    "  mov x7, x2                                         \n"
    // Pass 3rd parameter as sp (stack pointer).
    "  mov x2, sp                                         \n"
    "  blr x7                                             \n"
    // Load return address and frame pointer.
    "  ldp fp, lr, [sp], #16                              \n"
    // Drop all callee-saved registers.
    "  add sp, sp, #80                                    \n"
    "  ret                                                \n");
