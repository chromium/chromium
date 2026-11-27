// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/page_allocator_internal.h"
#include "partition_alloc/internal/partition_address_space_internal.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS) &&                                  \
    PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_BUILDFLAG(IS_ASAN) && \
    PA_CONFIG(HAS_LINUX_KERNEL)
#include <sanitizer/asan_interface.h>
#include <sys/mman.h>

#include "partition_alloc/zero_segment.h"

#if PA_USE_DEATH_TESTS()
#include <algorithm>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#endif  // PA_USE_DEATH_TESTS()
#endif

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

TEST(PartitionAddressSpaceTest, ZeroSegment) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
  const size_t expected_min_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;
  // We assume that we are always able to acquire the configured size when
  // executing a unit test. On Android, this test runs in a raw native
  // executable (`base_unittests_android_death_tests`) without ART, so it works
  // even prior to Android 17 (which introduced the Native Zygote).
  EXPECT_GE(size, expected_min_size);
#else
  EXPECT_EQ(size, 0u);
#endif
}

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
TEST(PartitionAddressSpaceTest, WellKnownReadOnlyRegions) {
  const size_t min_zero_segment_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;
  const WellKnownReadOnlyRegions well_known =
      GetWellKnownReadOnlyRegions(min_zero_segment_size);
  ASSERT_GE(well_known.count, 1u);
  ASSERT_LE(well_known.count, WellKnownReadOnlyRegions::kMaxRegions);
  EXPECT_EQ(well_known.regions[0].address, 0u);
  for (size_t i = 0; i < well_known.count; ++i) {
    EXPECT_LT(well_known.regions[i].address, min_zero_segment_size);
    EXPECT_GT(well_known.regions[i].size, 0u);
    EXPECT_EQ(well_known.regions[i].address % PageAllocationGranularity(), 0u);
    EXPECT_EQ(well_known.regions[i].size % PageAllocationGranularity(), 0u);
    if (i > 0) {
      EXPECT_GT(
          well_known.regions[i].address,
          well_known.regions[i - 1].address + well_known.regions[i - 1].size);
    }
  }
}
#endif  // PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_USE_DEATH_TESTS()
TEST(PartitionAddressSpaceTest, ZeroSegmentInaccessibleBegin) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }
  EXPECT_DEATH(
      {
        volatile uintptr_t zero = 0;
        volatile char* ptr = reinterpret_cast<volatile char*>(zero);
        *ptr = 0;
      },
      "");
}

TEST(PartitionAddressSpaceTest, ZeroSegmentInaccessibleEnd) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }
  EXPECT_DEATH(
      {
        volatile char* ptr =
            reinterpret_cast<volatile char*>(static_cast<uintptr_t>(size - 1));
        *ptr = 0;
      },
      "");
}
#endif

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_BUILDFLAG(IS_ASAN) && \
    PA_CONFIG(HAS_LINUX_KERNEL)

#if PA_USE_DEATH_TESTS()
namespace {

PA_NOINLINE PA_NO_SANITIZE("address") void UninstrumentedWrite(
    uintptr_t address) {
  *reinterpret_cast<volatile char*>(address) = 0;
}

}  // namespace

TEST(PartitionAddressSpaceTest, ZeroSegmentAsanUninstrumentedWriteFaults) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }

  size_t shadow_scale = 0;
  size_t shadow_offset = 0;
  __asan_get_shadow_mapping(&shadow_scale, &shadow_offset);

  // On x64 Linux ASan, LowShadow lies inside the zero segment
  // (`shadow_offset == 0x7fff8000`) and is mapped PROT_READ so uninstrumented
  // writes (e.g. from JIT/Wasm) fault instead of corrupting ASan shadow
  // memory. On arm64 / zero-based shadow, `shadow_offset` is 0 or outside the
  // zero segment, so probe 2 GiB inside the zero segment instead.
  constexpr uintptr_t kFallbackTarget = uintptr_t{2} << 30;  // 2 GiB
  const uintptr_t target =
      (shadow_offset != 0 && shadow_offset < size)
          ? static_cast<uintptr_t>(shadow_offset)
          : std::min(kFallbackTarget, static_cast<uintptr_t>(size - 1));

  EXPECT_DEATH(UninstrumentedWrite(target), "");
}

TEST(PartitionAddressSpaceTest, ZeroSegmentAsanNullDerefReport) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }

  size_t shadow_scale = 0;
  size_t shadow_offset = 0;
  __asan_get_shadow_mapping(&shadow_scale, &shadow_offset);
  if (shadow_offset != 0) {
    // Directly verify LowShadow remains readable (skip on zero-based shadow
    // where MEM_TO_SHADOW(0) == 0).
    EXPECT_FALSE(__asan_address_is_poisoned(nullptr));
  }

  // Marking LowShadow PROT_READ (instead of PROT_NONE) preserves ASan's
  // compiler-emitted shadow check on null dereferences so the fault still
  // occurs at address 0 rather than in LowShadow. `base_unittests` installs
  // `StackDumpSignalHandler` on SIGSEGV (reporting `SEGV_MAPERR 000000000000`)
  // unless `ASAN_OPTIONS=allow_user_segv_handler=0` is set (in which case ASan
  // prints `HINT: address points to the zero page.`).
  EXPECT_DEATH(
      {
        volatile uintptr_t zero = 0;
        volatile char* ptr = reinterpret_cast<volatile char*>(zero);
        *ptr = 0;
      },
      "address points to the zero page|SEGV_MAPERR 000000000000");
}
#endif  // PA_USE_DEATH_TESTS()

TEST(PartitionAddressSpaceTest, ZeroSegmentAsanLowMemReserved) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }

  const size_t page_size = SystemPageSize();
  size_t shadow_scale = 0;
  size_t shadow_offset = 0;
  __asan_get_shadow_mapping(&shadow_scale, &shadow_offset);

  constexpr uintptr_t kProbeAddr = 0x10000000;  // 256 MiB
  ASSERT_EQ(kProbeAddr % page_size, 0u);
  ASSERT_LE(kProbeAddr + page_size, size);
  if (shadow_offset != 0) {
    // Verify the probe address lies strictly within LowMem (below LowShadow).
    ASSERT_GE(shadow_offset, page_size);
    const uintptr_t low_shadow_start =
        static_cast<uintptr_t>(shadow_offset) - page_size;
    ASSERT_LE(kProbeAddr + page_size, low_shadow_start);
  }

  void* mapped =
      mmap(reinterpret_cast<void*>(kProbeAddr), page_size,
           PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapped != MAP_FAILED) {
    // Relocation away from `kProbeAddr` means the probe address is already
    // reserved as inaccessible and cannot be mapped as writable.
    EXPECT_NE(mapped, reinterpret_cast<void*>(kProbeAddr));
    munmap(mapped, page_size);
  }
}

TEST(PartitionAddressSpaceTest, ZeroSegmentAsanReinitIsIdempotent) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }

  // Simulates a second DSO statically linking PartitionAlloc running
  // `ZeroSegment::Init()` after the zero segment is already established.
  EXPECT_EQ(ZeroSegment::ReinitForAsanForTesting(), size);
  EXPECT_EQ(PartitionAddressSpace::GetZeroSegmentSize(), size);
}

#endif  // PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_BUILDFLAG(IS_ASAN) &&
        // PA_CONFIG(HAS_LINUX_KERNEL)

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
