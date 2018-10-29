// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/stack.h"
#include "base/environment.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <AvailabilityMacros.h>
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "base/os_compat_android.h"
#endif

#if !defined(OS_IOS)
#include <grp.h>
#endif

// We need to do this on AIX due to some inconsistencies in how AIX
// handles XOPEN_SOURCE and ALL_SOURCE.
#if defined(OS_AIX)
extern "C" char* mkdtemp(char* path);
#endif

namespace base {

namespace {

#if defined(OS_BSD) || defined(OS_MACOSX) || defined(OS_NACL) || \
  defined(OS_FUCHSIA) || (defined(OS_ANDROID) && __ANDROID_API__ < 21)
int CallStat(const char* path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return stat(path, sb);
}
int CallLstat(const char* path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return lstat(path, sb);
}
#else
int CallStat(const char* path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return stat64(path, sb);
}
int CallLstat(const char* path, stat_wrapper_t* sb) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return lstat64(path, sb);
}
#endif

#if !defined(OS_NACL_NONSFI)
// Helper for VerifyPathControlledByUser.
bool VerifySpecificPathControlledByUser(const FilePath& path,
                                        uid_t owner_uid,
                                        const std::set<gid_t>& group_gids) {
  stat_wrapper_t stat_info;
  if (CallLstat(path.value().c_str(), &stat_info) != 0) {
    DPLOG(ERROR) << "Failed to get information on path "
                 << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    DLOG(ERROR) << "Path " << path.value() << " is a symbolic link.";
    return false;
  }

  if (stat_info.st_uid != owner_uid) {
    DLOG(ERROR) << "Path " << path.value() << " is owned by the wrong user.";
    return false;
  }

  if ((stat_info.st_mode & S_IWGRP) &&
      !ContainsKey(group_gids, stat_info.st_gid)) {
    DLOG(ERROR) << "Path " << path.value()
                << " is writable by an unprivileged group.";
    return false;
  }

  if (stat_info.st_mode & S_IWOTH) {
    DLOG(ERROR) << "Path " << path.value() << " is writable by any user.";
    return false;
  }

  return true;
}

std::string TempFileName() {
#if defined(OS_MACOSX)
  return StringPrintf(".%s.XXXXXX", base::mac::BaseBundleID());
#endif

#if defined(GOOGLE_CHROME_BUILD)
  return std::string(".com.google.Chrome.XXXXXX");
#else
  return std::string(".org.chromium.Chromium.XXXXXX");
#endif
}

bool AdvanceEnumeratorWithStat(FileEnumerator* traversal,
                               FilePath* out_next_path,
                               struct stat* out_next_stat) {
  DCHECK(out_next_path);
  DCHECK(out_next_stat);
  *out_next_path = traversal->Next();
  if (out_next_path->empty())
    return false;

  *out_next_stat = traversal->GetInfo().stat();
  return true;
}

bool CopyFileContents(File* infile, File* outfile) {
  static constexpr size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);

  for (;;) {
    ssize_t bytes_read = infile->ReadAtCurrentPos(buffer.data(), buffer.size());
    if (bytes_read < 0)
      return false;
    if (bytes_read == 0)
      return true;
    // Allow for partial writes
    ssize_t bytes_written_per_read = 0;
    do {
      ssize_t bytes_written_partial = outfile->WriteAtCurrentPos(
          &buffer[bytes_written_per_read], bytes_read - bytes_written_per_read);
      if (bytes_written_partial < 0)
        return false;

      bytes_written_per_read += bytes_written_partial;
    } while (bytes_written_per_read < bytes_read);
  }

  NOTREACHED();
  return false;
}

