// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/files/file.h"

// The only 32-bit platform that uses this file is Android. On Android APIs
// >= 21, this standard define is the right way to express that you want a
// 64-bit offset in struct stat, and the stat64 struct and functions aren't
// useful.
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

static_assert(sizeof(base::stat_wrapper_t::st_size) >= 8);

#include <atomic>
#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/os_compat_android.h"
#endif

namespace base {

// Make sure our Whence mappings match the system headers.
static_assert(File::FROM_BEGIN == SEEK_SET && File::FROM_CURRENT == SEEK_CUR &&
                  File::FROM_END == SEEK_END,
              "whence mapping must match the system headers");

namespace {

#if BUILDFLAG(IS_APPLE)
// When enabled, `F_FULLFSYNC` is not used in `File::Flush`. Instead,
// `F_BARRIERFSYNC` or `flush()` is used (depending on the
// "MacEfficientFileFlushUseBarrier" param). The feature exists to measure the
// cost of `F_FULLFSYNC` compared to other solutions (not ready to enable by
// default as-is). See
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html
BASE_FEATURE(kMacEfficientFileFlush,
             "MacEfficientFileFlush",
             base::FEATURE_DISABLED_BY_DEFAULT);

const FeatureParam<bool> kMacEfficientFileFlushUseBarrier{
    &kMacEfficientFileFlush, "MacEfficientFileFlushUseBarrier", true};

enum class MacFileFlushMechanism {
  kFlush,
  kFullFsync,
  kBarrierFsync,
};

std::atomic<MacFileFlushMechanism> g_mac_file_flush_mechanism{
    MacFileFlushMechanism::kFullFsync};
#endif  // BUILDFLAG(IS_APPLE)

// NaCl doesn't provide the following system calls, so either simulate them or
// wrap them in order to minimize the number of #ifdef's in this file.
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
bool IsOpenAppend(PlatformFile file) {
  return (fcntl(file, F_GETFL) & O_APPEND) != 0;
}

int CallFtruncate(PlatformFile file, int64_t length) {
#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  static_assert(sizeof(off_t) >= sizeof(int64_t),
                "off_t is not a 64-bit integer");
  return HANDLE_EINTR(ftruncate(file, length));
#else
  return HANDLE_EINTR(ftruncate64(file, length));
#endif
}

int CallFutimes(PlatformFile file, const struct timeval times[2]) {
#ifdef __USE_XOPEN2K8
  // futimens should be available, but futimes might not be
  // http://pubs.opengroup.org/onlinepubs/9699919799/

  timespec ts_times[2];
  ts_times[0].tv_sec  = times[0].tv_sec;
  ts_times[0].tv_nsec = times[0].tv_usec * 1000;
  ts_times[1].tv_sec  = times[1].tv_sec;
  ts_times[1].tv_nsec = times[1].tv_usec * 1000;

  return futimens(file, ts_times);
#else
  return futimes(file, times);
#endif
}

#if !BUILDFLAG(IS_FUCHSIA)
short FcntlFlockType(std::optional<File::LockMode> mode) {
  if (!mode.has_value())
    return F_UNLCK;
  switch (mode.value()) {
    case File::LockMode::kShared:
      return F_RDLCK;
    case File::LockMode::kExclusive:
      return F_WRLCK;
  }
  NOTREACHED();
}

File::Error CallFcntlFlock(PlatformFile file,
                           std::optional<File::LockMode> mode) {
  struct flock lock;
  lock.l_type = FcntlFlockType(std::move(mode));
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;  // Lock entire file.
  if (HANDLE_EINTR(fcntl(file, F_SETLK, &lock)) == -1)
    return File::GetLastFileError();
  return File::FILE_OK;
}
#endif

#else   // BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)

bool IsOpenAppend(PlatformFile file) {
  // NaCl doesn't implement fcntl. Since NaCl's write conforms to the POSIX
  // standard and always appends if the file is opened with O_APPEND, just
  // return false here.
  return false;
}

int CallFtruncate(PlatformFile file, int64_t length) {
  NOTIMPLEMENTED();  // NaCl doesn't implement ftruncate.
  return 0;
}

int CallFutimes(PlatformFile file, const struct timeval times[2]) {
  NOTIMPLEMENTED();  // NaCl doesn't implement futimes.
  return 0;
}

File::Error CallFcntlFlock(PlatformFile file,
                           std::optional<File::LockMode> mode) {
  NOTIMPLEMENTED();  // NaCl doesn't implement flock struct.
  return File::FILE_ERROR_INVALID_OPERATION;
}
#endif  // BUILDFLAG(IS_NACL)

}  // namespace

