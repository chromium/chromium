// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/android/virtual_document_path.h"
#include "base/files/file_android.h"
#include "base/files/file_util.h"
#include "base/os_compat_android.h"
#endif

#if BUILDFLAG(IS_AIX)
#include "base/notimplemented.h"
#endif

namespace base {

// Make sure our Whence mappings match the system headers.
static_assert(File::FROM_BEGIN == SEEK_SET && File::FROM_CURRENT == SEEK_CUR &&
                  File::FROM_END == SEEK_END,
              "whence mapping must match the system headers");

namespace {

#if BUILDFLAG(IS_ANDROID)
#define OffsetType off64_t
// In case __USE_FILE_OFFSET64 is not used, the `File` methods in this file need
// to call lseek64(), pread64() and pwrite64() instead of lseek(), pread() and
// pwrite();
#define LSeekFunc lseek64
#define PReadFunc pread64
#define PWriteFunc pwrite64
#else
#define OffsetType off_t
#define LSeekFunc lseek
#define PReadFunc pread
#define PWriteFunc pwrite
#endif

static_assert(sizeof(int64_t) == sizeof(OffsetType));

bool IsReadWriteRangeValid(int64_t offset, int size) {
  if (size < 0 || !CheckAdd(offset, size - 1).IsValid() ||
      !IsValueInRangeForNumericType<OffsetType>(offset + size - 1)) {
    return false;
  }

  return true;
}

// AIX doesn't provide the following system calls, so either simulate them or
// wrap them in order to minimize the number of #ifdef's in this file.
#if !BUILDFLAG(IS_AIX)
bool IsOpenAppend(PlatformFile file) {
  return (fcntl(file, F_GETFL) & O_APPEND) != 0;
}

int CallFtruncate(PlatformFile file, int64_t length) {
#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  return HANDLE_EINTR(ftruncate(file, length));
#else
  return HANDLE_EINTR(ftruncate64(file, length));
#endif
}

int CallFutimes(PlatformFile file, const std::array<struct timeval, 2> times) {
#ifdef __USE_XOPEN2K8
  // futimens should be available, but futimes might not be
  // http://pubs.opengroup.org/onlinepubs/9699919799/

  std::array<timespec, 2> ts_times;
  ts_times[0].tv_sec = times[0].tv_sec;
  ts_times[0].tv_nsec = times[0].tv_usec * 1000;
  ts_times[1].tv_sec = times[1].tv_sec;
  ts_times[1].tv_nsec = times[1].tv_usec * 1000;

  return futimens(file, ts_times.data());
#else
#pragma clang diagnostic push  // Can be removed once Cronet's min-sdk is >= 26.
#pragma clang diagnostic ignored "-Wunguarded-availability"
  return futimes(file, times.data());
#pragma clang diagnostic pop
#endif
}

#if !BUILDFLAG(IS_FUCHSIA)
short FcntlFlockType(std::optional<File::LockMode> mode) {
  if (!mode.has_value()) {
    return F_UNLCK;
  }
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
  if (HANDLE_EINTR(fcntl(file, F_SETLK, &lock)) == -1) {
    return File::GetLastFileError();
  }
  return File::FILE_OK;
}
#endif

#else   // !BUILDFLAG(IS_AIX)

bool IsOpenAppend(PlatformFile file) {
  // AIX doesn't implement fcntl. Since AIX's write conforms to the POSIX
  // standard and always appends if the file is opened with O_APPEND, just
  // return false here.
  return false;
}

int CallFtruncate(PlatformFile file, int64_t length) {
  NOTIMPLEMENTED();  // AIX doesn't implement ftruncate.
  return 0;
}

int CallFutimes(PlatformFile file, const struct timeval times[2]) {
  NOTIMPLEMENTED();  // AIX doesn't implement futimes.
  return 0;
}

File::Error CallFcntlFlock(PlatformFile file,
                           std::optional<File::LockMode> mode) {
  NOTIMPLEMENTED();  // AIX doesn't implement flock struct.
  return File::FILE_ERROR_INVALID_OPERATION;
}
#endif  // BUILDFLAG(IS_AIX)

#if BUILDFLAG(IS_ANDROID)
bool GetContentUriInfo(const base::FilePath& path, File::Info* info) {
  FileEnumerator::FileInfo file_info;
  bool result = internal::ContentUriGetFileInfo(path, &file_info);
  if (result) {
    info->FromStat(file_info.stat());
  }
  return result;
}
#endif

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
  if (!IsValid()) {
    return;
  }