bool DoCopyDirectory(const FilePath& from_path,
                     const FilePath& to_path,
                     bool recursive,
                     bool open_exclusive) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  // Some old callers of CopyDirectory want it to support wildcards.
  // After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  DCHECK(to_path.value().find('*') == std::string::npos);
  DCHECK(from_path.value().find('*') == std::string::npos);

  if (from_path.value().size() >= PATH_MAX) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path))
    real_to_path = MakeAbsoluteFilePath(real_to_path);
  else
    real_to_path = MakeAbsoluteFilePath(real_to_path.DirName());
  if (real_to_path.empty())
    return false;

  FilePath real_from_path = MakeAbsoluteFilePath(from_path);
  if (real_from_path.empty())
    return false;
  if (real_to_path == real_from_path || real_from_path.IsParent(real_to_path))
    return false;

  int traverse_type = FileEnumerator::FILES | FileEnumerator::SHOW_SYM_LINKS;
  if (recursive)
    traverse_type |= FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(from_path, recursive, traverse_type);

  // We have to mimic windows behavior here. |to_path| may not exist yet,
  // start the loop with |to_path|.
  struct stat from_stat;
  FilePath current = from_path;
  if (stat(from_path.value().c_str(), &from_stat) < 0) {
    DPLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                 << from_path.value();
    return false;
  }
  FilePath from_path_base = from_path;
  if (recursive && DirectoryExists(to_path)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  // The Windows version of this function assumes that non-recursive calls
  // will always have a directory for from_path.
  // TODO(maruel): This is not necessary anymore.
  DCHECK(recursive || S_ISDIR(from_stat.st_mode));

  do {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    FilePath target_path(to_path);
    if (from_path_base != current &&
        !from_path_base.AppendRelativePath(current, &target_path)) {
      return false;
    }

    if (S_ISDIR(from_stat.st_mode)) {
      mode_t mode = (from_stat.st_mode & 01777) | S_IRUSR | S_IXUSR | S_IWUSR;
      if (mkdir(target_path.value().c_str(), mode) == 0)
        continue;
      if (errno == EEXIST && !open_exclusive)
        continue;

      DPLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                   << target_path.value();
      return false;
    }

    if (!S_ISREG(from_stat.st_mode)) {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
      continue;
    }

    // Add O_NONBLOCK so we can't block opening a pipe.
    File infile(open(current.value().c_str(), O_RDONLY | O_NONBLOCK));
    if (!infile.IsValid()) {
      DPLOG(ERROR) << "CopyDirectory() couldn't open file: " << current.value();
      return false;
    }

    struct stat stat_at_use;
    if (fstat(infile.GetPlatformFile(), &stat_at_use) < 0) {
      DPLOG(ERROR) << "CopyDirectory() couldn't stat file: " << current.value();
      return false;
    }

    if (!S_ISREG(stat_at_use.st_mode)) {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
      continue;
    }

    int open_flags = O_WRONLY | O_CREAT;
    // If |open_exclusive| is set then we should always create the destination
    // file, so O_NONBLOCK is not necessary to ensure we don't block on the
    // open call for the target file below, and since the destination will
    // always be a regular file it wouldn't affect the behavior of the
    // subsequent write calls anyway.
    if (open_exclusive)
      open_flags |= O_EXCL;
    else
      open_flags |= O_TRUNC | O_NONBLOCK;
    // Each platform has different default file opening modes for CopyFile which
    // we want to replicate here. On OS X, we use copyfile(3) which takes the
    // source file's permissions into account. On the other platforms, we just
    // use the base::File constructor. On Chrome OS, base::File uses a different
    // set of permissions than it does on other POSIX platforms.
#if defined(OS_MACOSX)
    int mode = 0600 | (stat_at_use.st_mode & 0177);
#elif defined(OS_CHROMEOS)
    int mode = 0644;
#else
    int mode = 0600;
#endif
    File outfile(open(target_path.value().c_str(), open_flags, mode));
    if (!outfile.IsValid()) {
      DPLOG(ERROR) << "CopyDirectory() couldn't create file: "
                   << target_path.value();
      return false;
    }

    if (!CopyFileContents(&infile, &outfile)) {
      DLOG(ERROR) << "CopyDirectory() couldn't copy file: " << current.value();
      return false;
    }
  } while (AdvanceEnumeratorWithStat(&traversal, &current, &from_stat));

  return true;
}
#endif  // !defined(OS_NACL_NONSFI)

