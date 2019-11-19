// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/atomicops.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/shared_memory_handle.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_ANDROID)
#include "base/callback.h"
#endif

#if defined(OS_POSIX)
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(OS_LINUX)
#include <sys/syscall.h>
#endif

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#endif

namespace base {

namespace {

// Each thread will open the shared memory.  Each thread will take a different 4
// byte int pointer, and keep changing it, with some small pauses in between.
// Verify that each thread's value in the shared memory is always correct.
class MultipleThreadMain : public PlatformThread::Delegate {
 public:
  static const uint32_t kDataSize = 1024;

  MultipleThreadMain(int16_t id, SharedMemoryHandle handle)
      : id_(id), shm_(handle, false) {}
  ~MultipleThreadMain() override = default;

  // PlatformThread::Delegate interface.
  void ThreadMain() override {
    EXPECT_TRUE(shm_.Map(kDataSize));
    int* ptr = static_cast<int*>(shm_.memory()) + id_;
    EXPECT_EQ(0, *ptr);

    for (int idx = 0; idx < 100; idx++) {
      *ptr = idx;
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
      EXPECT_EQ(*ptr, idx);
    }
    // Reset back to 0 for the next test that uses the same name.
    *ptr = 0;

    shm_.Unmap();
  }

 private:
  int16_t id_;
  SharedMemory shm_;

  DISALLOW_COPY_AND_ASSIGN(MultipleThreadMain);
};

enum class Mode {
  Default,
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  DisableDevShm = 1,
#endif
};

class SharedMemoryTest : public ::testing::TestWithParam<Mode> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case Mode::Default:
        break;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
      case Mode::DisableDevShm:
        CommandLine* cmdline = CommandLine::ForCurrentProcess();
        cmdline->AppendSwitch(switches::kDisableDevShmUsage);
        break;
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)
    }
  }
};

}  // namespace

// Check that memory is still mapped after its closed.
TEST_P(SharedMemoryTest, CloseNoUnmap) {
  const size_t kDataSize = 4096;

  SharedMemory memory;
  ASSERT_TRUE(memory.CreateAndMapAnonymous(kDataSize));
  char* ptr = static_cast<char*>(memory.memory());
  ASSERT_NE(ptr, static_cast<void*>(nullptr));
  memset(ptr, 'G', kDataSize);

  memory.Close();

  EXPECT_EQ(ptr, memory.memory());
  EXPECT_TRUE(!memory.handle().IsValid());

  for (size_t i = 0; i < kDataSize; i++) {
    EXPECT_EQ('G', ptr[i]);
  }

  memory.Unmap();
  EXPECT_EQ(nullptr, memory.memory());
}

// Create a set of N threads to each open a shared memory segment and write to
// it. Verify that they are always reading/writing consistent data.
TEST_P(SharedMemoryTest, MultipleThreads) {
  const int kNumThreads = 5;

  // On POSIX we have a problem when 2 threads try to create the shmem
  // (a file) at exactly the same time, since create both creates the
  // file and zerofills it.  We solve the problem for this unit test
  // (make it not flaky) by starting with 1 thread, then
  // intentionally don't clean up its shmem before running with
  // kNumThreads.

  SharedMemoryCreateOptions options;
  options.size = MultipleThreadMain::kDataSize;
  SharedMemory shm;
  EXPECT_TRUE(shm.Create(options));

  int threadcounts[] = { 1, kNumThreads };
  for (auto numthreads : threadcounts) {
    std::unique_ptr<PlatformThreadHandle[]> thread_handles;
    std::unique_ptr<MultipleThreadMain* []> thread_delegates;

    thread_handles.reset(new PlatformThreadHandle[numthreads]);
    thread_delegates.reset(new MultipleThreadMain*[numthreads]);

    // Spawn the threads.
    for (int16_t index = 0; index < numthreads; index++) {
      PlatformThreadHandle pth;
      thread_delegates[index] =
          new MultipleThreadMain(index, shm.handle().Duplicate());
      EXPECT_TRUE(PlatformThread::Create(0, thread_delegates[index], &pth));
      thread_handles[index] = pth;
    }

    // Wait for the threads to finish.
    for (int index = 0; index < numthreads; index++) {
      PlatformThread::Join(thread_handles[index]);
      delete thread_delegates[index];
    }
  }
}

