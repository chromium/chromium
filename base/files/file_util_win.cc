// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <windows.h>
#include <winsock2.h>

#include <io.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/clang_profiling_buildflags.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split_win.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "base/win/windows_version.h"

namespace base {

namespace {

int g_extra_allowed_path_for_no_execute = 0;

constexpr DWORD kFileShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr std::wstring_view kDefaultTempDirPrefix = L"ChromiumTemp";

// Returns the Win32 last error code or ERROR_SUCCESS if the last error code is
// ERROR_FILE_NOT_FOUND or ERROR_PATH_NOT_FOUND. This is useful in cases where
// the absence of a file or path is a success condition (e.g., when attempting
// to delete an item in the filesystem).
DWORD ReturnLastErrorOrSuccessOnNotFound() {
  const DWORD error_code = ::GetLastError();
  return (error_code == ERROR_FILE_NOT_FOUND ||
          error_code == ERROR_PATH_NOT_FOUND)
             ? ERROR_SUCCESS
             : error_code;
}

// Deletes all files and directories in a path.
// Returns ERROR_SUCCESS on success or the Windows error code corresponding to
// the first error encountered. ERROR_FILE_NOT_FOUND and ERROR_PATH_NOT_FOUND
// are considered success conditions, and are therefore never returned.
DWORD DeleteFileRecursive(const FilePath& path,
                          const FilePath::StringType& pattern,
                          bool recursive) {
  FileEnumerator traversal(path, false,
                           FileEnumerator::FILES | FileEnumerator::DIRECTORIES,
                           pattern);
  DWORD result = ERROR_SUCCESS;
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Try to clear the read-only bit if we find it.
    FileEnumerator::FileInfo info = traversal.GetInfo();
    if ((info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) &&
        (recursive || !info.IsDirectory())) {
      ::SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~DWORD{FILE_ATTRIBUTE_READONLY});
    }

    DWORD this_result = ERROR_SUCCESS;
    if (info.IsDirectory()) {
      if (recursive) {
        this_result = DeleteFileRecursive(current, pattern, true);
        DCHECK_NE(static_cast<LONG>(this_result), ERROR_FILE_NOT_FOUND);
        DCHECK_NE(static_cast<LONG>(this_result), ERROR_PATH_NOT_FOUND);
        if (this_result == ERROR_SUCCESS &&
            !::RemoveDirectory(current.value().c_str())) {
          this_result = ReturnLastErrorOrSuccessOnNotFound();
        }
      }
    } else if (!::DeleteFile(current.value().c_str())) {
      this_result = ReturnLastErrorOrSuccessOnNotFound();
    }
    if (result == ERROR_SUCCESS)
      result = this_result;
  }
  return result;
}

// Appends |mode_char| to |mode| before the optional character set encoding; see
// https://msdn.microsoft.com/library/yeby3zcb.aspx for details.
void AppendModeCharacter(wchar_t mode_char, std::wstring* mode) {
  size_t comma_pos = mode->find(L',');
  mode->insert(comma_pos == std::wstring::npos ? mode->length() : comma_pos, 1,
               mode_char);
}

bool DoCopyFile(const FilePath& from_path,
                const FilePath& to_path,
                bool fail_if_exists) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // Unlike the posix implementation that copies the file manually and discards
  // the ACL bits, CopyFile() copies the complete SECURITY_DESCRIPTOR and access
  // bits, which is usually not what we want. We can't do much about the
  // SECURITY_DESCRIPTOR but at least remove the read only bit.
  const wchar_t* dest = to_path.value().c_str();
  if (!::CopyFile(from_path.value().c_str(), dest, fail_if_exists)) {
    // Copy failed.
    return false;
  }
  DWORD attrs = GetFileAttributes(dest);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if (attrs & FILE_ATTRIBUTE_READONLY) {
    SetFileAttributes(dest, attrs & ~DWORD{FILE_ATTRIBUTE_READONLY});
  }
  return true;
}