  SCOPED_FILE_TRACE("Close");
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
#if BUILDFLAG(IS_ANDROID)
  if (java_parcel_file_descriptor_) {
    internal::ContentUriClose(java_parcel_file_descriptor_);
  }
#endif
  file_.reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("Seek", offset);
  return LSeekFunc(file_.get(), static_cast<OffsetType>(offset),
                   static_cast<int>(whence));
}

int File::Read(int64_t offset, char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (!IsReadWriteRangeValid(offset, size)) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("Read", size);

  int bytes_read = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(PReadFunc(file_.get(), data + bytes_read,
                                static_cast<size_t>(size - bytes_read),
                                static_cast<OffsetType>(offset + bytes_read)));
    if (rv <= 0) {
      break;
    }

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(rv);
}

int File::ReadAtCurrentPos(char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPos", size);

  int bytes_read = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(read(file_.get(), data + bytes_read,
                           static_cast<size_t>(size - bytes_read)));
    if (rv <= 0) {
      break;
    }

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(rv);
}

std::optional<size_t> File::ReadNoBestEffort(int64_t offset,
                                             base::span<uint8_t> data) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (!IsValueInRangeForNumericType<off_t>(offset)) {
    return std::nullopt;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("ReadNoBestEffort",
                              base::checked_cast<int64_t>(data.size()));
  const ssize_t bytes_read = HANDLE_EINTR(
      pread(file_.get(), data.data(), data.size(), static_cast<off_t>(offset)));
  if (bytes_read < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(bytes_read);
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPosNoBestEffort", size);
  return checked_cast<int>(
      HANDLE_EINTR(read(file_.get(), data, static_cast<size_t>(size))));
}

int File::Write(int64_t offset, const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (IsOpenAppend(file_.get())) {
    return WriteAtCurrentPos(data, size);
  }

  DCHECK(IsValid());
  if (!IsReadWriteRangeValid(offset, size)) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("Write", size);

  int bytes_written = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(
        PWriteFunc(file_.get(), data + bytes_written,
                   static_cast<size_t>(size - bytes_written),
                   static_cast<OffsetType>(offset + bytes_written)));
    if (rv <= 0) {
      break;
    }

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPos(const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPos", size);

  int bytes_written = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(write(file_.get(), data + bytes_written,
                            static_cast<size_t>(size - bytes_written)));
    if (rv <= 0) {
      break;
    }

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

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

  std::array<timeval, 2> times;
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
    // files backed by a ContentProvider which may or may not use FUSE to back
    // the FDs.
    //
    // For Document URIs, always use ContentUriGetFileInfo() since it will
    // succeed by using the Java API DocumentFile, which can provide
    // last-modified where FUSE cannot. FUSE always returns the current-time
    // which is problematic because Blobs are registered with an
    // expected-last-modified, and will fail if it changes by the time a client
    // accesses it.
    //
    // For other Content-URIS, if fstat() succeeded with a non-zero size, then
    // use the result, otherwise try via the Java APIs.
    return (success && info->size > 0 && !internal::IsDocumentUri(path_)) ||
           GetContentUriInfo(path_, info);
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
  if (!IsValid()) {
    return File();
  }

  SCOPED_FILE_TRACE("Duplicate");

  ScopedPlatformFile other_fd(HANDLE_EINTR(dup(GetPlatformFile())));
  if (!other_fd.is_valid()) {
    return File(File::GetLastFileError());
  }

  return File(std::move(other_fd), async());
}

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return FILE_ERROR_ACCESS_DENIED;
    case EBUSY:
    case ETXTBSY:
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

// TODO(erikkay): does it make sense to support FLAG_EXCLUSIVE_* here?
void File::DoInitialize(const FilePath& path, uint32_t flags) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(!IsValid());

  int open_flags = 0;
  if (flags & FLAG_CREATE) {
    open_flags = O_CREAT | O_EXCL;
  }

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

  if (flags & FLAG_TERMINAL_DEVICE) {
    open_flags |= O_NOCTTY | O_NDELAY;
  }

  if (flags & FLAG_APPEND && flags & FLAG_READ) {
    open_flags |= O_APPEND | O_RDWR;
  } else if (flags & FLAG_APPEND) {
    open_flags |= O_APPEND | O_WRONLY;
  }

  static_assert(O_RDONLY == 0, "O_RDONLY must equal zero");

  mode_t mode = S_IRUSR | S_IWUSR;
#if BUILDFLAG(IS_CHROMEOS)
  mode |= S_IRGRP | S_IROTH;
#endif

#if BUILDFLAG(IS_ANDROID)
  if (path.IsContentUri() || path.IsVirtualDocumentPath()) {
    auto result = files_internal::OpenAndroidFile(path, flags);
    if (!result.has_value()) {
      error_details_ = result.error();
      return;
    }

    // Save path for any call to GetInfo().
    path_ = result->content_uri;
    file_.reset(result->fd);
    java_parcel_file_descriptor_ = result->java_parcel_file_descriptor;
    created_ = result->created;
    async_ = (flags & FLAG_ASYNC);
    error_details_ = FILE_OK;
    return;
  }
#endif

  int descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));