#if !defined(OS_MACOSX)
// Appends |mode_char| to |mode| before the optional character set encoding; see
// https://www.gnu.org/software/libc/manual/html_node/Opening-Streams.html for
// details.
std::string AppendModeCharacter(StringPiece mode, char mode_char) {
  std::string result(mode.as_string());
  size_t comma_pos = result.find(',');
  result.insert(comma_pos == std::string::npos ? result.length() : comma_pos, 1,
                mode_char);
  return result;
}
#endif

}  // namespace

#if !defined(OS_NACL_NONSFI)
FilePath MakeAbsoluteFilePath(const FilePath& input) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  char full_path[PATH_MAX];
  if (realpath(input.value().c_str(), full_path) == nullptr)
    return FilePath();
  return FilePath(full_path);
}

// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool DeleteFile(const FilePath& path, bool recursive) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  const char* path_str = path.value().c_str();
  stat_wrapper_t file_info;
  if (CallLstat(path_str, &file_info) != 0) {
    // The Windows version defines this condition as success.
    return (errno == ENOENT || errno == ENOTDIR);
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0);
  if (!recursive)
    return (rmdir(path_str) == 0);

  bool success = true;
  stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true,
      FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
      FileEnumerator::SHOW_SYM_LINKS);
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (traversal.GetInfo().IsDirectory())
      directories.push(current.value());
    else
      success &= (unlink(current.value().c_str()) == 0);
  }

  while (!directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success &= (rmdir(dir.value().c_str()) == 0);
  }
  return success;
}

bool ReplaceFile(const FilePath& from_path,
                 const FilePath& to_path,
                 File::Error* error) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;
  if (error)
    *error = File::GetLastFileError();
  return false;
}

bool CopyDirectory(const FilePath& from_path,
                   const FilePath& to_path,
                   bool recursive) {
  return DoCopyDirectory(from_path, to_path, recursive, false);
}

bool CopyDirectoryExcl(const FilePath& from_path,
                       const FilePath& to_path,
                       bool recursive) {
  return DoCopyDirectory(from_path, to_path, recursive, true);
}
#endif  // !defined(OS_NACL_NONSFI)

bool CreateLocalNonBlockingPipe(int fds[2]) {
#if defined(OS_LINUX)
  return pipe2(fds, O_CLOEXEC | O_NONBLOCK) == 0;
#else
  int raw_fds[2];
  if (pipe(raw_fds) != 0)
    return false;
  ScopedFD fd_out(raw_fds[0]);
  ScopedFD fd_in(raw_fds[1]);
  if (!SetCloseOnExec(fd_out.get()))
    return false;
  if (!SetCloseOnExec(fd_in.get()))
    return false;
  if (!SetNonBlocking(fd_out.get()))
    return false;
  if (!SetNonBlocking(fd_in.get()))
    return false;
  fds[0] = fd_out.release();
  fds[1] = fd_in.release();
  return true;
#endif
}

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return false;
  if (flags & O_NONBLOCK)
    return true;
  if (HANDLE_EINTR(fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1)
    return false;
  return true;
}

bool SetCloseOnExec(int fd) {
#if defined(OS_NACL_NONSFI)
  const int flags = 0;
#else
  const int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    return false;
  if (flags & FD_CLOEXEC)
    return true;
#endif  // defined(OS_NACL_NONSFI)
  if (HANDLE_EINTR(fcntl(fd, F_SETFD, flags | FD_CLOEXEC)) == -1)
    return false;
  return true;
}

