// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Push all callee-saved registers to get them on the stack for conservative
// stack scanning.
//
// See asm/x64/push_registers_asm.cc for why the function is not generated
// using clang.
//
// Calling convention source:
// https://loongson.github.io/LoongArch-Documentation/LoongArch-ELF-ABI-EN.html
asm(".global PAPushAllRegistersAndIterateStack             \n"
    ".type PAPushAllRegistersAndIterateStack, %function    \n"
    ".hidden PAPushAllRegistersAndIterateStack             \n"
    "PAPushAllRegistersAndIterateStack:                    \n"
    // Push all callee-saved registers and save return address.
    "  addi.d $sp, $sp, -96                              \n"
    // Save return address.
    "  st.d $ra, $sp, 88                                 \n"
    // sp is callee-saved.
    "  st.d $sp, $sp, 80                                 \n"
    // s0-s9(fp) are callee-saved.
    "  st.d $fp, $sp, 72                                 \n"
    "  st.d $s8, $sp, 64                                 \n"
    "  st.d $s7, $sp, 56                                 \n"
    "  st.d $s6, $sp, 48                                 \n"
    "  st.d $s5, $sp, 40                                 \n"
    "  st.d $s4, $sp, 32                                 \n"
    "  st.d $s3, $sp, 24                                 \n"
    "  st.d $s2, $sp, 16                                 \n"
    "  st.d $s1, $sp, 8                                  \n"
    "  st.d $s0, $sp, 0                                  \n"
    // Maintain frame pointer(fp is s9).
    "  move $fp, $sp                                     \n"
    // Pass 1st parameter (a0) unchanged (Stack*).
    // Pass 2nd parameter (a1) unchanged (StackVisitor*).
    // Save 3rd parameter (a2; IterateStackCallback) to a3.
    "  move $a3, $a2                                     \n"
    // Pass 3rd parameter as sp (stack pointer).
    "  move $a2, $sp                                     \n"
    // Call the callback.
    "  jirl $ra, $a3, 0                                  \n"
    // Load return address.
    "  ld.d $ra, $sp, 88                                 \n"
    // Restore frame pointer.
    "  ld.d $fp, $sp, 72                                 \n"
    "  addi.d $sp, $sp, 96                               \n"
    "  jr $ra                                            \n");
