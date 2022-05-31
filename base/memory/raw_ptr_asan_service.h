// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
#define BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_

#include "base/allocator/buildflags.h"

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <cstddef>
#include <cstdint>

#include "base/base_export.h"

namespace base::internal {

BASE_EXPORT
class RawPtrAsanService {
 public:
  enum class Mode {
    kUninitialized,
    kDisabled,
    kEnabled,
  };

  void Configure(Mode mode);
  Mode mode() const { return mode_; }

  bool IsSupportedAllocation(void*) const;

  static RawPtrAsanService& GetInstance() { return instance_; }

 private:
  uint8_t* GetShadow(void* ptr) const;

  static void MallocHook(const volatile void*, size_t);
  static void FreeHook(const volatile void*) {}

  Mode mode_ = Mode::kUninitialized;
  size_t shadow_offset_ = 0;

  static RawPtrAsanService instance_;  // Not a static local variable because
                                       // `GetInstance()` is used in hot paths.
};

}  // namespace base::internal

#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#endif  // BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
