// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/protected_memory.h"

#include <windows.h>

#include <stdint.h>

#include "base/memory/page_size.h"
#include "base/process/process_metrics.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

namespace base {

namespace {

bool SetMemory(void* start, void* end, DWORD prot) {
  CHECK(end > start);
  const uintptr_t page_mask = ~(base::GetPageSize() - 1);
  const uintptr_t page_start = reinterpret_cast<uintptr_t>(start) & page_mask;
  DWORD old_prot;
  return VirtualProtect(reinterpret_cast<void*>(page_start),
                        reinterpret_cast<uintptr_t>(end) - page_start, prot,
                        &old_prot) != 0;
}

}  // namespace

namespace internal {
bool IsMemoryReadOnly(const void* ptr) {
  const uintptr_t page_mask = ~(base::GetPageSize() - 1);
  const uintptr_t page_start = reinterpret_cast<uintptr_t>(ptr) & page_mask;

  MEMORY_BASIC_INFORMATION info;
  SIZE_T result =
      VirtualQuery(reinterpret_cast<LPCVOID>(page_start), &info, sizeof(info));

  return (result > 0U) && (info.Protect == PAGE_READONLY);
}
}  // namespace internal

bool AutoWritableMemoryBase::SetMemoryReadWrite(void* start, void* end) {
  return SetMemory(start, end, PAGE_READWRITE);
}

bool AutoWritableMemoryBase::SetMemoryReadOnly(void* start, void* end) {
  return SetMemory(start, end, PAGE_READONLY);
}

}  // namespace base