// Allocate private (unique) shared memory with an empty string for a
// name.  Make sure several of them don't point to the same thing as
// we might expect if the names are equal.
TEST_P(SharedMemoryTest, AnonymousPrivate) {
  int i, j;
  int count = 4;
  bool rv;
  const uint32_t kDataSize = 8192;

  std::unique_ptr<SharedMemory[]> memories(new SharedMemory[count]);
  std::unique_ptr<int* []> pointers(new int*[count]);
  ASSERT_TRUE(memories.get());
  ASSERT_TRUE(pointers.get());

  for (i = 0; i < count; i++) {
    rv = memories[i].CreateAndMapAnonymous(kDataSize);
    EXPECT_TRUE(rv);
    int* ptr = static_cast<int*>(memories[i].memory());
    EXPECT_TRUE(ptr);
    pointers[i] = ptr;
  }

  for (i = 0; i < count; i++) {
    // zero out the first int in each except for i; for that one, make it 100.
    for (j = 0; j < count; j++) {
      if (i == j)
        pointers[j][0] = 100;
      else
        pointers[j][0] = 0;
    }
    // make sure there is no bleeding of the 100 into the other pointers
    for (j = 0; j < count; j++) {
      if (i == j)
        EXPECT_EQ(100, pointers[j][0]);
      else
        EXPECT_EQ(0, pointers[j][0]);
    }
  }

  for (i = 0; i < count; i++) {
    memories[i].Close();
  }
}

