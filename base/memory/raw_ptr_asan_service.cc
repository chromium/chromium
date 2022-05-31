// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_service.h"

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"

namespace base::internal {

RawPtrAsanService RawPtrAsanService::instance_;

namespace {
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_mapping.h#L154
constexpr size_t kShadowScale = 3;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_allocator.cpp#L143
constexpr size_t kChunkHeaderSize = 16;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L138
constexpr uint8_t kAsanHeapLeftRedzoneMagic = 0xfa;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L145
constexpr uint8_t kAsanUserPoisonedMemoryMagic = 0xf7;
}  // namespace

// Mark the first eight bytes of every allocation's header as "user poisoned".
// This allows us to filter out allocations made before BRP-ASan is activated.
// The change shouldn't reduce the regular ASan coverage.

// static
NO_SANITIZE("address")
void RawPtrAsanService::MallocHook(const volatile void* ptr, size_t size) {
  uint8_t* header =
      static_cast<uint8_t*>(const_cast<void*>(ptr)) - kChunkHeaderSize;
  *RawPtrAsanService::GetInstance().GetShadow(header) =
      kAsanUserPoisonedMemoryMagic;
}

NO_SANITIZE("address")
bool RawPtrAsanService::IsSupportedAllocation(void* allocation_start) const {
  uint8_t* header = static_cast<uint8_t*>(allocation_start) - kChunkHeaderSize;
  return *GetShadow(header) == kAsanUserPoisonedMemoryMagic;
}

NO_SANITIZE("address")
void RawPtrAsanService::Configure(Mode mode) {
  CHECK_EQ(mode_, Mode::kUninitialized);

  if (mode == Mode::kEnabled) {
    // The constants we use aren't directly exposed by the API, so
    // validate them at runtime as carefully as possible.
    size_t shadow_scale;
    __asan_get_shadow_mapping(&shadow_scale, &shadow_offset_);
    CHECK_EQ(shadow_scale, kShadowScale);

    uint8_t* dummy_alloc = new uint8_t;
    CHECK_EQ(*GetShadow(dummy_alloc - kChunkHeaderSize),
             kAsanHeapLeftRedzoneMagic);

    __asan_poison_memory_region(dummy_alloc, 1);
    CHECK_EQ(*GetShadow(dummy_alloc), kAsanUserPoisonedMemoryMagic);
    delete dummy_alloc;

    __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook);
  }

  mode_ = mode;
}

uint8_t* RawPtrAsanService::GetShadow(void* ptr) const {
  return reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(ptr) >> kShadowScale) + shadow_offset_);
}

}  // namespace base::internal
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