bool PathExists(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
#if defined(OS_ANDROID)
  if (path.IsContentUri()) {
    return ContentUriExists(path);
  }
#endif
  return access(path.value().c_str(), F_OK) == 0;
}

#if !defined(OS_NACL_NONSFI)
bool PathIsWritable(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return access(path.value().c_str(), W_OK) == 0;
}
#endif  // !defined(OS_NACL_NONSFI)

bool DirectoryExists(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  stat_wrapper_t file_info;
  if (CallStat(path.value().c_str(), &file_info) != 0)
    return false;
  return S_ISDIR(file_info.st_mode);
}

bool ReadFromFD(int fd, char* buffer, size_t bytes) {
  size_t total_read = 0;
  while (total_read < bytes) {
    ssize_t bytes_read =
        HANDLE_EINTR(read(fd, buffer + total_read, bytes - total_read));
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
  }
  return total_read == bytes;
}

#if !defined(OS_NACL_NONSFI)

int CreateAndOpenFdForTemporaryFileInDir(const FilePath& directory,
                                         FilePath* path) {
  ScopedBlockingCall scoped_blocking_call(
      BlockingType::MAY_BLOCK);  // For call to mkstemp().
  *path = directory.Append(TempFileName());
  const std::string& tmpdir_string = path->value();
  // this should be OK since mkstemp just replaces characters in place
  char* buffer = const_cast<char*>(tmpdir_string.c_str());

  return HANDLE_EINTR(mkstemp(buffer));
}

#if !defined(OS_FUCHSIA)
bool CreateSymbolicLink(const FilePath& target_path,
                        const FilePath& symlink_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(!target_path.empty());
  return ::symlink(target_path.value().c_str(),
                   symlink_path.value().c_str()) != -1;
}

bool ReadSymbolicLink(const FilePath& symlink_path, FilePath* target_path) {
  DCHECK(!symlink_path.empty());
  DCHECK(target_path);
  char buf[PATH_MAX];
  ssize_t count = ::readlink(symlink_path.value().c_str(), buf, arraysize(buf));

  if (count <= 0) {
    target_path->clear();
    return false;
  }

  *target_path = FilePath(FilePath::StringType(buf, count));
  return true;
}

bool GetPosixFilePermissions(const FilePath& path, int* mode) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  DCHECK(mode);

  stat_wrapper_t file_info;
  // Uses stat(), because on symbolic link, lstat() does not return valid
  // permission bits in st_mode
  if (CallStat(path.value().c_str(), &file_info) != 0)
    return false;

  *mode = file_info.st_mode & FILE_PERMISSION_MASK;
  return true;
}

bool SetPosixFilePermissions(const FilePath& path,
                             int mode) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  DCHECK_EQ(mode & ~FILE_PERMISSION_MASK, 0);

  // Calls stat() so that we can preserve the higher bits like S_ISGID.
  stat_wrapper_t stat_buf;
  if (CallStat(path.value().c_str(), &stat_buf) != 0)
    return false;

  // Clears the existing permission bits, and adds the new ones.
  mode_t updated_mode_bits = stat_buf.st_mode & ~FILE_PERMISSION_MASK;
  updated_mode_bits |= mode & FILE_PERMISSION_MASK;

  if (HANDLE_EINTR(chmod(path.value().c_str(), updated_mode_bits)) != 0)
    return false;

  return true;
}

