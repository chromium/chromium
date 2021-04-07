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
  lib_info.set_use_memfd(false);
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

TEST_F(ModernLinkerTest, CreatedRegionIsSealedMemfd) {
  if (!NativeLibInfo::SharedMemoryFunctionsSupportedForTesting())
    return;

  // Return early if there is no support for memfd_create(2).
  utsname uts;
  int major, minor;
  if (uname(&uts) == -1 || sscanf(uts.release, "%d.%d", &major, &minor) != 2) {
    LOG(ERROR) << "Could not read kernel version";
    return;
  }
  if (major < 3 || (major == 3 && minor < 17)) {
    LOG(INFO) << "Kernel too old for memfd, test skipped";
    return;
  }

  // Fill a synthetic RELRO region with 0xEE in private anonynous memory.
  constexpr size_t kRelroSize = 1 << 21;  // 2 MiB.
  void* relro_address = mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(MAP_FAILED, relro_address);
  NativeLibInfo lib_info = {0, 0};
  lib_info.set_use_memfd(true);
  lib_info.set_relro_info_for_testing(
      reinterpret_cast<uintptr_t>(relro_address), kRelroSize);
  memset(relro_address, 0xEE, kRelroSize);

  // Create shared RELRO.
  ASSERT_EQ(true, lib_info.CreateSharedRelroFdForTesting());
  int relro_fd = lib_info.get_relro_fd_for_testing();
  ASSERT_NE(-1, relro_fd);
  base::ScopedFD scoped_fd(relro_fd);

  // Check that a read-only mapping contains the data originally filled in.
  // Mapping memfd as read-only should be made with MAP_PRIVATE, but
  // nevertheless it will share physical pages.
  void* ro_address =
      mmap(nullptr, kRelroSize, PROT_READ, MAP_PRIVATE, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, ro_address);
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address));
  EXPECT_EQ(0, memcmp(relro_address, ro_address, kRelroSize));

  // Unmap the original writable mapping, just as it is done after initializing
  // the shared RELRO region.
  munmap(relro_address, kRelroSize);

  // Map the pages again to make them shared. As previously, MAP_PRIVATE still
  // shares the physical pages.
  void* ro_address2 =
      mmap(nullptr, kRelroSize, PROT_READ, MAP_PRIVATE, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, ro_address2);

  // Compare the two read-only mappings
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address2));
  EXPECT_EQ(0, memcmp(ro_address, ro_address2, kRelroSize));

  // Check that attempts to mmap as readable and writable fails. This verifies
  // that the F_SEAL_WRITE was applied on the region.
  EXPECT_EQ(MAP_FAILED, mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                             MAP_SHARED, relro_fd, 0));

  // Check for F_SEAL_WRITE as above, just with a write-only.
  EXPECT_EQ(MAP_FAILED,
            mmap(nullptr, kRelroSize, PROT_WRITE, MAP_SHARED, relro_fd, 0));

  // A MAP_PRIVATE read-write mapping is allowed on the sealed memfd region.
  // Ensure it does not share memory with the original FD: write to it, unmap,
  // verify that the original data has not changed, even after remapping.
  void* rw_address = mmap(nullptr, kRelroSize, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, rw_address);
  uint32_t* first_word = reinterpret_cast<uint32_t*>(rw_address);
  EXPECT_EQ(0xEEEEEEEEU, *first_word);
  *first_word = 0xAAAAAAAAU;
  munmap(rw_address, kRelroSize);
  void* ro_address3 =
      mmap(nullptr, kRelroSize, PROT_READ, MAP_PRIVATE, relro_fd, 0);
  ASSERT_NE(MAP_FAILED, ro_address3);
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address3));
  EXPECT_EQ(0xEEEEEEEEU, *reinterpret_cast<uint32_t*>(ro_address2));

  // Unmap to clean up.
  munmap(ro_address, kRelroSize);
  munmap(ro_address2, kRelroSize);
  munmap(ro_address3, kRelroSize);
}

}  // namespace chromium_android_linker