void File::Info::FromStat(const stat_wrapper_t& stat_info) {
  is_directory = S_ISDIR(stat_info.st_mode);
  is_symbolic_link = S_ISLNK(stat_info.st_mode);
  size = stat_info.st_size;

  // Get last modification time, last access time, and creation time from
  // |stat_info|.
  // Note: st_ctime is actually last status change time when the inode was last
  // updated, which happens on any metadata change. It is not the file's
  // creation time. However, other than on Mac & iOS where the actual file
  // creation time is included as st_birthtime, the rest of POSIX platforms have
  // no portable way to get the creation time.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  time_t last_modified_sec = stat_info.st_mtim.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtim.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atim.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atim.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctim.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctim.tv_nsec;
#elif BUILDFLAG(IS_ANDROID)
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = stat_info.st_mtime_nsec;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = stat_info.st_atime_nsec;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = stat_info.st_ctime_nsec;
#elif BUILDFLAG(IS_APPLE)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_birthtimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_birthtimespec.tv_nsec;
#elif BUILDFLAG(IS_BSD)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctimespec.tv_nsec;
#else
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = 0;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = 0;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = 0;
#endif

  last_modified =
      Time::FromTimeT(last_modified_sec) +
      Microseconds(last_modified_nsec / Time::kNanosecondsPerMicrosecond);

  last_accessed =
      Time::FromTimeT(last_accessed_sec) +
      Microseconds(last_accessed_nsec / Time::kNanosecondsPerMicrosecond);

  creation_time =
      Time::FromTimeT(creation_time_sec) +
      Microseconds(creation_time_nsec / Time::kNanosecondsPerMicrosecond);
}

bool File::IsValid() const {
  return file_.is_valid();
}

PlatformFile File::GetPlatformFile() const {
  return file_.get();
}

PlatformFile File::TakePlatformFile() {
  return file_.release();
}

void File::Close() {
  if (!IsValid())
    return;

  SCOPED_FILE_TRACE("Close");
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  file_.reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("Seek", offset);

#if BUILDFLAG(IS_ANDROID)
  static_assert(sizeof(int64_t) == sizeof(off64_t), "off64_t must be 64 bits");
  return lseek64(file_.get(), static_cast<off64_t>(offset),
                 static_cast<int>(whence));
#else
  static_assert(sizeof(int64_t) == sizeof(off_t), "off_t must be 64 bits");
  return lseek(file_.get(), static_cast<off_t>(offset),
               static_cast<int>(whence));
#endif
}

int File::Read(int64_t offset, char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0 || !IsValueInRangeForNumericType<off_t>(offset + size - 1))
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("Read", size);

  int bytes_read = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(pread(file_.get(), data + bytes_read,
                            static_cast<size_t>(size - bytes_read),
                            static_cast<off_t>(offset + bytes_read)));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(rv);
}

