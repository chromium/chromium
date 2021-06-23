// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>
#include <sys/utsname.h>

#include "base/android/linker/modern_linker_jni.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromium_android_linker {

// These tests get linked with base_unittests and leave JNI uninitialized. The
// tests must not execute any parts relying on initialization with JNI_Onload().

class ModernLinkerTest : public testing::Test {
 public:
  ModernLinkerTest() = default;
  ~ModernLinkerTest() override = default;
};

// Checks that NativeLibInfo::CreateSharedRelroFd() creates a shared memory
// region that is 'sealed' as read-only. Uses ashmem for creating the region.
TEST_F(ModernLinkerTest, CreatedRegionIsSealedAshmem) {
  if (!NativeLibInfo::SharedMemoryFunctionsSupportedForTesting()) {
    // The ModernLinker uses functions from libandroid.so that are not available
    // on old Android releases. TODO(pasko): Add a fallback to ashmem for L-M,
    // as it is done in crazylinker and enable the testing below for these
    // devices.
    return;
  }

  // Fill a synthetic RELRO region with 0xEE in private anonynous memory.
  constexpr size_t kRelroSize = 1 << 21;  // 2 MiB.
  void* relro_address = mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, relro_address);
  NativeLibInfo lib_info = {0, 0};
  lib_info.set_relro_info_for_testing(
      reinterpret_cast<uintptr_t>(relro_address), kRelroSize);
  memset(relro_address, 0xEE, kRelroSize);

  // Create shared RELRO.
  ASSERT_EQ(true, lib_info.CreateSharedRelroFdForTesting());
  int relro_fd = lib_info.get_relro_fd_for_testing();
  ASSERT_NE(-1, relro_fd);
  base::ScopedFD scoped_fd(relro_fd);

  // Check that a read-only mapping contains the data originally filled in.
  void* ro_address =
      mmap(nullptr, kRelroSize, PROT_READ, MAP_SHARED, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, ro_address);
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address));
  int not_equal = memcmp(relro_address, ro_address, kRelroSize);
  EXPECT_EQ(0, not_equal);
  munmap(ro_address, kRelroSize);

  // Check that attempts to mmap with PROT_WRITE fail.
  EXPECT_EQ(MAP_FAILED, mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_SHARED, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED, mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED,
            mmap(nullptr, kRelroSize, PROT_WRITE, MAP_SHARED, relro_fd, 0));
  EXPECT_EQ(MAP_FAILED,
            mmap(nullptr, kRelroSize, PROT_WRITE, MAP_PRIVATE, relro_fd, 0));
}

}  // namespace chromium_android_linker