bool DoCopyDirectory(const FilePath& from_path,
                     const FilePath& to_path,
                     bool recursive,
                     bool fail_if_exists) {
  // NOTE(maruel): Previous version of this function used to call
  // SHFileOperation().  This used to copy the file attributes and extended
  // attributes, OLE structured storage, NTFS file system alternate data
  // streams, SECURITY_DESCRIPTOR. In practice, this is not what we want, we
  // want the containing directory to propagate its SECURITY_DESCRIPTOR.
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // This function does not properly handle destinations within the source.
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path)) {
    real_to_path = MakeAbsoluteFilePath(real_to_path);
    if (real_to_path.empty())
      return false;
  } else {
    real_to_path = MakeAbsoluteFilePath(real_to_path.DirName());
    if (real_to_path.empty())
      return false;
  }
  FilePath real_from_path = MakeAbsoluteFilePath(from_path);
  if (real_from_path.empty())
    return false;
  if (real_to_path == real_from_path || real_from_path.IsParent(real_to_path))
    return false;

  int traverse_type = FileEnumerator::FILES;
  if (recursive)
    traverse_type |= FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(from_path, recursive, traverse_type);

  if (!PathExists(from_path)) {
    DLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                << from_path.value().c_str();
    return false;
  }
  // TODO(maruel): This is not necessary anymore.
  DCHECK(recursive || DirectoryExists(from_path));

  FilePath current = from_path;
  bool from_is_dir = DirectoryExists(from_path);
  bool success = true;
  FilePath from_path_base = from_path;
  if (recursive && DirectoryExists(to_path)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  while (success && !current.empty()) {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    FilePath target_path(to_path);
    if (from_path_base != current) {
      if (!from_path_base.AppendRelativePath(current, &target_path)) {
        success = false;
        break;
      }
    }

    if (from_is_dir) {
      if (!DirectoryExists(target_path) &&
          !::CreateDirectory(target_path.value().c_str(), NULL)) {
        DLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                    << target_path.value().c_str();
        success = false;
      }
    } else if (!DoCopyFile(current, target_path, fail_if_exists)) {
      DLOG(ERROR) << "CopyDirectory() couldn't create file: "
                  << target_path.value().c_str();
      success = false;
    }

    current = traversal.Next();
    if (!current.empty())
      from_is_dir = traversal.GetInfo().IsDirectory();
  }

  return success;
}

// Returns ERROR_SUCCESS on success, or a Windows error code on failure.
DWORD DoDeleteFile(const FilePath& path, bool recursive) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (path.empty())
    return ERROR_SUCCESS;

  if (path.value().length() >= MAX_PATH)
    return ERROR_BAD_PATHNAME;

  // Handle any path with wildcards.
  if (path.BaseName().value().find_first_of(FILE_PATH_LITERAL("*?")) !=
      FilePath::StringType::npos) {
    const DWORD error_code =
        DeleteFileRecursive(path.DirName(), path.BaseName().value(), recursive);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_FILE_NOT_FOUND);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_PATH_NOT_FOUND);
    return error_code;
  }

  // Report success if the file or path does not exist.
  const DWORD attr = ::GetFileAttributes(path.value().c_str());
  if (attr == INVALID_FILE_ATTRIBUTES)
    return ReturnLastErrorOrSuccessOnNotFound();

  // Clear the read-only bit if it is set.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !::SetFileAttributes(path.value().c_str(),
                           attr & ~DWORD{FILE_ATTRIBUTE_READONLY})) {
    // It's possible for |path| to be gone now under a race with other deleters.
    return ReturnLastErrorOrSuccessOnNotFound();
  }

  // Perform a simple delete on anything that isn't a directory.
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
    return ::DeleteFile(path.value().c_str())
               ? ERROR_SUCCESS
               : ReturnLastErrorOrSuccessOnNotFound();
  }

  if (recursive) {
    const DWORD error_code =
        DeleteFileRecursive(path, FILE_PATH_LITERAL("*"), true);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_FILE_NOT_FOUND);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_PATH_NOT_FOUND);
    if (error_code != ERROR_SUCCESS)
      return error_code;
  }
  return ::RemoveDirectory(path.value().c_str())
             ? ERROR_SUCCESS
             : ReturnLastErrorOrSuccessOnNotFound();
}

// Deletes the file/directory at |path| (recursively if |recursive| and |path|
// names a directory), returning true on success. Sets the Windows last-error
// code and returns false on failure.
bool DeleteFileOrSetLastError(const FilePath& path, bool recursive) {
  const DWORD error = DoDeleteFile(path, recursive);
  if (error == ERROR_SUCCESS)
    return true;

  ::SetLastError(error);
  return false;
}

constexpr int kMaxDeleteAttempts = 9;

void DeleteFileWithRetry(const FilePath& path,
                         bool recursive,
                         int attempt,
                         OnceCallback<void(bool)> reply_callback) {
  // Retry every 250ms for up to two seconds. These values were pulled out of
  // thin air, and may be adjusted in the future based on the metrics collected.
  static constexpr TimeDelta kDeleteFileRetryDelay = Milliseconds(250);

  if (DeleteFileOrSetLastError(path, recursive)) {
    // Consider introducing further retries until the item has been removed from
    // the filesystem and its name is ready for reuse; see the comments in
    // chrome/installer/mini_installer/delete_with_retry.cc for details.
    if (!reply_callback.is_null())
      std::move(reply_callback).Run(true);
    return;
  }

  ++attempt;
  DCHECK_LE(attempt, kMaxDeleteAttempts);
  if (attempt == kMaxDeleteAttempts) {
    if (!reply_callback.is_null())
      std::move(reply_callback).Run(false);
    return;
  }

  ThreadPool::PostDelayedTask(FROM_HERE,
                              {TaskPriority::BEST_EFFORT, MayBlock()},
                              BindOnce(&DeleteFileWithRetry, path, recursive,
                                       attempt, std::move(reply_callback)),
                              kDeleteFileRetryDelay);
}