bool ExecutableExistsInPath(Environment* env,
                            const FilePath::StringType& executable) {
  std::string path;
  if (!env->GetVar("PATH", &path)) {
    LOG(ERROR) << "No $PATH variable. Assuming no " << executable << ".";
    return false;
  }

  for (const StringPiece& cur_path :
       SplitStringPiece(path, ":", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    FilePath file(cur_path);
    int permissions;
    if (GetPosixFilePermissions(file.Append(executable), &permissions) &&
        (permissions & FILE_PERMISSION_EXECUTE_BY_USER))
      return true;
  }
  return false;
}

#endif  // !OS_FUCHSIA

#if !defined(OS_MACOSX)
// This is implemented in file_util_mac.mm for Mac.
bool GetTempDir(FilePath* path) {
  const char* tmp = getenv("TMPDIR");
  if (tmp) {
    *path = FilePath(tmp);
    return true;
  }

#if defined(OS_ANDROID)
  return PathService::Get(DIR_CACHE, path);
#else
  *path = FilePath("/tmp");
  return true;
#endif
}
#endif  // !defined(OS_MACOSX)

#if !defined(OS_MACOSX)  // Mac implementation is in file_util_mac.mm.
FilePath GetHomeDir() {
#if defined(OS_CHROMEOS)
  if (SysInfo::IsRunningOnChromeOS()) {
    // On Chrome OS chrome::DIR_USER_DATA is overridden with a primary user
    // homedir once it becomes available. Return / as the safe option.
    return FilePath("/");
  }
#endif

  const char* home_dir = getenv("HOME");
  if (home_dir && home_dir[0])
    return FilePath(home_dir);

#if defined(OS_ANDROID)
  DLOG(WARNING) << "OS_ANDROID: Home directory lookup not yet implemented.";
#endif

  FilePath rv;
  if (GetTempDir(&rv))
    return rv;

  // Last resort.
  return FilePath("/tmp");
}
#endif  // !defined(OS_MACOSX)

bool CreateTemporaryFile(FilePath* path) {
  ScopedBlockingCall scoped_blocking_call(
      BlockingType::MAY_BLOCK);  // For call to close().
  FilePath directory;
  if (!GetTempDir(&directory))
    return false;
  int fd = CreateAndOpenFdForTemporaryFileInDir(directory, path);
  if (fd < 0)
    return false;
  close(fd);
  return true;
}

FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  int fd = CreateAndOpenFdForTemporaryFileInDir(dir, path);
  if (fd < 0)
    return nullptr;

  FILE* file = fdopen(fd, "a+");
  if (!file)
    close(fd);
  return file;
}

bool CreateTemporaryFileInDir(const FilePath& dir, FilePath* temp_file) {
  ScopedBlockingCall scoped_blocking_call(
      BlockingType::MAY_BLOCK);  // For call to close().
  int fd = CreateAndOpenFdForTemporaryFileInDir(dir, temp_file);
  return ((fd >= 0) && !IGNORE_EINTR(close(fd)));
}

static bool CreateTemporaryDirInDirImpl(const FilePath& base_dir,
                                        const FilePath::StringType& name_tmpl,
                                        FilePath* new_dir) {
  ScopedBlockingCall scoped_blocking_call(
      BlockingType::MAY_BLOCK);  // For call to mkdtemp().
  DCHECK(name_tmpl.find("XXXXXX") != FilePath::StringType::npos)
      << "Directory name template must contain \"XXXXXX\".";

  FilePath sub_dir = base_dir.Append(name_tmpl);
  std::string sub_dir_string = sub_dir.value();

  // this should be OK since mkdtemp just replaces characters in place
  char* buffer = const_cast<char*>(sub_dir_string.c_str());
  char* dtemp = mkdtemp(buffer);
  if (!dtemp) {
    DPLOG(ERROR) << "mkdtemp";
    return false;
  }
  *new_dir = FilePath(dtemp);
  return true;
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             const FilePath::StringType& prefix,
                             FilePath* new_dir) {
  FilePath::StringType mkdtemp_template = prefix;
  mkdtemp_template.append(FILE_PATH_LITERAL("XXXXXX"));
  return CreateTemporaryDirInDirImpl(base_dir, mkdtemp_template, new_dir);
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath tmpdir;
  if (!GetTempDir(&tmpdir))
    return false;

  return CreateTemporaryDirInDirImpl(tmpdir, TempFileName(), new_temp_path);
}