int File::ReadAtCurrentPos(char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPos", size);

  int bytes_read = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(read(file_.get(), data + bytes_read,
                           static_cast<size_t>(size - bytes_read)));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(rv);
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0 || !IsValueInRangeForNumericType<off_t>(offset))
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadNoBestEffort", size);
  return checked_cast<int>(
      HANDLE_EINTR(pread(file_.get(), data, static_cast<size_t>(size),
                         static_cast<off_t>(offset))));
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPosNoBestEffort", size);
  return checked_cast<int>(
      HANDLE_EINTR(read(file_.get(), data, static_cast<size_t>(size))));
}

int File::Write(int64_t offset, const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (IsOpenAppend(file_.get()))
    return WriteAtCurrentPos(data, size);

  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("Write", size);

  int bytes_written = 0;
  long rv;
  do {
#if BUILDFLAG(IS_ANDROID)
    // In case __USE_FILE_OFFSET64 is not used, we need to call pwrite64()
    // instead of pwrite().
    static_assert(sizeof(int64_t) == sizeof(off64_t),
                  "off64_t must be 64 bits");
    rv = HANDLE_EINTR(pwrite64(file_.get(), data + bytes_written,
                               static_cast<size_t>(size - bytes_written),
                               offset + bytes_written));
#else
    rv = HANDLE_EINTR(pwrite(file_.get(), data + bytes_written,
                             static_cast<size_t>(size - bytes_written),
                             offset + bytes_written));
#endif
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPos(const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPos", size);

  int bytes_written = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(write(file_.get(), data + bytes_written,
                            static_cast<size_t>(size - bytes_written)));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPosNoBestEffort", size);
  return checked_cast<int>(
      HANDLE_EINTR(write(file_.get(), data, static_cast<size_t>(size))));
}

int64_t File::GetLength() const {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetLength");

  Info info;
  if (!GetInfo(&info)) {
    return -1;
  }

  return info.size;
}

bool File::SetLength(int64_t length) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("SetLength", length);
  return !CallFtruncate(file_.get(), length);
}

bool File::SetTimes(Time last_access_time, Time last_modified_time) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("SetTimes");

  timeval times[2];
  times[0] = last_access_time.ToTimeVal();
  times[1] = last_modified_time.ToTimeVal();

  return !CallFutimes(file_.get(), times);
}

bool File::GetInfo(Info* info) const {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetInfo");

  stat_wrapper_t file_info;
  bool success = (Fstat(file_.get(), &file_info) == 0);
  if (success) {
    info->FromStat(file_info);
  }
#if BUILDFLAG(IS_ANDROID)
  if (path_.IsContentUri()) {
    // Content-URIs may represent files on the local disk, or may be virtual
    // files backed by a ContentProvider. First attempt to use fstat(fd) with a
    // FD from ContentResolver#openAssetFileDescriptor(). Some files may not
    // succeed at all, or may have size=0 in which case we will attempt to get
    // info via DocumentFile.
    return (success && info->size > 0) ||
           internal::ContentUriGetFileInfo(path_, info);
  }
#endif
  return success;
}

#if !BUILDFLAG(IS_FUCHSIA)
File::Error File::Lock(File::LockMode mode) {
  SCOPED_FILE_TRACE("Lock");
  return CallFcntlFlock(file_.get(), mode);
}

File::Error File::Unlock() {
  SCOPED_FILE_TRACE("Unlock");
  return CallFcntlFlock(file_.get(), std::optional<File::LockMode>());
}
#endif

File File::Duplicate() const {
  if (!IsValid())
    return File();

  SCOPED_FILE_TRACE("Duplicate");

  ScopedPlatformFile other_fd(HANDLE_EINTR(dup(GetPlatformFile())));
  if (!other_fd.is_valid())
    return File(File::GetLastFileError());

  return File(std::move(other_fd), async());
}