OnceClosure GetDeleteFileCallbackInternal(
    const FilePath& path,
    bool recursive,
    OnceCallback<void(bool)> reply_callback) {
  OnceCallback<void(bool)> bound_callback;
  if (!reply_callback.is_null()) {
    bound_callback = BindPostTask(SequencedTaskRunner::GetCurrentDefault(),
                                  std::move(reply_callback));
  }
  return BindOnce(&DeleteFileWithRetry, path, recursive, /*attempt=*/0,
                  std::move(bound_callback));
}

// This function verifies that no code is attempting to set an ACL on a file
// that is outside of 'safe' paths. A 'safe' path is defined as one that is
// within the user data dir, or the temporary directory. This is explicitly to
// prevent code from trying to pass a writeable handle to a file outside of
// these directories to an untrusted process. E.g. if some future code created a
// writeable handle to a file in c:\users\user\sensitive.dat, this DCHECK would
// hit. Setting an ACL on a file outside of these chrome-controlled directories
// might cause the browser or operating system to fail in unexpected ways.
bool IsPathSafeToSetAclOn(const FilePath& path) {
#if BUILDFLAG(CLANG_PROFILING)
  // TODO(crbug.com/329482479) Use PreventExecuteMappingUnchecked for .profraw.
  // Ignore .profraw profiling files, as they can occur anywhere, and only occur
  // during testing.
  if (path.Extension() == FILE_PATH_LITERAL(".profraw")) {
    return true;
  }
#endif  // BUILDFLAG(CLANG_PROFILING)
  std::vector<int> valid_path_keys({DIR_TEMP});
  if (g_extra_allowed_path_for_no_execute) {
    valid_path_keys.push_back(g_extra_allowed_path_for_no_execute);
  }

  // MakeLongFilePath is needed here because temp files can have an 8.3 path
  // under certain conditions. See comments in base::MakeLongFilePath.
  FilePath long_path = MakeLongFilePath(path);
  DCHECK(!long_path.empty()) << "Cannot get long path for " << path;

  std::vector<FilePath> valid_paths;
  for (const auto path_key : valid_path_keys) {
    FilePath valid_path;
    if (!PathService::Get(path_key, &valid_path)) {
      DLOG(FATAL) << "Cannot get path for pathservice key " << path_key;
      continue;
    }
    valid_paths.push_back(valid_path);
  }

  // Admin users create temporary files in SystemTemp; see
  // `CreateNewTempDirectory` below.
  FilePath secure_system_temp;
  if (::IsUserAnAdmin() &&
      PathService::Get(DIR_SYSTEM_TEMP, &secure_system_temp)) {
    valid_paths.push_back(secure_system_temp);
  }

  for (const auto& valid_path : valid_paths) {
    // Temp files can sometimes have an 8.3 path. See comments in
    // `MakeLongFilePath`.
    FilePath full_path = MakeLongFilePath(valid_path);
    DCHECK(!full_path.empty()) << "Cannot get long path for " << valid_path;
    if (full_path.IsParent(long_path)) {
      return true;
    }
  }

  return false;
}

}  // namespace

OnceClosure GetDeleteFileCallback(const FilePath& path,
                                  OnceCallback<void(bool)> reply_callback) {
  return GetDeleteFileCallbackInternal(path, /*recursive=*/false,
                                       std::move(reply_callback));
}

OnceClosure GetDeletePathRecursivelyCallback(
    const FilePath& path,
    OnceCallback<void(bool)> reply_callback) {
  return GetDeleteFileCallbackInternal(path, /*recursive=*/true,
                                       std::move(reply_callback));
}

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  wchar_t file_path[MAX_PATH];
  if (!_wfullpath(file_path, input.value().c_str(), MAX_PATH))
    return FilePath();
  return FilePath(file_path);
}

bool DeleteFile(const FilePath& path) {
  return DeleteFileOrSetLastError(path, /*recursive=*/false);
}

bool DeletePathRecursively(const FilePath& path) {
  return DeleteFileOrSetLastError(path, /*recursive=*/true);
}

bool DeleteFileAfterReboot(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (path.value().length() >= MAX_PATH)
    return false;

  return ::MoveFileEx(path.value().c_str(), nullptr,
                      MOVEFILE_DELAY_UNTIL_REBOOT);
}

