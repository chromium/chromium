// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_

#include "build/build_config.h"
#if defined(ARCH_CPU_ARM64)
extern "C" {
int64_t arm_bti_test_function(int64_t);
int64_t arm_bti_test_function_invalid_offset(int64_t);
void arm_bti_test_function_end(void);
}
#endif  // defined(ARCH_CPU_ARM64)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ARM_BTI_TEST_FUNCTIONS_H_
