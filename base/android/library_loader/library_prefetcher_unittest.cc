// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_prefetcher.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/memory/writable_shared_memory_region.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
namespace base {
namespace android {

// Fails with ASAN, crbug.com/570423.
#if !defined(ADDRESS_SANITIZER)
namespace {
const size_t kPageSize = 4096;
}  // namespace

// https://crbug.com/1056021 - flaky on Nexus 5.
TEST(NativeLibraryPrefetcherTest, DISABLED_TestPercentageOfResidentCode) {
  size_t length = 4 * kPageSize;
  auto shared_region = base::WritableSharedMemoryRegion::Create(length);
  ASSERT_TRUE(shared_region.IsValid());
  auto mapping = shared_region.Map();
  ASSERT_TRUE(mapping.IsValid());
  // SAFETY: There's no public way to get a span of the full mapped memory size.
  // The `mapped_size()` is larger then `size()` but is the actual size of the
  // shared memory backing.
  span<uint8_t> memory =
      UNSAFE_BUFFERS(base::span(mapping.data(), mapping.mapped_size()));
  auto start = reinterpret_cast<uintptr_t>(&*memory.begin());
  auto end = reinterpret_cast<uintptr_t>(&*memory.end());

  // Remove everything.
  ASSERT_EQ(0, madvise(memory.data(), memory.size(), MADV_DONTNEED));
  EXPECT_EQ(0, NativeLibraryPrefetcher::PercentageOfResidentCode(start, end));

  // Get everything back.
  ASSERT_EQ(0, mlock(memory.data(), memory.size()));
  EXPECT_EQ(100, NativeLibraryPrefetcher::PercentageOfResidentCode(start, end));
  munlock(memory.data(), memory.size());
}
#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace android
}  // namespace base
#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)