bool ReplaceFile(const FilePath& from_path,
                 const FilePath& to_path,
                 File::Error* error) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Assume that |to_path| already exists and try the normal replace. This will
  // fail with ERROR_FILE_NOT_FOUND if |to_path| does not exist. When writing to
  // a network share, we may not be able to change the ACLs. Ignore ACL errors
  // then (REPLACEFILE_IGNORE_MERGE_ERRORS).
  if (::ReplaceFile(to_path.value().c_str(), from_path.value().c_str(), NULL,
                    REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
    return true;
  }

  File::Error replace_error = File::OSErrorToFileError(GetLastError());

  // Try a simple move next. It will only succeed when |to_path| doesn't already
  // exist.
  if (::MoveFile(from_path.value().c_str(), to_path.value().c_str()))
    return true;

  // In the case of FILE_ERROR_NOT_FOUND from ReplaceFile, it is likely that
  // |to_path| does not exist. In this case, the more relevant error comes
  // from the call to MoveFile.
  if (error) {
    *error = replace_error == File::FILE_ERROR_NOT_FOUND
                 ? File::GetLastFileError()
                 : replace_error;
  }
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

bool PathExists(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

namespace {

bool PathHasAccess(const FilePath& path,
                   DWORD dir_desired_access,
                   DWORD file_desired_access) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  const wchar_t* const path_str = path.value().c_str();
  DWORD fileattr = GetFileAttributes(path_str);
  if (fileattr == INVALID_FILE_ATTRIBUTES)
    return false;

  bool is_directory = fileattr & FILE_ATTRIBUTE_DIRECTORY;
  DWORD desired_access =
      is_directory ? dir_desired_access : file_desired_access;
  DWORD flags_and_attrs =
      is_directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;

  win::ScopedHandle file(CreateFile(path_str, desired_access, kFileShareAll,
                                    nullptr, OPEN_EXISTING, flags_and_attrs,
                                    nullptr));

  return file.is_valid();
}

}  // namespace

bool PathIsReadable(const FilePath& path) {
  return PathHasAccess(path, FILE_LIST_DIRECTORY, GENERIC_READ);
}

bool PathIsWritable(const FilePath& path) {
  return PathHasAccess(path, FILE_ADD_FILE, GENERIC_WRITE);
}

bool DirectoryExists(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

bool GetTempDir(FilePath* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  *path = FilePath(temp_path).StripTrailingSeparators();
  return true;
}

FilePath GetHomeDir() {
  wchar_t result[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT,
                                result)) &&
      result[0]) {
    return FilePath(result);
  }

  // Fall back to the temporary directory on failure.
  FilePath temp;
  if (GetTempDir(&temp))
    return temp;

  // Last resort.
  return FilePath(FILE_PATH_LITERAL("C:\\"));
}

File CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* temp_file) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Open the file with exclusive r/w/d access, and allow the caller to decide
  // to mark it for deletion upon close after the fact.
  constexpr uint32_t kFlags = File::FLAG_CREATE | File::FLAG_READ |
                              File::FLAG_WRITE | File::FLAG_WIN_EXCLUSIVE_READ |
                              File::FLAG_WIN_EXCLUSIVE_WRITE |
                              File::FLAG_CAN_DELETE_ON_CLOSE;

  // Use GUID instead of ::GetTempFileName() to generate unique file names.
  // "Due to the algorithm used to generate file names, GetTempFileName can
  // perform poorly when creating a large number of files with the same prefix.
  // In such cases, it is recommended that you construct unique file names based
  // on GUIDs."
  // https://msdn.microsoft.com/library/windows/desktop/aa364991.aspx

  FilePath temp_name;
  File file;

  // Although it is nearly impossible to get a duplicate name with GUID, we
  // still use a loop here in case it happens.
  for (int i = 0; i < 100; ++i) {
    temp_name = dir.Append(FormatTemporaryFileName(
        UTF8ToWide(Uuid::GenerateRandomV4().AsLowercaseString())));
    file.Initialize(temp_name, kFlags);
    if (file.IsValid())
      break;
  }

  if (!file.IsValid()) {
    DPLOG(WARNING) << "Failed to get temporary file name in " << dir.value();
    return file;
  }

  wchar_t long_temp_name[MAX_PATH + 1];
  const DWORD long_name_len =
      GetLongPathName(temp_name.value().c_str(), long_temp_name, MAX_PATH);
  if (long_name_len != 0 && long_name_len <= MAX_PATH) {
    *temp_file =
        FilePath(FilePath::StringPieceType(long_temp_name, long_name_len));
  } else {
    // GetLongPathName() failed, but we still have a temporary file.
    *temp_file = std::move(temp_name);
  }

  return file;
}

bool CreateTemporaryFileInDir(const FilePath& dir, FilePath* temp_file) {
  return CreateAndOpenTemporaryFileInDir(dir, temp_file).IsValid();
}

FilePath FormatTemporaryFileName(FilePath::StringPieceType identifier) {
  return FilePath(StrCat({identifier, FILE_PATH_LITERAL(".tmp")}));
}

ScopedFILE CreateAndOpenTemporaryStreamInDir(const FilePath& dir,
                                             FilePath* path) {
  // Open file in binary mode, to avoid problems with fwrite. On Windows
  // it replaces \n's with \r\n's, which may surprise you.
  // Reference: http://msdn.microsoft.com/en-us/library/h9t88zwz(VS.71).aspx
  return ScopedFILE(
      FileToFILE(CreateAndOpenTemporaryFileInDir(dir, path), "wb+"));
}