#if BUILDFLAG(IS_APPLE)
void File::InitializeFeatures() {
  if (FeatureList::IsEnabled(kMacEfficientFileFlush)) {
    // "relaxed" because there is no dependency between these memory operations
    // and other memory operations.
    if (kMacEfficientFileFlushUseBarrier.Get()) {
      g_mac_file_flush_mechanism.store(MacFileFlushMechanism::kBarrierFsync,
                                       std::memory_order_relaxed);
    } else {
      g_mac_file_flush_mechanism.store(MacFileFlushMechanism::kFlush,
                                       std::memory_order_relaxed);
    }
  }
}
#endif  // BUILDFLAG(IS_APPLE)

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return FILE_ERROR_ACCESS_DENIED;
    case EBUSY:
#if !BUILDFLAG(IS_NACL)  // ETXTBSY not defined by NaCl.
    case ETXTBSY:
#endif
      return FILE_ERROR_IN_USE;
    case EEXIST:
      return FILE_ERROR_EXISTS;
    case EIO:
      return FILE_ERROR_IO;
    case ENOENT:
      return FILE_ERROR_NOT_FOUND;
    case ENFILE:  // fallthrough
    case EMFILE:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return FILE_ERROR_NOT_A_DIRECTORY;
    default:
      // This function should only be called for errors.
      DCHECK_NE(0, saved_errno);
      return FILE_ERROR_FAILED;
  }
}

// NaCl doesn't implement system calls to open files directly.
#if !BUILDFLAG(IS_NACL)
// TODO(erikkay): does it make sense to support FLAG_EXCLUSIVE_* here?
void File::DoInitialize(const FilePath& path, uint32_t flags) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(!IsValid());

  int open_flags = 0;
  if (flags & FLAG_CREATE)
    open_flags = O_CREAT | O_EXCL;

  created_ = false;

  if (flags & FLAG_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    DCHECK(flags & FLAG_WRITE);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (flags & FLAG_OPEN_TRUNCATED) {
    DCHECK(!open_flags);
    DCHECK(flags & FLAG_WRITE);
    open_flags = O_TRUNC;
  }

  if (!open_flags && !(flags & FLAG_OPEN) && !(flags & FLAG_OPEN_ALWAYS)) {
    NOTREACHED();
  }

  if (flags & FLAG_WRITE && flags & FLAG_READ) {
    open_flags |= O_RDWR;
  } else if (flags & FLAG_WRITE) {
    open_flags |= O_WRONLY;
  } else if (!(flags & FLAG_READ) && !(flags & FLAG_WRITE_ATTRIBUTES) &&
             !(flags & FLAG_APPEND) && !(flags & FLAG_OPEN_ALWAYS)) {
    // Note: For FLAG_WRITE_ATTRIBUTES and no other read/write flags, we'll
    // open the file in O_RDONLY mode (== 0, see static_assert below), so that
    // we get a fd that can be used for SetTimes().
    NOTREACHED();
  }

  if (flags & FLAG_TERMINAL_DEVICE)
    open_flags |= O_NOCTTY | O_NDELAY;

  if (flags & FLAG_APPEND && flags & FLAG_READ)
    open_flags |= O_APPEND | O_RDWR;
  else if (flags & FLAG_APPEND)
    open_flags |= O_APPEND | O_WRONLY;

  static_assert(O_RDONLY == 0, "O_RDONLY must equal zero");

  mode_t mode = S_IRUSR | S_IWUSR;
#if BUILDFLAG(IS_CHROMEOS)
  mode |= S_IRGRP | S_IROTH;
#endif

#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri()) {
    int fd = internal::OpenContentUri(path, flags);
    if (fd < 0) {
      error_details_ = FILE_ERROR_FAILED;
      return;
    }

    // Save path for any call to GetInfo().
    path_ = path;
    created_ = (flags & (FLAG_CREATE_ALWAYS | FLAG_CREATE));
    async_ = (flags & FLAG_ASYNC);
    error_details_ = FILE_OK;
    file_.reset(fd);
    return;
  }
