// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_helper.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#include "third_party/ashmem/ashmem.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#error "MacOS uses shared_memory_mac.cc"
#endif

namespace base {

SharedMemory::SharedMemory() = default;

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : shm_(handle), read_only_(read_only) {}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  DCHECK(handle.IsValid());
  handle.Close();
}

// static
size_t SharedMemory::GetHandleLimit() {
  return GetMaxFds();
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

// static
int SharedMemory::GetFdFromSharedMemoryHandle(
    const SharedMemoryHandle& handle) {
  return handle.GetHandle();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

#if !defined(OS_ANDROID)

// Chromium mostly only uses the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
// TODO(jrg): there is no way to "clean up" all unused named shmem if
// we restart from a crash.  (That isn't a new problem, but it is a problem.)
// In case we want to delete it later, it may be useful to save the value
// of mem_filename after FilePathForMemoryName().
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!shm_.IsValid());
  if (options.size == 0) return false;

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  ThreadRestrictions::ScopedAllowIO allow_io;

  bool fix_size = true;
  ScopedFD fd;
  ScopedFD readonly_fd;
  FilePath path;
  if (!options.name_deprecated || options.name_deprecated->empty()) {
    bool result =
        CreateAnonymousSharedMemory(options, &fd, &readonly_fd, &path);
    if (!result)
      return false;
  } else {
    if (!FilePathForMemoryName(*options.name_deprecated, &path))
      return false;

    // Make sure that the file is opened without any permission
    // to other users on the system.
    const mode_t kOwnerOnly = S_IRUSR | S_IWUSR;

    // First, try to create the file.
    fd.reset(HANDLE_EINTR(
        open(path.value().c_str(), O_RDWR | O_CREAT | O_EXCL, kOwnerOnly)));
    if (!fd.is_valid() && options.open_existing_deprecated) {
      // If this doesn't work, try and open an existing file in append mode.
      // Opening an existing file in a world writable directory has two main
      // security implications:
      // - Attackers could plant a file under their control, so ownership of
      //   the file is checked below.
      // - Attackers could plant a symbolic link so that an unexpected file
      //   is opened, so O_NOFOLLOW is passed to open().
#if !defined(OS_AIX)
      fd.reset(HANDLE_EINTR(
          open(path.value().c_str(), O_RDWR | O_APPEND | O_NOFOLLOW)));
#else
      // AIX has no 64-bit support for open flags such as -
      //  O_CLOEXEC, O_NOFOLLOW and O_TTY_INIT.
      fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDWR | O_APPEND)));
#endif
      // Check that the current user owns the file.
      // If uid != euid, then a more complex permission model is used and this
      // API is not appropriate.
      const uid_t real_uid = getuid();
      const uid_t effective_uid = geteuid();
      struct stat sb;
      if (fd.is_valid() &&
          (fstat(fd.get(), &sb) != 0 || sb.st_uid != real_uid ||
           sb.st_uid != effective_uid)) {
        LOG(ERROR) <<
            "Invalid owner when opening existing shared memory file.";
        close(fd.get());
        return false;
      }

      // An existing file was opened, so its size should not be fixed.
      fix_size = false;
    }

    if (options.share_read_only) {
      // Also open as readonly so that we can GetReadOnlyHandle.
      readonly_fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
      if (!readonly_fd.is_valid()) {
        DPLOG(ERROR) << "open(\"" << path.value() << "\", O_RDONLY) failed";
        close(fd.get());
        return false;
      }
    }
  }
  if (fd.is_valid() && fix_size) {
    // Get current size.
    struct stat stat;
    if (fstat(fd.get(), &stat) != 0)
      return false;
    const size_t current_size = stat.st_size;
    if (current_size != options.size) {
      if (HANDLE_EINTR(ftruncate(fd.get(), options.size)) != 0)
        return false;
    }
    requested_size_ = options.size;
  }
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    FilePath dir = path.DirName();
    if (access(dir.value().c_str(), W_OK | X_OK) < 0) {
      PLOG(ERROR) << "Unable to access(W_OK|X_OK) " << dir.value();
      if (dir.value() == "/dev/shm") {
        LOG(FATAL) << "This is frequently caused by incorrect permissions on "
                   << "/dev/shm.  Try 'sudo chmod 1777 /dev/shm' to fix.";
      }
    }
    return false;
  }

  int mapped_file = -1;
  int readonly_mapped_file = -1;

  bool result = PrepareMapFile(std::move(fd), std::move(readonly_fd),
                               &mapped_file, &readonly_mapped_file);
  shm_ = SharedMemoryHandle(FileDescriptor(mapped_file, false), options.size,
                            UnguessableToken::Create());
  readonly_shm_ =
      SharedMemoryHandle(FileDescriptor(readonly_mapped_file, false),
                         options.size, shm_.GetGUID());
  return result;
}