bool CreateTemporaryDirInDir(const FilePath& base_dir,
                             FilePath::StringPieceType prefix,
                             FilePath* new_dir) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  FilePath path_to_create;

  for (int count = 0; count < 50; ++count) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    std::wstring new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(AsWString(NumberToString16(GetCurrentProcId())));
    new_dir_name.push_back('_');
    new_dir_name.append(AsWString(
        NumberToString16(RandInt(0, std::numeric_limits<int32_t>::max()))));

    path_to_create = base_dir.Append(new_dir_name);
    if (::CreateDirectory(path_to_create.value().c_str(), NULL)) {
      *new_dir = path_to_create;
      return true;
    }
  }

  return false;
}

// The directory is created under SystemTemp for security reasons if the caller
// is admin to avoid attacks from lower privilege processes.
//
// If unable to create a dir under SystemTemp, the dir is created under
// %TEMP%. The reasons for not being able to create a dir under SystemTemp could
// be because `%systemroot%\SystemTemp` does not exist, or unable to resolve
// `DIR_WINDOWS` or `DIR_PROGRAM_FILES`, say due to registry redirection, or
// unable to create a directory due to SystemTemp being read-only or having
// atypical ACLs. An override of `DIR_SYSTEM_TEMP` by tests will be respected.
bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  DCHECK(new_temp_path);

  FilePath parent_dir;
  if (::IsUserAnAdmin() && PathService::Get(DIR_SYSTEM_TEMP, &parent_dir) &&
      CreateTemporaryDirInDir(parent_dir,
                              prefix.empty() ? kDefaultTempDirPrefix : prefix,
                              new_temp_path)) {
    return true;
  }

  if (!GetTempDir(&parent_dir))
    return false;

  return CreateTemporaryDirInDir(parent_dir, prefix, new_temp_path);
}

bool CreateDirectoryAndGetError(const FilePath& full_path,
                                File::Error* error) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* const full_path_str = full_path.value().c_str();
  const DWORD fileattr = ::GetFileAttributes(full_path_str);
  if (fileattr != INVALID_FILE_ATTRIBUTES) {
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      return true;
    }
    DLOG(WARNING) << "CreateDirectory(" << full_path_str << "), "
                  << "conflicts with existing file.";
    if (error)
      *error = File::FILE_ERROR_NOT_A_DIRECTORY;
    ::SetLastError(ERROR_FILE_EXISTS);
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  FilePath parent_path(full_path.DirName());
  if (parent_path.value() == full_path.value()) {
    if (error)
      *error = File::FILE_ERROR_NOT_FOUND;
    ::SetLastError(ERROR_FILE_NOT_FOUND);
    return false;
  }
  if (!CreateDirectoryAndGetError(parent_path, error)) {
    DLOG(WARNING) << "Failed to create one of the parent directories.";
    DCHECK(!error || *error != File::FILE_OK);
    return false;
  }

  if (::CreateDirectory(full_path_str, NULL))
    return true;

  const DWORD error_code = ::GetLastError();
  if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path)) {
    // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we were
    // racing with someone creating the same directory, or a file with the same
    // path.  If DirectoryExists() returns true, we lost the race to create the
    // same directory.
    return true;
  }
  if (error)
    *error = File::OSErrorToFileError(error_code);
  ::SetLastError(error_code);
  DPLOG(WARNING) << "Failed to create directory " << full_path_str;
  return false;
}

