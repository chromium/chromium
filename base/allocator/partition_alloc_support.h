// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/base_export.h"

namespace base {
namespace allocator {

#if defined(PA_ALLOW_PCSCAN)
BASE_EXPORT void RegisterPCScanStatsReporter();
#endif

}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_SUPPORT_H_