// Our current implementation of shmem is with mmap()ing of files.
// These files need to be deleted explicitly.
// In practice this call is only needed for unit tests.
bool SharedMemory::Delete(const std::string& name) {
  FilePath path;
  if (!FilePathForMemoryName(name, &path))
    return false;

  if (PathExists(path))
    return DeleteFile(path, false);

  // Doesn't exist, so success.
  return true;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  FilePath path;
  if (!FilePathForMemoryName(name, &path))
    return false;

  read_only_ = read_only;

  int mode = read_only ? O_RDONLY : O_RDWR;
  ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), mode)));
  ScopedFD readonly_fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!readonly_fd.is_valid()) {
    DPLOG(ERROR) << "open(\"" << path.value() << "\", O_RDONLY) failed";
    return false;
  }
  int mapped_file = -1;
  int readonly_mapped_file = -1;
  bool result = PrepareMapFile(std::move(fd), std::move(readonly_fd),
                               &mapped_file, &readonly_mapped_file);
  // This form of sharing shared memory is deprecated. https://crbug.com/345734.
  // However, we can't get rid of it without a significant refactor because its
  // used to communicate between two versions of the same service process, very
  // early in the life cycle.
  // Technically, we should also pass the GUID from the original shared memory
  // region. We don't do that - this means that we will overcount this memory,
  // which thankfully isn't relevant since Chrome only communicates with a
  // single version of the service process.
  // We pass the size |0|, which is a dummy size and wrong, but otherwise
  // harmless.
  shm_ = SharedMemoryHandle(FileDescriptor(mapped_file, false), 0u,
                            UnguessableToken::Create());
  readonly_shm_ = SharedMemoryHandle(
      FileDescriptor(readonly_mapped_file, false), 0, shm_.GetGUID());
  return result;
}
#endif  // !defined(OS_ANDROID)

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid())
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

#if defined(OS_ANDROID)
  // On Android, Map can be called with a size and offset of zero to use the
  // ashmem-determined size.
  if (bytes == 0) {
    DCHECK_EQ(0, offset);
    int ashmem_bytes = ashmem_get_size_region(shm_.GetHandle());
    if (ashmem_bytes < 0)
      return false;
    bytes = ashmem_bytes;
  }

  // Sanity check. This shall catch invalid uses of the SharedMemory APIs
  // but will not protect against direct mmap() attempts.
  if (shm_.IsReadOnly()) {
    // Use a DCHECK() to call writable mappings with read-only descriptors
    // in debug builds immediately. Return an error for release builds
    // or during unit-testing (assuming a ScopedLogAssertHandler was installed).
    DCHECK(read_only_)
        << "Trying to map a region writable with a read-only descriptor.";
    if (!read_only_) {
      return false;
    }
    if (!shm_.SetRegionReadOnly()) {  // Ensure the region is read-only.
      return false;
    }
  }
#endif

  memory_ = mmap(nullptr, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, shm_.GetHandle(), offset);

  bool mmap_succeeded = memory_ && memory_ != reinterpret_cast<void*>(-1);
  if (mmap_succeeded) {
    mapped_size_ = bytes;
    mapped_id_ = shm_.GetGUID();
    DCHECK_EQ(0U,
              reinterpret_cast<uintptr_t>(memory_) &
                  (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  } else {
    memory_ = nullptr;
  }

  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
  munmap(memory_, mapped_size_);
  memory_ = nullptr;
  mapped_size_ = 0;
  mapped_id_ = UnguessableToken();
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle_copy = shm_;
  handle_copy.SetOwnershipPassesToIPC(true);
  Unmap();
  shm_ = SharedMemoryHandle();
  return handle_copy;
}

#if !defined(OS_ANDROID)
void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
  if (readonly_shm_.IsValid()) {
    readonly_shm_.Close();
    readonly_shm_ = SharedMemoryHandle();
  }
}

// For the given shmem named |mem_name|, return a filename to mmap()
// (and possibly create).  Modifies |filename|.  Return false on
// error, or true of we are happy.
bool SharedMemory::FilePathForMemoryName(const std::string& mem_name,
                                         FilePath* path) {
  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK_EQ(std::string::npos, mem_name.find('/'));
  DCHECK_EQ(std::string::npos, mem_name.find('\0'));

  FilePath temp_dir;
  if (!GetShmemTempDir(false, &temp_dir))
    return false;

#if defined(GOOGLE_CHROME_BUILD)
  static const char kShmem[] = "com.google.Chrome.shmem.";
#else
  static const char kShmem[] = "org.chromium.Chromium.shmem.";
#endif
  *path = temp_dir.AppendASCII(kShmem + mem_name);
  return true;
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  CHECK(readonly_shm_.IsValid());
  return readonly_shm_.Duplicate();
}
#endif  // !defined(OS_ANDROID)

}  // namespace base