bool NormalizeFilePath(const FilePath& path, FilePath* real_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  File file(path, File::FLAG_OPEN | File::FLAG_READ |
                      File::FLAG_WIN_SHARE_DELETE |
                      File::FLAG_WIN_BACKUP_SEMANTICS);
  if (!file.IsValid()) {
    return false;
  }

  // The expansion of `path` into a full path may make it longer. Since
  // '\Device\HarddiskVolume1' is 23 characters long, we can add 30 characters.
  constexpr int kMaxPathLength = MAX_PATH + 30;
  wchar_t native_file_path[kMaxPathLength];
  // On success, `used_wchars` returns the number of written characters, not
  // including the trailing '\0'. Thus, failure is indicated by returning 0 or
  // >= kMaxPathLength.
  DWORD used_wchars = ::GetFinalPathNameByHandle(
      file.GetPlatformFile(), native_file_path, kMaxPathLength,
      FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
  if (used_wchars >= kMaxPathLength || used_wchars == 0) {
    return false;
  }

  // With `VOLUME_NAME_NT` flag, GetFinalPathNameByHandle() returns the path
  // with the volume device path and existing code expects we return a path
  // starting 'X:\' so we need to call DevicePathToDriveLetterPath.
  if (!DevicePathToDriveLetterPath(
          FilePath(FilePath::StringPieceType(native_file_path, used_wchars)),
          real_path)) {
    return false;
  }

  // `real_path` can be longer than MAX_PATH and we should only return paths
  // that are less than MAX_PATH.
  return real_path->value().size() <= MAX_PATH;
}

bool DevicePathToDriveLetterPath(const FilePath& nt_device_path,
                                 FilePath* out_drive_letter_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // Get the mapping of drive letters to device paths.
  // Note: There are 26 letters possible, and each entry takes 4 characters of
  // space (e.g. ['C', ':', '\\', '\0'] plus an additional NUL character at the
  // end, meaning 128 is safely above the maximum possible size needed).
  std::array<wchar_t, 128> drive_strings_buffer = {};
  DWORD count = ::GetLogicalDriveStrings(drive_strings_buffer.size() - 1u,
                                         drive_strings_buffer.data());
  CHECK_LT(count, drive_strings_buffer.size());
  if (!count) {
    DLOG(ERROR) << "Failed to get drive mapping";
    return false;
  }
  // Truncate the buffer to the bytes actually copied by GetLogicalDriveStrings.
  // Note: This gets rid of the superfluous NUL character at the end. Thus,
  // `drive_strings` is now a sequence of null terminated strings.
  std::wstring_view drive_strings(drive_strings_buffer.data(), count);

  // For each string in the drive mapping, get the junction that links
  // to it.  If that junction is a prefix of |device_path|, then we
  // know that |drive| is the real path prefix.
  for (std::wstring_view drive_string : base::SplitStringPiece(
           drive_strings, base::MakeStringViewWithNulChars(L"\0"),
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    wchar_t drive[] = L" :";
    drive[0u] = drive_string[0u];  // Copy the drive letter.

    wchar_t device_path_as_string[MAX_PATH];
    if (::QueryDosDevice(drive, device_path_as_string,
                         std::size(device_path_as_string))) {
      FilePath device_path(device_path_as_string);
      if (device_path == nt_device_path ||
          device_path.IsParent(nt_device_path)) {
        *out_drive_letter_path =
            FilePath(drive + nt_device_path.value().substr(
                                 wcslen(device_path_as_string)));
        return true;
      }
    }
  }

  // No drive matched.  The path does not start with a device junction
  // that is mounted as a drive letter.  This means there is no drive
  // letter path to the volume that holds |device_path|, so fail.
  return false;
}

FilePath MakeLongFilePath(const FilePath& input) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  DWORD path_long_len = ::GetLongPathName(input.value().c_str(), nullptr, 0);
  if (path_long_len == 0UL)
    return FilePath();

  std::wstring path_long_str;
  path_long_len = ::GetLongPathName(input.value().c_str(),
                                    WriteInto(&path_long_str, path_long_len),
                                    path_long_len);
  if (path_long_len == 0UL)
    return FilePath();

  return FilePath(path_long_str);
}

bool CreateWinHardLink(const FilePath& to_file, const FilePath& from_file) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return ::CreateHardLink(to_file.value().c_str(), from_file.value().c_str(),
                          nullptr);
}

// TODO(rkc): Work out if we want to handle NTFS junctions here or not, handle
// them if we do decide to.
bool IsLink(const FilePath& file_path) {
  return false;
}

bool GetFileInfo(const FilePath& file_path, File::Info* results) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.value().c_str(), GetFileExInfoStandard,
                           &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  // TODO(crbug.com/40227936): Change Info::size to uint64_t and eliminate this
  // cast.
  results->size = checked_cast<int64_t>(size.QuadPart);

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  results->last_modified = Time::FromFileTime(attr.ftLastWriteTime);
  results->last_accessed = Time::FromFileTime(attr.ftLastAccessTime);
  results->creation_time = Time::FromFileTime(attr.ftCreationTime);

  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  // 'N' is unconditionally added below, so be sure there is not one already
  // present before a comma in |mode|.
  DCHECK(
      strchr(mode, 'N') == nullptr ||
      (strchr(mode, ',') != nullptr && strchr(mode, 'N') > strchr(mode, ',')));
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  std::wstring w_mode = UTF8ToWide(mode);
  AppendModeCharacter(L'N', &w_mode);
  return _wfsopen(filename.value().c_str(), w_mode.c_str(), _SH_DENYNO);
}

FILE* FileToFILE(File file, const char* mode) {
  DCHECK(!file.async());
  if (!file.IsValid())
    return NULL;
  int fd =
      _open_osfhandle(reinterpret_cast<intptr_t>(file.GetPlatformFile()), 0);
  if (fd < 0)
    return NULL;
  file.TakePlatformFile();
  FILE* stream = _fdopen(fd, mode);
  if (!stream)
    _close(fd);
  return stream;
}

File FILEToFile(FILE* file_stream) {
  if (!file_stream)
    return File();

  int fd = _fileno(file_stream);
  DCHECK_GE(fd, 0);
  intptr_t file_handle = _get_osfhandle(fd);
  DCHECK_NE(file_handle, reinterpret_cast<intptr_t>(INVALID_HANDLE_VALUE));

  HANDLE other_handle = nullptr;
  if (!::DuplicateHandle(
          /*hSourceProcessHandle=*/GetCurrentProcess(),
          reinterpret_cast<HANDLE>(file_handle),
          /*hTargetProcessHandle=*/GetCurrentProcess(), &other_handle,
          /*dwDesiredAccess=*/0,
          /*bInheritHandle=*/FALSE,
          /*dwOptions=*/DUPLICATE_SAME_ACCESS)) {
    return File(File::GetLastFileError());
  }

  return File(ScopedPlatformFile(other_handle));
}