  if (flags & FLAG_OPEN_ALWAYS) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));
      if (descriptor >= 0) {
        created_ = true;
      }
    }
  }

  if (descriptor < 0) {
    error_details_ = File::GetLastFileError();
    return;
  }

  if (flags & (FLAG_CREATE_ALWAYS | FLAG_CREATE)) {
    created_ = true;
  }

  if (flags & FLAG_DELETE_ON_CLOSE) {
    unlink(path.value().c_str());
  }

  async_ = ((flags & FLAG_ASYNC) == FLAG_ASYNC);
  error_details_ = FILE_OK;
  file_.reset(descriptor);
}

bool File::Flush() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(IsValid());
  SCOPED_FILE_TRACE("Flush");

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX)
  return !HANDLE_EINTR(fdatasync(file_.get()));
#elif BUILDFLAG(IS_APPLE)
  // On macOS and iOS, fsync() is guaranteed to send the file's data to the
  // underlying storage device, but may return before the device actually writes
  // the data to the medium. When used by database systems, this may result in
  // unexpected data loss. This function uses F_BARRIERFSYNC to provide stronger
  // guarantees than fsync(). The default behavior used to be `F_FULLFSYNC`.
  // Changing it to F_BARRIERFSYNC for greatly reduced latency was extensively
  // tried via experiment and showed no detectable sign of increased corruption
  // in mechanisms that make use of this function. For similar discussions
  // regarding rationale one can refer to the SQLite documentation where the
  // default is to go directly to fsync. (See PRAGMA fullfsync)
  //
  // See documentation:
  // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html
  //
  if (!HANDLE_EINTR(fcntl(file_.get(), F_BARRIERFSYNC))) {
    return true;
  }

  // `fsync()` if `F_BARRIERFSYNC` failed. Some file systems do not support
  // `F_BARRIERFSYNC` but we cannot use the error code as a definitive indicator
  // that it's the case, so we'll keep trying `F_BARRIERFSYNC` for every call to
  // this method when it's the case. See the CL description at
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
  if (path.IsContentUri() || path.IsVirtualDocumentPath()) {
    std::optional<FilePath> content_uri = base::ResolveToContentUri(path);
    if (!content_uri) {
      errno = ENOENT;
      return -1;
    }
    // Attempt to open the file and call GetInfo(), otherwise call Java code
    // with the path which is required for dirs.
    File file(*content_uri, base::File::FLAG_OPEN | base::File::FLAG_READ);
    Info info;
    if ((file.IsValid() && file.GetInfo(&info)) ||
        GetContentUriInfo(*content_uri, &info)) {
      UNSAFE_BUFFERS(memset(sb, 0, sizeof(*sb)));
      sb->st_mode = info.is_directory ? S_IFDIR : S_IFREG;
      sb->st_size = info.size;
      sb->st_mtime = info.last_modified.ToTimeT();
      // Time internally is stored as microseconds since windows epoch, so first
      // get subsecond time, and then convert to nanos. Do not subtract
      // Time::UnixEpoch() (which is a little bigger than 2^53), or convert to
      // nanos (multiply by 10^3 which is just under 2^10) prior to doing
      // modulo as these can cause overflow / clamping at [-2^63, 2^63) which
      // will corrupt the result.
      sb->st_mtime_nsec =
          (info.last_modified.ToDeltaSinceWindowsEpoch().InMicroseconds() %
           Time::kMicrosecondsPerSecond) *
          Time::kNanosecondsPerMicrosecond;
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

int File::Mkdir(const FilePath& path, mode_t mode) {
#if BUILDFLAG(IS_ANDROID)
  if (path.IsVirtualDocumentPath()) {
    std::optional<files_internal::VirtualDocumentPath> vp =
        files_internal::VirtualDocumentPath::Parse(path.value());
    if (!vp) {
      errno = ENOENT;
      return -1;
    }
    return vp->Mkdir(mode) ? 0 : -1;
  }
#endif
  return mkdir(path.value().c_str(), mode);
}

}  // namespace base
