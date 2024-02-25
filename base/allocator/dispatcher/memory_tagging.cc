// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/memory_tagging.h"

namespace base::allocator::dispatcher {
static_assert(
    MTEMode::kUndefined ==
    ConvertToMTEMode(partition_alloc::TagViolationReportingMode::kUndefined));
static_assert(
    MTEMode::kDisabled ==
    ConvertToMTEMode(partition_alloc::TagViolationReportingMode::kDisabled));
static_assert(
    MTEMode::kSynchronous ==
    ConvertToMTEMode(partition_alloc::TagViolationReportingMode::kSynchronous));
static_assert(MTEMode::kAsynchronous ==
              ConvertToMTEMode(
                  partition_alloc::TagViolationReportingMode::kAsynchronous));
}  // namespace base::allocator::dispatcher
