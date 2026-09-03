// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <atomic>
#include <limits>
#include <optional>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/build_config.h"

namespace base::subtle {

namespace {

struct ScopedPathUnlinkerTraits {
  static const FilePath* InvalidValue() { return nullptr; }

  static void Free(const FilePath* path) {
    if (unlink(path->value().c_str())) {
      PLOG(WARNING) << "unlink";
    }
  }
};

// Unlinks the FilePath when the object is destroyed.
using ScopedPathUnlinker =
    ScopedGeneric<const FilePath*, ScopedPathUnlinkerTraits>;

enum class FDAccessModeError {
  kFcntlFailed,
  kMismatch,
};

std::optional<FDAccessModeError> CheckFDAccessMode(int fd, int expected_mode) {
  int fd_status = fcntl(fd, F_GETFL);
  if (fd_status == -1) {
    // TODO(crbug.com/40574272): convert to DLOG when bug fixed.
    PLOG(ERROR) << "fcntl(" << fd << ", F_GETFL) failed";
    return FDAccessModeError::kFcntlFailed;
  }

  int mode = fd_status & O_ACCMODE;
  if (mode != expected_mode) {
    return FDAccessModeError::kMismatch;
  }

  return std::nullopt;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Added in Linux 6.3; not in all sysroot headers yet.
#ifndef MFD_NOEXEC_SEAL
#define MFD_NOEXEC_SEAL 0x0008U
#endif

// Creates the region as an anonymous memfd instead of an unlinked file in
// /dev/shm. This needs two to three system calls instead of seven, no path
// lookup and no dentry, and no up-front fallocate(): memfd pages live on the
// kernel-internal shmem mount, which has no size limit that could turn a later
// page fault into SIGBUS the way a full /dev/shm does, so there is nothing to
// reserve against (allocation failure under memory pressure is an OOM
// condition, exactly as for anonymous memory). It also stops depending on
// /dev/shm being mounted, accessible and large enough - container runtimes
// commonly provide a 64 MiB /dev/shm, which is what --disable-dev-shm-usage
// exists to work around.
//
// Returns an invalid pair when memfd_create() is unavailable (kernels before
// 3.17, or a seccomp policy that rejects it) so that the caller falls back to
// the file-based implementation.
ScopedFDPair CreateAnonymousRegion(PlatformSharedMemoryRegion::Mode mode,
                                   size_t size) {
  // MFD_NOEXEC_SEAL creates the file non-executable and seals that state.
  // Kernels older than 6.3 reject the flag with EINVAL; remember that.
  // The raw system call is used (as in mojo/core/channel_linux.cc) so that
  // this does not add a glibc 2.27 requirement.
  static constexpr char kName[] = "shared-memory-region";
  static std::atomic<bool> try_noexec_seal{true};
  ScopedFD fd;
  if (try_noexec_seal.load(std::memory_order_relaxed)) {
    fd.reset(static_cast<int>(
        syscall(__NR_memfd_create, kName,
                MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_NOEXEC_SEAL)));
    if (!fd.is_valid() && errno == EINVAL) {
      try_noexec_seal.store(false, std::memory_order_relaxed);
    }
  }
  if (!fd.is_valid()) {
    fd.reset(static_cast<int>(
        syscall(__NR_memfd_create, kName, MFD_CLOEXEC | MFD_ALLOW_SEALING)));
  }
  if (!fd.is_valid()) {
    DPLOG_IF(ERROR, errno != ENOSYS) << "memfd_create";
    return {};
  }

  if (HANDLE_EINTR(ftruncate(fd.get(), checked_cast<off_t>(size))) != 0) {
    DPLOG(ERROR) << "ftruncate";
    return {};
  }

  // The size of a region never changes after creation. Sealing it means that
  // no holder of a descriptor for this region - including less trusted
  // processes it is later shared with - can shrink the file and make other
  // processes fault on access, or grow it. F_SEAL_SEAL keeps anyone from
  // adding write seals later, which would make future mappings fail.
  if (fcntl(fd.get(), F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) !=
      0) {
    DPLOG(ERROR) << "fcntl(F_ADD_SEALS)";
    return {};
  }

  ScopedFD readonly_fd;
  if (mode == PlatformSharedMemoryRegion::Mode::kWritable) {
    // A writable region must be convertible to read-only later, which here
    // means owning a second, O_RDONLY open file description for the same
    // file (see ConvertToReadOnly() and
    // CheckPlatformHandlePermissionsCorrespondToMode()). A memfd has no name,
    // but procfs can reopen it. This is the only step that needs filesystem
    // access; processes without it get their writable regions from a broker.
    const std::string path =
        StrCat({"/proc/self/fd/", NumberToString(fd.get())});
    readonly_fd.reset(HANDLE_EINTR(open(path.c_str(), O_RDONLY | O_CLOEXEC)));
    if (!readonly_fd.is_valid()) {
      DPLOG(ERROR) << "open(" << path << ", O_RDONLY)";
      return {};
    }
  }

  return ScopedFDPair(std::move(fd), std::move(readonly_fd));
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

// static
expected<PlatformSharedMemoryRegion, PlatformSharedMemoryRegion::TakeError>
PlatformSharedMemoryRegion::TakeOrFail(ScopedFDPair handle,
                                       Mode mode,
                                       size_t size,
                                       const UnguessableToken& guid) {
  if (!handle.fd.is_valid()) {
    return {};
  }

  if (size == 0) {
    return {};
  }

  if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }

  expected<void, TakeError> result =
      CheckPlatformHandlePermissionsCorrespondToMode(handle.get(), mode, size);
  if (!result.has_value()) {
    return unexpected(result.error());
  }

  switch (mode) {
    case Mode::kReadOnly:
    case Mode::kUnsafe:
      // TODO(dcheng): This may not be reachable given the above.
      if (handle.readonly_fd.is_valid()) {
        handle.readonly_fd.reset();
        DLOG(WARNING) << "Readonly handle shouldn't be valid for a "
                         "non-writable memory region; closing";
      }
      break;
    case Mode::kWritable:
      // TODO(dcheng): This may not be reachable given the above.
      if (!handle.readonly_fd.is_valid()) {
        DLOG(ERROR)
            << "Readonly handle must be valid for writable memory region";
        return {};
      }
      break;
  }

  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid);
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    ScopedFD handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  CHECK_NE(mode, Mode::kWritable);
  return Take(ScopedFDPair(std::move(handle), ScopedFD()), mode, size, guid);
}

FDPair PlatformSharedMemoryRegion::GetPlatformHandle() const {
  return handle_.get();
}

bool PlatformSharedMemoryRegion::IsValid() const {
  return handle_.fd.is_valid() &&
         (mode_ == Mode::kWritable ? handle_.readonly_fd.is_valid() : true);
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  if (!IsValid()) {
    return {};
  }

  CHECK_NE(mode_, Mode::kWritable)
      << "Duplicating a writable shared memory region is prohibited";

  ScopedFD duped_fd(HANDLE_EINTR(dup(handle_.fd.get())));
  if (!duped_fd.is_valid()) {
    DPLOG(ERROR) << "dup(" << handle_.fd.get() << ") failed";
    return {};
  }

  return PlatformSharedMemoryRegion({std::move(duped_fd), ScopedFD()}, mode_,
                                    size_, guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid()) {
    return false;
  }

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  handle_.fd.reset(handle_.readonly_fd.release());
  mode_ = Mode::kReadOnly;
  return true;
}

bool PlatformSharedMemoryRegion::ConvertToUnsafe() {
  if (!IsValid()) {
    return false;
  }

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to unsafe";

  handle_.readonly_fd.reset();
  mode_ = Mode::kUnsafe;
  return true;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::CreateUnsafeAnonymous(
    size_t size) {
  if (size == 0 ||
      size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }
  ScopedFDPair anonymous_region = CreateAnonymousRegion(Mode::kUnsafe, size);
  if (!anonymous_region.fd.is_valid()) {
    return {};
  }
  return PlatformSharedMemoryRegion(std::move(anonymous_region), Mode::kUnsafe,
                                    size, UnguessableToken::Create());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
                                                              ,
                                                              bool executable
#endif
) {
  if (size == 0) {
    return {};
  }

  if (size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (!executable) {
    ScopedFDPair anonymous_region = CreateAnonymousRegion(mode, size);
    if (anonymous_region.fd.is_valid()) {
      return PlatformSharedMemoryRegion(std::move(anonymous_region), mode, size,
                                        UnguessableToken::Create());
    }
    // memfd_create() is not available; fall back to a file in /dev/shm.
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  ScopedAllowBlocking scoped_allow_blocking;

  // We don't use shm_open() API in order to support the --disable-dev-shm-usage
  // flag.
  FilePath directory;
  if (!GetShmemTempDir(
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
          executable,
#else
          false /* executable */,
#endif
          &directory)) {
    return {};
  }

  FilePath path;
  ScopedFD fd = CreateAndOpenFdForTemporaryFileInDir(directory,
                                                     /*name_prefix=*/{}, &path);
  File shm_file(fd.release());

  if (!shm_file.IsValid()) {
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    FilePath dir = path.DirName();
    if (access(dir.value().c_str(), W_OK | X_OK) < 0) {
      PLOG(ERROR) << "Unable to access(W_OK|X_OK) " << dir.value();
      if (dir.value() == "/dev/shm") {
        LOG(FATAL) << "This is frequently caused by incorrect permissions on "
                   << "/dev/shm.  Try 'sudo chmod 1777 /dev/shm' to fix.";
      }
    }
    return {};
  }

  // Deleting the file prevents anyone else from mapping it in (making it
  // private), and prevents the need for cleanup (once the last fd is
  // closed, it is truly freed).
  ScopedPathUnlinker path_unlinker(&path);

  ScopedFD readonly_fd;
  if (mode == Mode::kWritable) {
    // Also open as readonly so that we can ConvertToReadOnly().
    readonly_fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
    if (!readonly_fd.is_valid()) {
      DPLOG(ERROR) << "open(\"" << path.value() << "\", O_RDONLY) failed";
      return {};
    }
  }

  if (!AllocateFileRegion(&shm_file, 0, size)) {
    return {};
  }

  if (readonly_fd.is_valid()) {
    stat_wrapper_t shm_stat;
    if (File::Fstat(shm_file.GetPlatformFile(), &shm_stat) != 0) {
      DPLOG(ERROR) << "fstat(fd) failed";
      return {};
    }

    stat_wrapper_t readonly_stat;
    if (File::Fstat(readonly_fd.get(), &readonly_stat) != 0) {
      DPLOG(ERROR) << "fstat(readonly_fd) failed";
      return {};
    }

    if (shm_stat.st_dev != readonly_stat.st_dev ||
        shm_stat.st_ino != readonly_stat.st_ino) {
      LOG(ERROR) << "Writable and read-only inodes don't match; bailing";
      return {};
    }
  }

  return PlatformSharedMemoryRegion(
      {ScopedFD(shm_file.TakePlatformFile()), std::move(readonly_fd)}, mode,
      size, UnguessableToken::Create());
}

expected<void, PlatformSharedMemoryRegion::TakeError>
PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformSharedMemoryHandle handle,
    Mode mode,
    size_t size) {
  if (auto result = CheckFDAccessMode(
          handle.fd, mode == Mode::kReadOnly ? O_RDONLY : O_RDWR);
      result.has_value()) {
    switch (*result) {
      case FDAccessModeError::kFcntlFailed:
        return unexpected(TakeError::kFcntlFailed);
      case FDAccessModeError::kMismatch:
        return unexpected(mode == Mode::kReadOnly
                              ? TakeError::kExpectedReadOnlyButNot
                              : TakeError::kExpectedWritableButNot);
    }
  }

  if (mode == Mode::kWritable) {
    if (auto result = CheckFDAccessMode(handle.readonly_fd, O_RDONLY);
        result.has_value()) {
      switch (*result) {
        case FDAccessModeError::kFcntlFailed:
          return unexpected(TakeError::kFcntlFailed);
        case FDAccessModeError::kMismatch:
          return unexpected(TakeError::kReadOnlyFdNotReadOnly);
      }
    }
    return ok();
  }

  // The second descriptor must be invalid in kReadOnly and kUnsafe modes.
  if (handle.readonly_fd != -1) {
    return unexpected(TakeError::kUnexpectedReadOnlyFd);
  }

  return ok();
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    ScopedFDPair handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace base::subtle