bool CreateDirectoryAndGetError(const FilePath& full_path,
                                File::Error* error) {
  ScopedBlockingCall scoped_blocking_call(
      BlockingType::MAY_BLOCK);  // For call to mkdir().
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (auto i = subpaths.rbegin(); i != subpaths.rend(); ++i) {
    if (DirectoryExists(*i))
      continue;
    if (mkdir(i->value().c_str(), 0700) == 0)
      continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    int saved_errno = errno;
    if (!DirectoryExists(*i)) {
      if (error)
        *error = File::OSErrorToFileError(saved_errno);
      return false;
    }
  }
  return true;
}

bool NormalizeFilePath(const FilePath& path, FilePath* normalized_path) {
  FilePath real_path_result = MakeAbsoluteFilePath(path);
  if (real_path_result.empty())
    return false;

  // To be consistant with windows, fail if |real_path_result| is a
  // directory.
  if (DirectoryExists(real_path_result))
    return false;

  *normalized_path = real_path_result;
  return true;
}

// TODO(rkc): Refactor GetFileInfo and FileEnumerator to handle symlinks
// correctly. http://code.google.com/p/chromium-os/issues/detail?id=15948
bool IsLink(const FilePath& file_path) {
  stat_wrapper_t st;
  // If we can't lstat the file, it's safe to assume that the file won't at
  // least be a 'followable' link.
  if (CallLstat(file_path.value().c_str(), &st) != 0)
    return false;
  return S_ISLNK(st.st_mode);
}

bool GetFileInfo(const FilePath& file_path, File::Info* results) {
  stat_wrapper_t file_info;
#if defined(OS_ANDROID)
  if (file_path.IsContentUri()) {
    File file = OpenContentUriForRead(file_path);
    if (!file.IsValid())
      return false;
    return file.GetInfo(results);
  } else {
#endif  // defined(OS_ANDROID)
    if (CallStat(file_path.value().c_str(), &file_info) != 0)
      return false;
#if defined(OS_ANDROID)
  }
#endif  // defined(OS_ANDROID)

  results->FromStat(file_info);
  return true;
}
#endif  // !defined(OS_NACL_NONSFI)

FILE* OpenFile(const FilePath& filename, const char* mode) {
  // 'e' is unconditionally added below, so be sure there is not one already
  // present before a comma in |mode|.
  DCHECK(
      strchr(mode, 'e') == nullptr ||
      (strchr(mode, ',') != nullptr && strchr(mode, 'e') > strchr(mode, ',')));
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  FILE* result = nullptr;
#if defined(OS_MACOSX)
  // macOS does not provide a mode character to set O_CLOEXEC; see
  // https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/fopen.3.html.
  const char* the_mode = mode;
#else
  std::string mode_with_e(AppendModeCharacter(mode, 'e'));
  const char* the_mode = mode_with_e.c_str();
#endif
  do {
    result = fopen(filename.value().c_str(), the_mode);
  } while (!result && errno == EINTR);
#if defined(OS_MACOSX)
  // Mark the descriptor as close-on-exec.
  if (result)
    SetCloseOnExec(fileno(result));
#endif
  return result;
}

// NaCl doesn't implement system calls to open files directly.
#if !defined(OS_NACL)
FILE* FileToFILE(File file, const char* mode) {
  FILE* stream = fdopen(file.GetPlatformFile(), mode);
  if (stream)
    file.TakePlatformFile();
  return stream;
}
#endif  // !defined(OS_NACL)

int ReadFile(const FilePath& filename, char* data, int max_size) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_RDONLY));
  if (fd < 0)
    return -1;

  ssize_t bytes_read = HANDLE_EINTR(read(fd, data, max_size));
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_read;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  int fd = HANDLE_EINTR(creat(filename.value().c_str(), 0666));
  if (fd < 0)
    return -1;

  int bytes_written = WriteFileDescriptor(fd, data, size) ? size : -1;
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_written;
}