std::optional<uint64_t> ReadFile(const FilePath& filename, span<char> buffer) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  win::ScopedHandle file(CreateFile(filename.value().c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                    NULL));
  if (!file.is_valid()) {
    return std::nullopt;
  }

  // TODO(crbug.com/40227936): Consider supporting reading more than INT_MAX
  // bytes.
  DWORD bytes_to_read = static_cast<DWORD>(checked_cast<int>(buffer.size()));

  DWORD bytes_read;
  if (!::ReadFile(file.get(), buffer.data(), bytes_to_read, &bytes_read,
                  nullptr)) {
    return std::nullopt;
  }
  return bytes_read;
}

bool WriteFile(const FilePath& filename, span<const uint8_t> data) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  win::ScopedHandle file(CreateFile(filename.value().c_str(), GENERIC_WRITE, 0,
                                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                    NULL));
  if (!file.is_valid()) {
    DPLOG(WARNING) << "WriteFile failed for path " << filename.value();
    return false;
  }

  DWORD written;
  DWORD size = checked_cast<DWORD>(data.size());
  BOOL result = ::WriteFile(file.get(), data.data(), size, &written, nullptr);
  if (result && written == size) {
    return true;
  }

  if (!result) {
    // WriteFile failed.
    DPLOG(WARNING) << "writing file " << filename.value() << " failed";
  } else {
    // Didn't write all the bytes.
    DLOG(WARNING) << "wrote" << written << " bytes to " << filename.value()
                  << " expected " << size;
  }
  return false;
}

bool AppendToFile(const FilePath& filename, span<const uint8_t> data) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  win::ScopedHandle file(CreateFile(filename.value().c_str(), FILE_APPEND_DATA,
                                    0, nullptr, OPEN_EXISTING, 0, nullptr));
  if (!file.is_valid()) {
    VPLOG(1) << "CreateFile failed for path " << filename.value();
    return false;
  }

  DWORD written;
  DWORD size = checked_cast<DWORD>(data.size());
  BOOL result = ::WriteFile(file.get(), data.data(), size, &written, nullptr);
  if (result && written == size)
    return true;

  if (!result) {
    // WriteFile failed.
    VPLOG(1) << "Writing file " << filename.value() << " failed";
  } else {
    // Didn't write all the bytes.
    VPLOG(1) << "Only wrote " << written << " out of " << size << " byte(s) to "
             << filename.value();
  }
  return false;
}

bool AppendToFile(const FilePath& filename, std::string_view data) {
  return AppendToFile(filename, as_bytes(make_span(data)));
}

bool GetCurrentDirectory(FilePath* dir) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  *dir = FilePath(FilePath::StringPieceType(system_buffer))
             .StripTrailingSeparators();
  return true;
}

bool SetCurrentDirectory(const FilePath& directory) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  return ::SetCurrentDirectory(directory.value().c_str()) != 0;
}

int GetMaximumPathComponentLength(const FilePath& path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  wchar_t volume_path[MAX_PATH];
  if (!GetVolumePathNameW(path.NormalizePathSeparators().value().c_str(),
                          volume_path, std::size(volume_path))) {
    return -1;
  }

  DWORD max_length = 0;
  if (!GetVolumeInformationW(volume_path, NULL, 0, NULL, &max_length, NULL,
                             NULL, 0)) {
    return -1;
  }

  // Length of |path| with path separator appended.
  size_t prefix = path.StripTrailingSeparators().value().size() + 1;
  // The whole path string must be shorter than MAX_PATH. That is, it must be
  // prefix + component_length < MAX_PATH (or equivalently, <= MAX_PATH - 1).
  int whole_path_limit = std::max(0, MAX_PATH - 1 - static_cast<int>(prefix));
  return std::min(whole_path_limit, static_cast<int>(max_length));
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  return DoCopyFile(from_path, to_path, false);
}

bool SetNonBlocking(int fd) {
  unsigned long nonblocking = 1;
  if (ioctlsocket(static_cast<SOCKET>(fd), static_cast<long>(FIONBIO),
                  &nonblocking) == 0)
    return true;
  return false;
}

bool PreReadFile(const FilePath& file_path,
                 bool is_executable,
                 bool sequential,
                 int64_t max_bytes) {
  DCHECK_GE(max_bytes, 0);

  if (max_bytes == 0) {
    // ::PrefetchVirtualMemory() fails when asked to read zero bytes.
    // base::MemoryMappedFile::Initialize() fails on an empty file.
    return true;
  }

  // ::PrefetchVirtualMemory() fails if the file is opened with write access.
  MemoryMappedFile::Access access = is_executable
                                        ? MemoryMappedFile::READ_CODE_IMAGE
                                        : MemoryMappedFile::READ_ONLY;
  MemoryMappedFile mapped_file;
  if (!mapped_file.Initialize(file_path, access)) {
    return false;
  }

  const ::SIZE_T length =
      std::min(base::saturated_cast<::SIZE_T>(max_bytes),
               base::saturated_cast<::SIZE_T>(mapped_file.length()));
  ::_WIN32_MEMORY_RANGE_ENTRY address_range = {mapped_file.data(), length};
  // Use ::PrefetchVirtualMemory(). This is better than a
  // simple data file read, more from a RAM perspective than CPU. This is
  // because reading the file as data results in double mapping to
  // Image/executable pages for all pages of code executed.
  return ::PrefetchVirtualMemory(::GetCurrentProcess(),
                                 /*NumberOfEntries=*/1, &address_range,
                                 /*Flags=*/0);
}

