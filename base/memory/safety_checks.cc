// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/safety_checks.h"

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#include "base/allocator/partition_alloc_support.h"
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

namespace base {

void SetDoubleFreeOrCorruptionDetectedFn(void (*fn)(uintptr_t)) {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  base::allocator::SetDoubleFreeOrCorruptionDetectedFn(fn);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
}

}  // namespace base
