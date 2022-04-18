// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_
#define BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <stdint.h>

namespace base {

// Static class responsible for the platform-specific logic to map a shared
// memory region into the virtual address space of the process.
class BASE_EXPORT PlatformSharedMemoryMapper {
 public:
  PlatformSharedMemoryMapper() = delete;

  static absl::optional<span<uint8_t>> Map(
      subtle::PlatformSharedMemoryHandle handle,
      bool write_allowed,
      uint64_t offset,
      size_t size);

  static void Unmap(span<uint8_t> mapping);

 private:
  static absl::optional<span<uint8_t>> MapInternal(
      subtle::PlatformSharedMemoryHandle handle,
      bool write_allowed,
      uint64_t offset,
      size_t size);

  static void UnmapInternal(span<uint8_t> mapping);
};

}  // namespace base

#endif  // BASE_MEMORY_PLATFORM_SHARED_MEMORY_MAPPER_H_
