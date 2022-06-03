// Copyright 2015 The Chromium Authors. All rights reserved.
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
  void* address = mapping.memory();
  size_t start = reinterpret_cast<size_t>(address);
  size_t end = start + length;

  // Remove everything.
  ASSERT_EQ(0, madvise(address, length, MADV_DONTNEED));
  EXPECT_EQ(0, NativeLibraryPrefetcher::PercentageOfResidentCode(start, end));

  // Get everything back.
  ASSERT_EQ(0, mlock(address, length));
  EXPECT_EQ(100, NativeLibraryPrefetcher::PercentageOfResidentCode(start, end));
  munlock(address, length);
}
#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace android
}  // namespace base
#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)
