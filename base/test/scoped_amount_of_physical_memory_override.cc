// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_amount_of_physical_memory_override.h"

#include "base/check_op.h"
#include "base/system/sys_info.h"

namespace base::test {

ScopedAmountOfPhysicalMemoryOverride::ScopedAmountOfPhysicalMemoryOverride(
    ByteCount amount_of_memory) {
  CHECK_GT(amount_of_memory, ByteCount(0));
  old_amount_of_physical_memory_ =
      base::SysInfo::SetAmountOfPhysicalMemoryForTesting(amount_of_memory);
}

ScopedAmountOfPhysicalMemoryOverride::~ScopedAmountOfPhysicalMemoryOverride() {
  if (old_amount_of_physical_memory_) {
    base::SysInfo::SetAmountOfPhysicalMemoryForTesting(
        *old_amount_of_physical_memory_);
  } else {
    base::SysInfo::ClearAmountOfPhysicalMemoryForTesting();
  }
}

}  // namespace base::test