#if !(defined(OS_MACOSX) && !defined(OS_IOS))
// The Mach functionality is tested in shared_memory_mac_unittest.cc.
TEST_P(SharedMemoryTest, GetReadOnlyHandle) {
  StringPiece contents = "Hello World";

  SharedMemory writable_shmem;
  SharedMemoryCreateOptions options;
  options.size = contents.size();
  options.share_read_only = true;
  ASSERT_TRUE(writable_shmem.Create(options));
  ASSERT_TRUE(writable_shmem.Map(options.size));
  memcpy(writable_shmem.memory(), contents.data(), contents.size());
  EXPECT_TRUE(writable_shmem.Unmap());

  SharedMemoryHandle readonly_handle = writable_shmem.GetReadOnlyHandle();
  EXPECT_EQ(writable_shmem.handle().GetGUID(), readonly_handle.GetGUID());
  EXPECT_EQ(writable_shmem.handle().GetSize(), readonly_handle.GetSize());
  ASSERT_TRUE(readonly_handle.IsValid());
  SharedMemory readonly_shmem(readonly_handle, /*readonly=*/true);

  ASSERT_TRUE(readonly_shmem.Map(contents.size()));
  EXPECT_EQ(contents,
            StringPiece(static_cast<const char*>(readonly_shmem.memory()),
                        contents.size()));
  EXPECT_TRUE(readonly_shmem.Unmap());

#if defined(OS_ANDROID)
  // On Android, mapping a region through a read-only descriptor makes the
  // region read-only. Any writable mapping attempt should fail.
  ASSERT_FALSE(writable_shmem.Map(contents.size()));
#else
  // Make sure the writable instance is still writable.
  ASSERT_TRUE(writable_shmem.Map(contents.size()));
  StringPiece new_contents = "Goodbye";
  memcpy(writable_shmem.memory(), new_contents.data(), new_contents.size());
  EXPECT_EQ(new_contents,
            StringPiece(static_cast<const char*>(writable_shmem.memory()),
                        new_contents.size()));
#endif

  // We'd like to check that if we send the read-only segment to another
  // process, then that other process can't reopen it read/write.  (Since that
  // would be a security hole.)  Setting up multiple processes is hard in a
  // unittest, so this test checks that the *current* process can't reopen the
  // segment read/write.  I think the test here is stronger than we actually
  // care about, but there's a remote possibility that sending a file over a
  // pipe would transform it into read/write.
  SharedMemoryHandle handle = readonly_shmem.handle();

#if defined(OS_ANDROID)
  // The "read-only" handle is still writable on Android:
  // http://crbug.com/320865
  (void)handle;
#elif defined(OS_FUCHSIA)
  uintptr_t addr;
  EXPECT_NE(ZX_OK, zx::vmar::root_self()->map(
                       0, *zx::unowned_vmo(handle.GetHandle()), 0,
                       contents.size(), ZX_VM_PERM_WRITE, &addr))
      << "Shouldn't be able to map as writable.";

  zx::vmo duped_handle;
  EXPECT_NE(ZX_OK, zx::unowned_vmo(handle.GetHandle())
                       ->duplicate(ZX_RIGHT_WRITE, &duped_handle))
      << "Shouldn't be able to duplicate the handle into a writable one.";

  EXPECT_EQ(ZX_OK, zx::unowned_vmo(handle.GetHandle())
                       ->duplicate(ZX_RIGHT_READ, &duped_handle))
      << "Should be able to duplicate the handle into a readable one.";
#elif defined(OS_POSIX)
  int handle_fd = SharedMemory::GetFdFromSharedMemoryHandle(handle);
  EXPECT_EQ(O_RDONLY, fcntl(handle_fd, F_GETFL) & O_ACCMODE)
      << "The descriptor itself should be read-only.";

  errno = 0;
  void* writable = mmap(nullptr, contents.size(), PROT_READ | PROT_WRITE,
                        MAP_SHARED, handle_fd, 0);
  int mmap_errno = errno;
  EXPECT_EQ(MAP_FAILED, writable)
      << "It shouldn't be possible to re-mmap the descriptor writable.";
  EXPECT_EQ(EACCES, mmap_errno) << strerror(mmap_errno);
  if (writable != MAP_FAILED)
    EXPECT_EQ(0, munmap(writable, readonly_shmem.mapped_size()));

#elif defined(OS_WIN)
  EXPECT_EQ(NULL, MapViewOfFile(handle.GetHandle(), FILE_MAP_WRITE, 0, 0, 0))
      << "Shouldn't be able to map memory writable.";

  HANDLE temp_handle;
  BOOL rv = ::DuplicateHandle(GetCurrentProcess(), handle.GetHandle(),
                              GetCurrentProcess(), &temp_handle,
                              FILE_MAP_ALL_ACCESS, false, 0);
  EXPECT_EQ(FALSE, rv)
      << "Shouldn't be able to duplicate the handle into a writable one.";
  if (rv)
    win::ScopedHandle writable_handle(temp_handle);
  rv = ::DuplicateHandle(GetCurrentProcess(), handle.GetHandle(),
                         GetCurrentProcess(), &temp_handle, FILE_MAP_READ,
                         false, 0);
  EXPECT_EQ(TRUE, rv)
      << "Should be able to duplicate the handle into a readable one.";
  if (rv)
    win::ScopedHandle writable_handle(temp_handle);
#else
#error Unexpected platform; write a test that tries to make 'handle' writable.
#endif  // defined(OS_POSIX) || defined(OS_WIN)
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

TEST_P(SharedMemoryTest, ShareToSelf) {
  StringPiece contents = "Hello World";

  SharedMemory shmem;
  ASSERT_TRUE(shmem.CreateAndMapAnonymous(contents.size()));
  memcpy(shmem.memory(), contents.data(), contents.size());
  EXPECT_TRUE(shmem.Unmap());

  SharedMemoryHandle shared_handle = shmem.handle().Duplicate();
  ASSERT_TRUE(shared_handle.IsValid());
  EXPECT_TRUE(shared_handle.OwnershipPassesToIPC());
  EXPECT_EQ(shared_handle.GetGUID(), shmem.handle().GetGUID());
  EXPECT_EQ(shared_handle.GetSize(), shmem.handle().GetSize());
  SharedMemory shared(shared_handle, /*readonly=*/false);

  ASSERT_TRUE(shared.Map(contents.size()));
  EXPECT_EQ(
      contents,
      StringPiece(static_cast<const char*>(shared.memory()), contents.size()));

  shared_handle = shmem.handle().Duplicate();
  ASSERT_TRUE(shared_handle.IsValid());
  ASSERT_TRUE(shared_handle.OwnershipPassesToIPC());
  SharedMemory readonly(shared_handle, /*readonly=*/true);

  ASSERT_TRUE(readonly.Map(contents.size()));
  EXPECT_EQ(contents,
            StringPiece(static_cast<const char*>(readonly.memory()),
                        contents.size()));
}

TEST_P(SharedMemoryTest, ShareWithMultipleInstances) {
  static const StringPiece kContents = "Hello World";

  SharedMemory shmem;
  ASSERT_TRUE(shmem.CreateAndMapAnonymous(kContents.size()));
  // We do not need to unmap |shmem| to let |shared| map.
  const StringPiece shmem_contents(static_cast<const char*>(shmem.memory()),
                                   shmem.requested_size());

  SharedMemoryHandle shared_handle = shmem.handle().Duplicate();
  ASSERT_TRUE(shared_handle.IsValid());
  SharedMemory shared(shared_handle, /*readonly=*/false);
  ASSERT_TRUE(shared.Map(kContents.size()));
  // The underlying shared memory is created by |shmem|, so both
  // |shared|.requested_size() and |readonly|.requested_size() are zero.
  ASSERT_EQ(0U, shared.requested_size());
  const StringPiece shared_contents(static_cast<const char*>(shared.memory()),
                                    shmem.requested_size());

  shared_handle = shmem.handle().Duplicate();
  ASSERT_TRUE(shared_handle.IsValid());
  ASSERT_TRUE(shared_handle.OwnershipPassesToIPC());
  SharedMemory readonly(shared_handle, /*readonly=*/true);
  ASSERT_TRUE(readonly.Map(kContents.size()));
  ASSERT_EQ(0U, readonly.requested_size());
  const StringPiece readonly_contents(
      static_cast<const char*>(readonly.memory()),
      shmem.requested_size());

  // |shmem| should be able to update the content.
  memcpy(shmem.memory(), kContents.data(), kContents.size());

  ASSERT_EQ(kContents, shmem_contents);
  ASSERT_EQ(kContents, shared_contents);
  ASSERT_EQ(kContents, readonly_contents);

  // |shared| should also be able to update the content.
  memcpy(shared.memory(), ToLowerASCII(kContents).c_str(), kContents.size());

  ASSERT_EQ(StringPiece(ToLowerASCII(kContents)), shmem_contents);
  ASSERT_EQ(StringPiece(ToLowerASCII(kContents)), shared_contents);
  ASSERT_EQ(StringPiece(ToLowerASCII(kContents)), readonly_contents);
}

TEST_P(SharedMemoryTest, MapAt) {
  ASSERT_TRUE(SysInfo::VMAllocationGranularity() >= sizeof(uint32_t));
  const size_t kCount = SysInfo::VMAllocationGranularity();
  const size_t kDataSize = kCount * sizeof(uint32_t);

  SharedMemory memory;
  ASSERT_TRUE(memory.CreateAndMapAnonymous(kDataSize));
  uint32_t* ptr = static_cast<uint32_t*>(memory.memory());
  ASSERT_NE(ptr, static_cast<void*>(nullptr));

  for (size_t i = 0; i < kCount; ++i) {
    ptr[i] = i;
  }

  memory.Unmap();

  size_t offset = SysInfo::VMAllocationGranularity();
  ASSERT_TRUE(memory.MapAt(static_cast<off_t>(offset), kDataSize - offset));
  offset /= sizeof(uint32_t);
  ptr = static_cast<uint32_t*>(memory.memory());
  ASSERT_NE(ptr, static_cast<void*>(nullptr));
  for (size_t i = offset; i < kCount; ++i) {
    EXPECT_EQ(ptr[i - offset], i);
  }
}

TEST_P(SharedMemoryTest, MapTwice) {
  const uint32_t kDataSize = 1024;
  SharedMemory memory;
  bool rv = memory.CreateAndMapAnonymous(kDataSize);
  EXPECT_TRUE(rv);

  void* old_address = memory.memory();

  rv = memory.Map(kDataSize);
  EXPECT_FALSE(rv);
  EXPECT_EQ(old_address, memory.memory());
}

#if defined(OS_POSIX)
// This test is not applicable for iOS (crbug.com/399384).
// The Mach functionality is tested in shared_memory_mac_unittest.cc.
#if !defined(OS_MACOSX) && !defined(OS_IOS)
// Create a shared memory object, mmap it, and mprotect it to PROT_EXEC.
TEST_P(SharedMemoryTest, AnonymousExecutable) {
#if defined(OS_LINUX)
  // On Chromecast both /dev/shm and /tmp are mounted with 'noexec' option,
  // which makes this test fail. But Chromecast doesn't use NaCL so we don't
  // need this.
  if (!IsPathExecutable(FilePath("/dev/shm")) &&
      !IsPathExecutable(FilePath("/tmp"))) {
    return;
  }
#endif  // OS_LINUX
  const uint32_t kTestSize = 1 << 16;

  SharedMemory shared_memory;
  SharedMemoryCreateOptions options;
  options.size = kTestSize;
  options.executable = true;

  EXPECT_TRUE(shared_memory.Create(options));
  EXPECT_TRUE(shared_memory.Map(shared_memory.requested_size()));

  EXPECT_EQ(0, mprotect(shared_memory.memory(), shared_memory.requested_size(),
                        PROT_READ | PROT_EXEC));
}
#endif  // !defined(OS_MACOSX) && !defined(OS_IOS)

#if defined(OS_ANDROID)
// This test is restricted to Android since there is no way on other platforms
// to guarantee that a region can never be mapped with PROT_EXEC. E.g. on
// Linux, anonymous shared regions come from /dev/shm which can be mounted
// without 'noexec'. In this case, anything can perform an mprotect() to
// change the protection mask of a given page.
TEST(SharedMemoryTest, AnonymousIsNotExecutableByDefault) {
  const uint32_t kTestSize = 1 << 16;

  SharedMemory shared_memory;
  SharedMemoryCreateOptions options;
  options.size = kTestSize;

  EXPECT_TRUE(shared_memory.Create(options));
  EXPECT_TRUE(shared_memory.Map(shared_memory.requested_size()));

  errno = 0;
  EXPECT_EQ(-1, mprotect(shared_memory.memory(), shared_memory.requested_size(),
                         PROT_READ | PROT_EXEC));
  EXPECT_EQ(EACCES, errno);
}
#endif  // OS_ANDROID

// Android supports a different permission model than POSIX for its "ashmem"
// shared memory implementation. So the tests about file permissions are not
// included on Android. Fuchsia does not use a file-backed shared memory
// implementation.

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

// Set a umask and restore the old mask on destruction.
class ScopedUmaskSetter {
 public:
  explicit ScopedUmaskSetter(mode_t target_mask) {
    old_umask_ = umask(target_mask);
  }
  ~ScopedUmaskSetter() { umask(old_umask_); }
 private:
  mode_t old_umask_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedUmaskSetter);
};