bool WriteFileDescriptor(const int fd, const char* data, int size) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size;
       bytes_written_total += bytes_written_partial) {
    bytes_written_partial =
        HANDLE_EINTR(write(fd, data + bytes_written_total,
                           size - bytes_written_total));
    if (bytes_written_partial < 0)
      return false;
  }

  return true;
}

#if !defined(OS_NACL_NONSFI)

bool AppendToFile(const FilePath& filename, const char* data, int size) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  bool ret = true;
  int fd = HANDLE_EINTR(open(filename.value().c_str(), O_WRONLY | O_APPEND));
  if (fd < 0) {
    VPLOG(1) << "Unable to create file " << filename.value();
    return false;
  }

  // This call will either write all of the data or return false.
  if (!WriteFileDescriptor(fd, data, size)) {
    VPLOG(1) << "Error while writing to file " << filename.value();
    ret = false;
  }

  if (IGNORE_EINTR(close(fd)) < 0) {
    VPLOG(1) << "Error while closing file " << filename.value();
    return false;
  }

  return ret;
}

bool GetCurrentDirectory(FilePath* dir) {
  // getcwd can return ENOENT, which implies it checks against the disk.
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  char system_buffer[PATH_MAX] = "";
  if (!getcwd(system_buffer, sizeof(system_buffer))) {
    NOTREACHED();
    return false;
  }
  *dir = FilePath(system_buffer);
  return true;
}

bool SetCurrentDirectory(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return chdir(path.value().c_str()) == 0;
}

bool VerifyPathControlledByUser(const FilePath& base,
                                const FilePath& path,
                                uid_t owner_uid,
                                const std::set<gid_t>& group_gids) {
  if (base != path && !base.IsParent(path)) {
     DLOG(ERROR) << "|base| must be a subdirectory of |path|.  base = \""
                 << base.value() << "\", path = \"" << path.value() << "\"";
     return false;
  }

  std::vector<FilePath::StringType> base_components;
  std::vector<FilePath::StringType> path_components;

  base.GetComponents(&base_components);
  path.GetComponents(&path_components);

  std::vector<FilePath::StringType>::const_iterator ib, ip;
  for (ib = base_components.begin(), ip = path_components.begin();
       ib != base_components.end(); ++ib, ++ip) {
    // |base| must be a subpath of |path|, so all components should match.
    // If these CHECKs fail, look at the test that base is a parent of
    // path at the top of this function.
    DCHECK(ip != path_components.end());
    DCHECK(*ip == *ib);
  }

  FilePath current_path = base;
  if (!VerifySpecificPathControlledByUser(current_path, owner_uid, group_gids))
    return false;

  for (; ip != path_components.end(); ++ip) {
    current_path = current_path.Append(*ip);
    if (!VerifySpecificPathControlledByUser(
            current_path, owner_uid, group_gids))
      return false;
  }
  return true;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool VerifyPathControlledByAdmin(const FilePath& path) {
  const unsigned kRootUid = 0;
  const FilePath kFileSystemRoot("/");

  // The name of the administrator group on mac os.
  const char* const kAdminGroupNames[] = {
    "admin",
    "wheel"
  };

  // Reading the groups database may touch the file system.
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);

  std::set<gid_t> allowed_group_ids;
  for (int i = 0, ie = arraysize(kAdminGroupNames); i < ie; ++i) {
    struct group *group_record = getgrnam(kAdminGroupNames[i]);
    if (!group_record) {
      DPLOG(ERROR) << "Could not get the group ID of group \""
                   << kAdminGroupNames[i] << "\".";
      continue;
    }

    allowed_group_ids.insert(group_record->gr_gid);
  }

  return VerifyPathControlledByUser(
      kFileSystemRoot, path, kRootUid, allowed_group_ids);
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

int GetMaximumPathComponentLength(const FilePath& path) {
#if defined(OS_FUCHSIA)
  // Return a value we do not expect anyone ever to reach, but which is small
  // enough to guard against e.g. bugs causing multi-megabyte paths.
  return 1024;
#else
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  return pathconf(path.value().c_str(), _PC_NAME_MAX);
#endif
}

#if !defined(OS_ANDROID)
// This is implemented in file_util_android.cc for that platform.
bool GetShmemTempDir(bool executable, FilePath* path) {
#if defined(OS_LINUX) || defined(OS_AIX)
  bool disable_dev_shm = false;
#if !defined(OS_CHROMEOS)
  disable_dev_shm = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDevShmUsage);
#endif
  bool use_dev_shm = true;
  if (executable) {
    static const bool s_dev_shm_executable =
        IsPathExecutable(FilePath("/dev/shm"));
    use_dev_shm = s_dev_shm_executable;
  }
  if (use_dev_shm && !disable_dev_shm) {
    *path = FilePath("/dev/shm");
    return true;
  }
#endif  // defined(OS_LINUX) || defined(OS_AIX)
  return GetTempDir(path);
}
#endif  // !defined(OS_ANDROID)