bool PreventExecuteMappingInternal(const FilePath& path, bool skip_path_check) {
  if (!base::FeatureList::IsEnabled(
          features::kEnforceNoExecutableFileHandles)) {
    return true;
  }

  bool is_path_safe = skip_path_check || IsPathSafeToSetAclOn(path);

  if (!is_path_safe) {
    // To mitigate the effect of past OS bugs where attackers are able to use
    // writeable handles to create malicious executable images which can be
    // later mapped into unsandboxed processes, file handles that permit writing
    // that are passed to untrusted processes, e.g. renderers, should be marked
    // with a deny execute ACE. This prevents re-opening the file for execute
    // later on.
    //
    // To accomplish this, code that needs to pass writable file handles to a
    // renderer should open the file with the flags added by
    // `AddFlagsForPassingToUntrustedProcess()` (explicitly
    // FLAG_WIN_NO_EXECUTE). This results in this PreventExecuteMapping being
    // called by base::File.
    //
    // However, simply using this universally on all files that are opened
    // writeable is also undesirable: things can and will randomly break if they
    // are marked no-exec (e.g. marking an exe that the user downloads as
    // no-exec will prevent the user from running it). There are also
    // performance implications of doing this for all files unnecessarily.
    //
    // Code that passes writable files to the renderer is also expected to
    // reference files in places like the user data dir (e.g. for the filesystem
    // API) or temp files. Any attempt to pass a writeable handle to a path
    // outside these areas is likely its own security issue as an untrusted
    // renderer process should never have write access to e.g. system files or
    // downloads.
    //
    // This check aims to catch misuse of
    // `AddFlagsForPassingToUntrustedProcess()` on paths outside these
    // locations. Any time it hits it is also likely that a handle to a
    // dangerous path is being passed to a renderer, which is inherently unsafe.
    //
    // If this check hits, please do not ignore it but consult security team.
    DLOG(FATAL) << "Unsafe to deny execute access to path : " << path;

    return false;
  }

  static constexpr wchar_t kEveryoneSid[] = L"WD";
  auto sids = win::Sid::FromSddlStringVector({kEveryoneSid});

  // Remove executable access from the file. The API does not add a duplicate
  // ACE if it already exists.
  return win::DenyAccessToPath(path, *sids, FILE_EXECUTE, /*NO_INHERITANCE=*/0,
                               /*recursive=*/false);
}

bool PreventExecuteMapping(const FilePath& path) {
  return PreventExecuteMappingInternal(path, false);
}

bool PreventExecuteMappingUnchecked(
    const FilePath& path,
    base::PassKey<PreventExecuteMappingClasses> passkey) {
  return PreventExecuteMappingInternal(path, true);
}

void SetExtraNoExecuteAllowedPath(int path_key) {
  DCHECK(!g_extra_allowed_path_for_no_execute ||
         g_extra_allowed_path_for_no_execute == path_key);
  g_extra_allowed_path_for_no_execute = path_key;
  base::FilePath valid_path;
  DCHECK(
      base::PathService::Get(g_extra_allowed_path_for_no_execute, &valid_path));
}

// -----------------------------------------------------------------------------

namespace internal {

bool MoveUnsafe(const FilePath& from_path, const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  if (MoveFileEx(from_path.value().c_str(), to_path.value().c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0)
    return true;

  // Keep the last error value from MoveFileEx around in case the below
  // fails.
  bool ret = false;
  DWORD last_error = ::GetLastError();

  if (DirectoryExists(from_path)) {
    // MoveFileEx fails if moving directory across volumes. We will simulate
    // the move by using Copy and Delete. Ideally we could check whether
    // from_path and to_path are indeed in different volumes.
    ret = internal::CopyAndDeleteDirectory(from_path, to_path);
  }

  if (!ret) {
    // Leave a clue about what went wrong so that it can be (at least) picked
    // up by a PLOG entry.
    ::SetLastError(last_error);
  }

  return ret;
}

bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  if (CopyDirectory(from_path, to_path, true)) {
    if (DeletePathRecursively(from_path))
      return true;

    // Like Move, this function is not transactional, so we just
    // leave the copied bits behind if deleting from_path fails.
    // If to_path exists previously then we have already overwritten
    // it by now, we don't get better off by deleting the new bits.
  }
  return false;
}

}  // namespace internal
}  // namespace base