// Create a shared memory object, check its permissions.
#if !(defined(OS_MACOSX) && !defined(OS_IOS))
// The Mach functionality is tested in shared_memory_mac_unittest.cc.
TEST_P(SharedMemoryTest, FilePermissionsAnonymous) {
  const uint32_t kTestSize = 1 << 8;

  SharedMemory shared_memory;
  SharedMemoryCreateOptions options;
  options.size = kTestSize;
  // Set a file mode creation mask that gives all permissions.
  ScopedUmaskSetter permissive_mask(S_IWGRP | S_IWOTH);

  EXPECT_TRUE(shared_memory.Create(options));

  int shm_fd =
      SharedMemory::GetFdFromSharedMemoryHandle(shared_memory.handle());
  struct stat shm_stat;
  EXPECT_EQ(0, fstat(shm_fd, &shm_stat));
  // Neither the group, nor others should be able to read the shared memory
  // file.
  EXPECT_FALSE(shm_stat.st_mode & S_IRWXO);
  EXPECT_FALSE(shm_stat.st_mode & S_IRWXG);
}
#endif  // !(defined(OS_MACOSX) && !defined(OS_IOS)

// Create a shared memory object, check its permissions.
#if !(defined(OS_MACOSX) && !defined(OS_IOS))
// The Mach functionality is tested in shared_memory_mac_unittest.cc.
TEST_P(SharedMemoryTest, FilePermissionsNamed) {
  const uint32_t kTestSize = 1 << 8;

  SharedMemory shared_memory;
  SharedMemoryCreateOptions options;
  options.size = kTestSize;

  // Set a file mode creation mask that gives all permissions.
  ScopedUmaskSetter permissive_mask(S_IWGRP | S_IWOTH);

  EXPECT_TRUE(shared_memory.Create(options));

  int fd = SharedMemory::GetFdFromSharedMemoryHandle(shared_memory.handle());
  struct stat shm_stat;
  EXPECT_EQ(0, fstat(fd, &shm_stat));
  // Neither the group, nor others should have been able to open the shared
  // memory file while its name existed.
  EXPECT_FALSE(shm_stat.st_mode & S_IRWXO);
  EXPECT_FALSE(shm_stat.st_mode & S_IRWXG);
}
#endif  // !(defined(OS_MACOSX) && !defined(OS_IOS)
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#endif  // defined(OS_POSIX)