#if !defined(OS_MACOSX)
// Mac has its own implementation, this is for all other Posix systems.
bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  File infile;
#if defined(OS_ANDROID)
  if (from_path.IsContentUri()) {
    infile = OpenContentUriForRead(from_path);
  } else {
    infile = File(from_path, File::FLAG_OPEN | File::FLAG_READ);
  }
#else
  infile = File(from_path, File::FLAG_OPEN | File::FLAG_READ);
#endif
  if (!infile.IsValid())
    return false;

  File outfile(to_path, File::FLAG_WRITE | File::FLAG_CREATE_ALWAYS);
  if (!outfile.IsValid())
    return false;

  return CopyFileContents(&infile, &outfile);
}
#endif  // !defined(OS_MACOSX)

// -----------------------------------------------------------------------------

namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(BlockingType::MAY_BLOCK);
  // Windows compatibility: if |to_path| exists, |from_path| and |to_path|
  // must be the same type, either both files, or both directories.
  stat_wrapper_t to_file_info;
  if (CallStat(to_path.value().c_str(), &to_file_info) == 0) {
    stat_wrapper_t from_file_info;
    if (CallStat(from_path.value().c_str(), &from_file_info) != 0)
      return false;
    if (S_ISDIR(to_file_info.st_mode) != S_ISDIR(from_file_info.st_mode))
      return false;
  }

  if (rename(from_path.value().c_str(), to_path.value().c_str()) == 0)
    return true;

  if (!CopyDirectory(from_path, to_path, true))
    return false;

  DeleteFile(from_path, true);
  return true;
}

}  // namespace internal

#endif  // !defined(OS_NACL_NONSFI)

#if defined(OS_LINUX) || defined(OS_AIX)
BASE_EXPORT bool IsPathExecutable(const FilePath& path) {
  bool result = false;
  FilePath tmp_file_path;

  ScopedFD fd(CreateAndOpenFdForTemporaryFileInDir(path, &tmp_file_path));
  if (fd.is_valid()) {
    DeleteFile(tmp_file_path, false);
    long sysconf_result = sysconf(_SC_PAGESIZE);
    CHECK_GE(sysconf_result, 0);
    size_t pagesize = static_cast<size_t>(sysconf_result);
    CHECK_GE(sizeof(pagesize), sizeof(sysconf_result));
    void* mapping = mmap(nullptr, pagesize, PROT_READ, MAP_SHARED, fd.get(), 0);
    if (mapping != MAP_FAILED) {
      if (mprotect(mapping, pagesize, PROT_READ | PROT_EXEC) == 0)
        result = true;
      munmap(mapping, pagesize);
    }
  }
  return result;
}
#endif  // defined(OS_LINUX) || defined(OS_AIX)

}  // namespace base