#endif

  int descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));

  if (flags & FLAG_OPEN_ALWAYS) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));
      if (descriptor >= 0)
        created_ = true;
    }
  }

  if (descriptor < 0) {
    error_details_ = File::GetLastFileError();
    return;
  }

  if (flags & (FLAG_CREATE_ALWAYS | FLAG_CREATE))
    created_ = true;

  if (flags & FLAG_DELETE_ON_CLOSE)
    unlink(path.value().c_str());

  async_ = ((flags & FLAG_ASYNC) == FLAG_ASYNC);
  error_details_ = FILE_OK;
  file_.reset(descriptor);
}
#endif  // !BUILDFLAG(IS_NACL)

bool File::Flush() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  SCOPED_FILE_TRACE("Flush");

#if BUILDFLAG(IS_NACL)
  NOTIMPLEMENTED();  // NaCl doesn't implement fsync.
  return true;
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
  return !HANDLE_EINTR(fdatasync(file_.get()));
#elif BUILDFLAG(IS_APPLE)
  // On macOS and iOS, fsync() is guaranteed to send the file's data to the
  // underlying storage device, but may return before the device actually writes
  // the data to the medium. When used by database systems, this may result in
  // unexpected data loss. Depending on experiment state, this function may use
  // F_BARRIERFSYNC or F_FULLFSYNC to provide stronger guarantees than fsync().
  //
  // See documentation:
  // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html
  //
  // "relaxed" because there is no dependency between this memory operation and
  // other memory operations.
  switch (g_mac_file_flush_mechanism.load(std::memory_order_relaxed)) {
    case MacFileFlushMechanism::kBarrierFsync: {
      if (!HANDLE_EINTR(fcntl(file_.get(), F_BARRIERFSYNC))) {
        return true;
      }
      // Fall back to `fsync()` in case of failure.
      break;
    }
    case MacFileFlushMechanism::kFullFsync: {
      if (!HANDLE_EINTR(fcntl(file_.get(), F_FULLFSYNC))) {
        return true;
      }
      // Fall back to `fsync()` in case of failure.
      break;
    }
    case MacFileFlushMechanism::kFlush: {
      // Fall back to `fsync()`.
      break;
    }
  }

  // `fsync()` if `F_BARRIERFSYNC` or `F_FULLFSYNC` failed, or if the mechanism
  // is `kFlush`. Some file systems do not support `F_FULLFSYNC` /
  // `F_BARRIERFSYNC` but we cannot use the error code as a definitive indicator
  // that it's the case, so we'll keep trying `F_FULLFSYNC` / `F_BARRIERFSYNC`
  // for every call to this method when it's the case. See the CL description at
  // https://crrev.com/c/1400159 for details.
  return !HANDLE_EINTR(fsync(file_.get()));
#else
  return !HANDLE_EINTR(fsync(file_.get()));
#endif
}

void File::SetPlatformFile(PlatformFile file) {
  DCHECK(!file_.is_valid());
  file_.reset(file);
}

// static
File::Error File::GetLastFileError() {
  return base::File::OSErrorToFileError(errno);
}

int File::Stat(const FilePath& path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri()) {
    // Attempt to open the file and call GetInfo(), otherwise call Java code
    // with the path which is required for dirs.
    File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    Info info;
    if ((file.IsValid() && file.GetInfo(&info)) ||
        internal::ContentUriGetFileInfo(path, &info)) {
      memset(sb, 0, sizeof(*sb));
      sb->st_mode = info.is_directory ? S_IFDIR : S_IFREG;
      sb->st_size = info.size;
      sb->st_mtime = info.last_modified.ToTimeT();
      sb->st_mtime_nsec =
          (info.last_modified - Time::UnixEpoch()).InNanoseconds() %
          Time::kNanosecondsPerSecond;
      return 0;
    }
  }
#endif
  return stat(path.value().c_str(), sb);
}
int File::Fstat(int fd, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return fstat(fd, sb);
}
int File::Lstat(const FilePath& path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return lstat(path.value().c_str(), sb);
}

}  // namespace base