// Map() will return addresses which are aligned to the platform page size, this
// varies from platform to platform though.  Since we'd like to advertise a
// minimum alignment that callers can count on, test for it here.
TEST_P(SharedMemoryTest, MapMinimumAlignment) {
  static const int kDataSize = 8192;

  SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(kDataSize));
  EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(
      shared_memory.memory()) & (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
  shared_memory.Close();
}

#if defined(OS_WIN)
TEST_P(SharedMemoryTest, UnsafeImageSection) {
  const char kTestSectionName[] = "UnsafeImageSection";
  wchar_t path[MAX_PATH];
  EXPECT_GT(::GetModuleFileName(nullptr, path, base::size(path)), 0U);

  // Map the current executable image to save us creating a new PE file on disk.
  base::win::ScopedHandle file_handle(::CreateFile(
      path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
  EXPECT_TRUE(file_handle.IsValid());
  base::win::ScopedHandle section_handle(
      ::CreateFileMappingA(file_handle.Get(), nullptr,
                           PAGE_READONLY | SEC_IMAGE, 0, 0, kTestSectionName));
  EXPECT_TRUE(section_handle.IsValid());

  // Check opening from handle and duplicated from handle.
  SharedMemory shared_memory_handle_local(
      SharedMemoryHandle(section_handle.Take(), 1, UnguessableToken::Create()),
      true);
  EXPECT_FALSE(shared_memory_handle_local.Map(1));
  EXPECT_EQ(nullptr, shared_memory_handle_local.memory());

  // Check that a handle without SECTION_QUERY also can't be mapped as it can't
  // be checked.
  SharedMemory shared_memory_handle_dummy;
  SharedMemoryCreateOptions options;
  options.size = 0x1000;
  EXPECT_TRUE(shared_memory_handle_dummy.Create(options));
  HANDLE handle_no_query;
  EXPECT_TRUE(::DuplicateHandle(
      ::GetCurrentProcess(), shared_memory_handle_dummy.handle().GetHandle(),
      ::GetCurrentProcess(), &handle_no_query, FILE_MAP_READ, FALSE, 0));
  SharedMemory shared_memory_handle_no_query(
      SharedMemoryHandle(handle_no_query, options.size,
                         UnguessableToken::Create()),
      true);
  EXPECT_FALSE(shared_memory_handle_no_query.Map(1));
  EXPECT_EQ(nullptr, shared_memory_handle_no_query.memory());
}
#endif  // defined(OS_WIN)

#if !(defined(OS_MACOSX) && !defined(OS_IOS))
// The Mach functionality is tested in shared_memory_mac_unittest.cc.
TEST_P(SharedMemoryTest, MappedId) {
  const uint32_t kDataSize = 1024;
  SharedMemory memory;
  SharedMemoryCreateOptions options;
  options.size = kDataSize;

  EXPECT_TRUE(memory.Create(options));
  base::UnguessableToken id = memory.handle().GetGUID();
  EXPECT_FALSE(id.is_empty());
  EXPECT_TRUE(memory.mapped_id().is_empty());

  EXPECT_TRUE(memory.Map(kDataSize));
  EXPECT_EQ(id, memory.mapped_id());

  memory.Close();
  EXPECT_EQ(id, memory.mapped_id());

  memory.Unmap();
  EXPECT_TRUE(memory.mapped_id().is_empty());
}
#endif  // !(defined(OS_MACOSX) && !defined(OS_IOS)

INSTANTIATE_TEST_SUITE_P(Default,
                         SharedMemoryTest,
                         ::testing::Values(Mode::Default));
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
INSTANTIATE_TEST_SUITE_P(SkipDevShm,
                         SharedMemoryTest,
                         ::testing::Values(Mode::DisableDevShm));
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
TEST(SharedMemoryTest, ReadOnlyRegions) {
  const uint32_t kDataSize = 1024;
  SharedMemory memory;
  SharedMemoryCreateOptions options;
  options.size = kDataSize;
  EXPECT_TRUE(memory.Create(options));

  EXPECT_FALSE(memory.handle().IsRegionReadOnly());

  // Check that it is possible to map the region directly from the fd.
  int region_fd = memory.handle().GetHandle();
  EXPECT_GE(region_fd, 0);
  void* address = mmap(nullptr, kDataSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                       region_fd, 0);
  bool success = address && address != MAP_FAILED;
  ASSERT_TRUE(address);
  ASSERT_NE(address, MAP_FAILED);
  if (success) {
    EXPECT_EQ(0, munmap(address, kDataSize));
  }

  ASSERT_TRUE(memory.handle().SetRegionReadOnly());
  EXPECT_TRUE(memory.handle().IsRegionReadOnly());

  // Check that it is no longer possible to map the region read/write.
  errno = 0;
  address = mmap(nullptr, kDataSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                 region_fd, 0);
  success = address && address != MAP_FAILED;
  ASSERT_FALSE(success);
  ASSERT_EQ(EPERM, errno);
  if (success) {
    EXPECT_EQ(0, munmap(address, kDataSize));
  }
}

TEST(SharedMemoryTest, ReadOnlyDescriptors) {
  const uint32_t kDataSize = 1024;
  SharedMemory memory;
  SharedMemoryCreateOptions options;
  options.size = kDataSize;
  EXPECT_TRUE(memory.Create(options));

  EXPECT_FALSE(memory.handle().IsRegionReadOnly());

  // Getting a read-only descriptor should not make the region read-only itself.
  SharedMemoryHandle ro_handle = memory.GetReadOnlyHandle();
  EXPECT_FALSE(memory.handle().IsRegionReadOnly());

  // Mapping a writable region from a read-only descriptor should not
  // be possible, it will DCHECK() in debug builds (see test below),
  // while returning false on release ones.
  {
    bool dcheck_fired = false;
    logging::ScopedLogAssertHandler log_assert(
        base::BindRepeating([](bool* flag, const char*, int, base::StringPiece,
                               base::StringPiece) { *flag = true; },
                            base::Unretained(&dcheck_fired)));

    SharedMemory rw_region(ro_handle.Duplicate(), /* read_only */ false);
    EXPECT_FALSE(rw_region.Map(kDataSize));
    EXPECT_EQ(DCHECK_IS_ON() ? true : false, dcheck_fired);
  }

  // Nor shall it turn the region read-only itself.
  EXPECT_FALSE(ro_handle.IsRegionReadOnly());

  // Mapping a read-only region from a read-only descriptor should work.
  SharedMemory ro_region(ro_handle.Duplicate(), /* read_only */ true);
  EXPECT_TRUE(ro_region.Map(kDataSize));

  // And it should turn the region read-only too.
  EXPECT_TRUE(ro_handle.IsRegionReadOnly());
  EXPECT_TRUE(memory.handle().IsRegionReadOnly());
  EXPECT_FALSE(memory.Map(kDataSize));

  ro_handle.Close();
}

#endif  // OS_ANDROID

}  // namespace base
