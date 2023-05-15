// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_amount_of_physical_memory_override.h"

#include "base/check_op.h"
#include "base/system/sys_info.h"

namespace base::test {

ScopedAmountOfPhysicalMemoryOverride::ScopedAmountOfPhysicalMemoryOverride(
    uint64_t amount_of_memory_mb) {
  CHECK_GT(amount_of_memory_mb, 0u);
  old_amount_of_physical_memory_mb_ =
      base::SysInfo::SetAmountOfPhysicalMemoryMbForTesting(amount_of_memory_mb);
}

ScopedAmountOfPhysicalMemoryOverride::~ScopedAmountOfPhysicalMemoryOverride() {
  if (old_amount_of_physical_memory_mb_) {
    base::SysInfo::SetAmountOfPhysicalMemoryMbForTesting(
        *old_amount_of_physical_memory_mb_);
  } else {
    base::SysInfo::ClearAmountOfPhysicalMemoryMbForTesting();
  }
}

}  // namespace base::test
