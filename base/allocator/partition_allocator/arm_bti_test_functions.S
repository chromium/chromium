# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains a test function for checking Arm's branch target
# identification (BTI) feature, which helps mitigate jump-oriented
# programming. To get it working, BTI instructions must be executed
# on a compatible core, and the executable pages must be mapped with
# PROT_BTI. To validate that pages mapped with PROT_BTI are working
# correctly:
# 1) Allocate a read-write page.
# 2) Copy between the start and end symbols into that page.
# 3) Set the page to read-execute with PROT_BTI.
# 4) Call the first offset of the page, verify the result.
# 5) Call the second offset of the page (skipping the landing pad).
#    Verify that it crashes as expected.
# This test works irrespective of whether BTI is enabled for C/C++
# objects via -mbranch-protection=standard.

.text
.global arm_bti_test_function
.global arm_bti_test_function_invalid_offset
.global arm_bti_test_function_end
arm_bti_test_function:
    # Mark the start of this function as a valid call target.
    bti jc
    add x0, x0, #1
arm_bti_test_function_invalid_offset:
    # This label simulates calling an incomplete function.
    # Jumping here should crash systems which support BTI.
    add x0, x0, #2
    ret
arm_bti_test_function_end:
    nop

// For details see section "6.2 Program Property" in
// "ELF for the Arm 64-bit Architecture (AArch64)"
// https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst#62program-property
.pushsection .note.gnu.property, "a";
.balign 8;
.long 4;
.long 0x10;
.long 0x5;
.asciz "GNU";
.long 0xc0000000; /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
.long 4;
.long 1;           /* GNU_PROPERTY_AARCH64_BTI */;
.long 0;
.popsection

