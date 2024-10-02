// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_server.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fusebox/fusebox_copy_to_fd.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "chrome/browser/ash/fusebox/fusebox_histograms.h"
#include "chrome/browser/ash/fusebox/fusebox_read_writer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/fusebox/dbus-constants.h"
#include "url/url_util.h"

// This file provides the "business logic" half of the FuseBox server, coupled
// with the "D-Bus protocol logic" half in fusebox_service_provider.cc.

namespace fusebox {

namespace {

Server* g_server_instance = nullptr;

template <typename CallbackType, typename ResponseProtoType>
CallbackType HistogramWrap(
    const HistogramEnumFileSystemType histogram_enum_file_system_type,
    const char* rpc_method_name,
    CallbackType callback) {
  static constexpr auto func =
      [](const char* file_system_type_name, const char* rpc_method_name,
         CallbackType wrappee, const ResponseProtoType& response) {
        const std::string histogram_name =
            base::StrCat({"FileBrowser.Fusebox.RPC.", file_system_type_name,
                          ".", rpc_method_name});

        int32_t posix_error_code =
            response.has_posix_error_code() ? response.posix_error_code() : 0;

        base::UmaHistogramEnumeration(
            histogram_name, GetHistogramEnumPosixErrorCode(posix_error_code));

        if (wrappee) {
          std::move(wrappee).Run(response);
        }
      };

  return base::BindOnce(
      func, NameForHistogramEnumFileSystemType(histogram_enum_file_system_type),
      rpc_method_name, std::move(callback));
}

bool UseTempFile(std::string_view fs_url_as_string) {
  // MTP (the protocol) does not support incremental writes. When creating an
  // MTP file (via FuseBox), we need to supply its contents as a whole. Up
  // until that transfer, spool incremental writes to a temporary file.
  return base::StartsWith(fs_url_as_string,
                          file_manager::util::kFuseBoxSubdirPrefixMTP);
}

bool UseEmptyTruncateWorkaround(std::string_view fs_url_as_string,
                                int64_t length) {
  // Not all storage::AsyncFileUtil back-ends implement the CreateFile or
  // Truncate methods. When they don't, and truncating to a zero length, work
  // around it as a RemoveFile followed by copying in an empty file.
  return (length == 0) &&
         base::StartsWith(fs_url_as_string,
                          file_manager::util::kFuseBoxSubdirPrefixMTP);
}

std::pair<std::string, bool> ResolvePrefixMap(
    const fusebox::Server::PrefixMap& prefix_map,
    const std::string& s) {
  size_t i = s.find('/');
  if (i == std::string::npos) {
    i = s.size();
  }
  auto iter = prefix_map.find(s.substr(0, i));
  if (iter == prefix_map.end()) {
    return std::make_pair("", false);
  }
  return std::make_pair(base::StrCat({iter->second.fs_url_prefix, s.substr(i)}),
                        iter->second.read_only);
}

// Parsed is the T in the base::expected<T, E> type returned by
// ParseFileSystemURL. It holds a storage::FileSystemContext, a
// storage::FileSystemURL and read-only-ness.
struct Parsed {
  Parsed(scoped_refptr<storage::FileSystemContext> fs_context_arg,
         storage::FileSystemURL fs_url_arg,
         bool read_only_arg);
  ~Parsed();

  scoped_refptr<storage::FileSystemContext> fs_context;
  const storage::FileSystemURL fs_url;
  const bool read_only;
};

Parsed::Parsed(scoped_refptr<storage::FileSystemContext> fs_context_arg,
               storage::FileSystemURL fs_url_arg,
               bool read_only_arg)
    : fs_context(std::move(fs_context_arg)),
      fs_url(std::move(fs_url_arg)),
      read_only(read_only_arg) {}

Parsed::~Parsed() = default;

struct ParseError {
  explicit ParseError(int posix_error_code_arg,
                      bool is_moniker_root_arg = false);

  const int posix_error_code;
  // is_moniker_root is used for the special case where the file_system_url is
  // fusebox::kMonikerSubdir (also known as "moniker"). There is no
  // storage::FileSystemURL registered for "moniker" (as opposed to for
  // "moniker/1234etc"), so ParseFileSystemURL (which returns a valid
  // storage::FileSystemURL on success) must return an error. However, Stat2 or
  // ReadDir2 on "moniker" should succeed (but return an empty directory).
  const bool is_moniker_root;
};

ParseError::ParseError(int posix_error_code_arg, bool is_moniker_root_arg)
    : posix_error_code(posix_error_code_arg),
      is_moniker_root(is_moniker_root_arg) {}

// All of the Server methods' take a protobuf argument. Many protobufs have a
// file_system_url field (a string). This function parses that string as a
// storage::FileSystemURL (resolving it in the context of the MonikerMap and
// PrefixMap) as well as finding the storage::FileSystemContext we will need to
// serve those methods.
base::expected<Parsed, ParseError> ParseFileSystemURL(
    const fusebox::MonikerMap& moniker_map,
    const fusebox::Server::PrefixMap& prefix_map,
    const std::string& fs_url_as_string) {
  scoped_refptr<storage::FileSystemContext> fs_context =
      file_manager::util::GetFileManagerFileSystemContext(
          ProfileManager::GetActiveUserProfile());
  if (fs_url_as_string.empty()) {
    LOG(ERROR) << "No FileSystemURL";
    return base::unexpected(ParseError(EINVAL));
  } else if (!fs_context) {
    LOG(ERROR) << "No FileSystemContext";
    return base::unexpected(ParseError(EFAULT));
  }

  // encoded is fs_url_as_string transformed such that "fsp.hash/x/y#z.txt"
  // becomes "fsp.hash/x%2Fy%23z.txt". The "#" in particular would otherwise be
  // problematic, since the conversion from string to GURL does not consider
  // the "#y.txt" part of the URL path, even though "#" is a valid character
  // for ChromeOS (Linux) file names.
  //
  // The initial "/" stays a slash, not a "%2F", since that is what
  // ResolvePrefixMap and MonikerMap::ExtractToken expects to find.
  std::string encoded;
  size_t slash = fs_url_as_string.find('/');
  if (slash == std::string::npos) {
    encoded = fs_url_as_string;
  } else {
    url::RawCanonOutputT<char> canon_output;
    url::EncodeURIComponent(fs_url_as_string.substr(slash + 1), &canon_output);
    encoded = base::StrCat(
        {fs_url_as_string.substr(0, slash + 1), canon_output.view()});
  }

  storage::FileSystemURL fs_url;
  bool read_only = false;

  // Intercept any moniker names and replace them by their linked target.
  using ResultType = fusebox::MonikerMap::ExtractTokenResult::ResultType;
  auto extract_token_result = fusebox::MonikerMap::ExtractToken(encoded);
  switch (extract_token_result.result_type) {
    case ResultType::OK: {
      auto resolved = moniker_map.Resolve(extract_token_result.token);
      if (!resolved.first.is_valid()) {
        LOG(ERROR) << "Unresolvable Moniker";
        return base::unexpected(ParseError(ENOENT));
      }
      fs_url = std::move(resolved.first);
      read_only = resolved.second;
      break;
    }
    case ResultType::NOT_A_MONIKER_FS_URL: {
      auto resolved = ResolvePrefixMap(prefix_map, encoded);
      if (resolved.first.empty()) {
        LOG(ERROR) << "Unresolvable Prefix";
        return base::unexpected(ParseError(ENOENT));
      }
      read_only = resolved.second;
      fs_url = fs_context->CrackURLInFirstPartyContext(GURL(resolved.first));
      if (!fs_url.is_valid()) {
        LOG(ERROR) << "Invalid FileSystemURL";
        return base::unexpected(ParseError(EINVAL));
      }
      break;
    }
    case ResultType::MONIKER_FS_URL_BUT_ONLY_ROOT: {
      return base::unexpected(ParseError(ENOENT, true));
    }
    case ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED:
      return base::unexpected(ParseError(ENOENT));
  }

  if (!ash::FileSystemBackend::Get(*fs_context)->CanHandleType(fs_url.type())) {
    LOG(ERROR) << "Backend cannot handle "
               << storage::GetFileSystemTypeString(fs_url.type());
    return base::unexpected(ParseError(EINVAL));
  }
  return Parsed(std::move(fs_context), std::move(fs_url), read_only);
}

// Some functions (marked with a §) below, take an fs_context argument that
// looks unused, but we need to keep the storage::FileSystemContext reference
// alive until the callbacks are run.

void FillInDirEntryProto(DirEntryProto* dir_entry_proto,
                         const base::File::Info& info,
                         bool read_only) {
  dir_entry_proto->set_mode_bits(
      Server::MakeModeBits(info.is_directory, read_only));
  // The base::File::Info comment says that info.size is "undefined when
  // info.is_directory is true".
  dir_entry_proto->set_size(info.is_directory ? 0 : info.size);
  dir_entry_proto->set_mtime(
      info.last_modified.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void RunCreateAndThenStatCallback(
    Server::CreateCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    uint64_t fuse_handle,
    base::OnceClosure on_failure,
    base::File::Error error_code,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    std::move(on_failure).Run();
    CreateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  CreateResponseProto response_proto;
  response_proto.set_fuse_handle(fuse_handle);
  FillInDirEntryProto(response_proto.mutable_stat(), info, read_only);
  std::move(callback).Run(response_proto);
}

void RunCreateCallback(
    Server::CreateCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    storage::FileSystemURL fs_url,
    bool read_only,
    uint64_t fuse_handle,
    base::OnceClosure on_failure,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    std::move(on_failure).Run();
    CreateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr storage::FileSystemOperation::GetMetadataFieldSet metadata_fields =
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified};

  auto outer_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RunCreateAndThenStatCallback, std::move(callback), fs_context, read_only,
      fuse_handle, std::move(on_failure)));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(fs_context->operation_runner()), fs_url,
          metadata_fields, std::move(outer_callback)));
}

void RunMkDirAndThenStatCallback(
    Server::MkDirCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    base::File::Error error_code,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  MkDirResponseProto response_proto;
  FillInDirEntryProto(response_proto.mutable_stat(), info, read_only);
  std::move(callback).Run(response_proto);
}

void RunMkDirCallback(
    Server::MkDirCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    storage::FileSystemURL fs_url,
    bool read_only,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr storage::FileSystemOperation::GetMetadataFieldSet metadata_fields =
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified};

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunMkDirAndThenStatCallback, std::move(callback),
                     fs_context, read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(fs_context->operation_runner()), fs_url,
          metadata_fields, std::move(outer_callback)));
}

void RunRenameCallbackPosixErrorCode(
    Server::RenameCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    int posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (posix_error_code) {
    RenameResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  RenameResponseProto response_proto;
  std::move(callback).Run(response_proto);
}

void RunRenameCallbackBaseFileError(
    Server::RenameCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    base::File::Error error_code) {
  RunRenameCallbackPosixErrorCode(std::move(callback), std::move(fs_context),
                                  FileErrorToErrno(error_code));
}

void RunRmDirCallback(
    Server::RmDirCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  RmDirResponseProto response_proto;
  std::move(callback).Run(response_proto);
}

void RunTruncateAndThenStatCallback(
    Server::TruncateCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    base::File::Error error_code,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  TruncateResponseProto response_proto;
  FillInDirEntryProto(response_proto.mutable_stat(), info, read_only);
  std::move(callback).Run(response_proto);
}

void RunTruncateCallback(
    Server::TruncateCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    storage::FileSystemURL fs_url,
    bool read_only,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr storage::FileSystemOperation::GetMetadataFieldSet metadata_fields =
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified};

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunTruncateAndThenStatCallback, std::move(callback),
                     fs_context, read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(fs_context->operation_runner()), fs_url,
          metadata_fields, std::move(outer_callback)));
}

void RunUnlinkCallback(
    Server::UnlinkCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  UnlinkResponseProto response_proto;
  std::move(callback).Run(response_proto);
}

void RunStat2Callback(
    Server::Stat2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    base::File::Error error_code,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    Stat2ResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  Stat2ResponseProto response_proto;
  FillInDirEntryProto(response_proto.mutable_stat(), info, read_only);
  std::move(callback).Run(response_proto);
}

std::string SubdirForTempDir(base::ScopedTempDir& scoped_temp_dir) {
  std::string basename = scoped_temp_dir.GetPath().BaseName().AsUTF8Unsafe();
  while (!basename.empty() && (basename[0] == '.')) {  // Strip leading dots.
    basename = basename.substr(1);
  }
  return base::StrCat({file_manager::util::kFuseBoxSubdirPrefixTMP, basename});
}

void EmptyTruncateWorkaroundCallback2(Server::TruncateCallback callback,
                                      base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  TruncateResponseProto response_proto;
  if (error_code != base::File::Error::FILE_OK) {
    response_proto.set_posix_error_code(FileErrorToErrno(error_code));
  } else {
    DirEntryProto* dir_entry_proto = response_proto.mutable_stat();
    constexpr bool is_directory = false;
    constexpr bool read_only = false;
    dir_entry_proto->set_mode_bits(
        Server::MakeModeBits(is_directory, read_only));
    dir_entry_proto->set_size(0);
    dir_entry_proto->set_mtime(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(response_proto)));
}

void EmptyTruncateWorkaroundCallback1(
    scoped_refptr<storage::FileSystemContext> fs_context,
    const storage::FileSystemURL fs_url,
    Server::TruncateCallback callback,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error_code != base::File::Error::FILE_OK) {
    EmptyTruncateWorkaroundCallback2(std::move(callback), error_code);
    return;
  }
  fs_context->operation_runner()->CopyInForeignFile(
      base::FilePath("/dev/null"), fs_url,
      base::BindOnce(&EmptyTruncateWorkaroundCallback2, std::move(callback)));
}

void DoEmptyTruncateWorkaround(
    scoped_refptr<storage::FileSystemContext> fs_context,
    const storage::FileSystemURL fs_url,
    Server::TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::RemoveFile),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(fs_context->operation_runner()), fs_url,
          base::BindOnce(&EmptyTruncateWorkaroundCallback1, fs_context, fs_url,
                         std::move(callback))));
}

void CrossFileSystemRenameCallback3(
    scoped_refptr<storage::FileSystemContext> fs_context,
    base::OnceCallback<void(int posix_error_code)> callback,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(FileErrorToErrno(error_code));
}

void CrossFileSystemRenameCallback2(
    base::ScopedFD scoped_fd,
    scoped_refptr<storage::FileSystemContext> fs_context,
    const storage::FileSystemURL src_fs_url,
    base::OnceCallback<void(int posix_error_code)> callback,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error_code != base::File::FILE_OK) {
    std::move(callback).Run(FileErrorToErrno(error_code));
    return;
  }

  fs_context->operation_runner()->RemoveFile(
      src_fs_url, base::BindOnce(&CrossFileSystemRenameCallback3, fs_context,
                                 std::move(callback)));
}

void CrossFileSystemRenameCallback1(
    scoped_refptr<storage::FileSystemContext> fs_context,
    const storage::FileSystemURL src_fs_url,
    const storage::FileSystemURL dst_fs_url,
    base::OnceCallback<void(int posix_error_code)> callback,
    base::expected<base::ScopedFD, int> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!result.has_value()) {
    std::move(callback).Run(result.error());
    return;
  }

  std::string fd_path =
      base::StringPrintf("/proc/self/fd/%d", result.value().get());

  fs_context->operation_runner()->CopyInForeignFile(
      base::FilePath(fd_path), dst_fs_url,
      base::BindOnce(&CrossFileSystemRenameCallback2, std::move(result.value()),
                     fs_context, std::move(src_fs_url), std::move(callback)));
}

// Implement a cross-file-system rename (a move) as three steps:
// 1. CopyToFileDescriptor, from src to an O_TMPFILE file.
// 2. CopyInForeignFile, from the O_TMPFILE file to dst.
// 3. RemoveFile, of the src. The base::ScopedFD destructor will delete (both
//    in the C++ sense and in the file system sense) the O_TMPFILE file.
void DoCrossFileSystemRename(
    scoped_refptr<storage::FileSystemContext> fs_context,
    std::string profile_path,
    const storage::FileSystemURL src_fs_url,
    const storage::FileSystemURL dst_fs_url,
    base::OnceCallback<void(int posix_error_code)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::ScopedFD temp_file(open(profile_path.c_str(),
                                O_CLOEXEC | O_EXCL | O_TMPFILE | O_RDWR, 0600));
  if (!temp_file.is_valid()) {
    std::move(callback).Run(ENOSPC);
    return;
  }
  CopyToFileDescriptor(
      fs_context, src_fs_url, std::move(temp_file),
      base::BindOnce(&CrossFileSystemRenameCallback1, fs_context, src_fs_url,
                     std::move(dst_fs_url), std::move(callback)));
}

}  // namespace

Server::FuseFileMapEntry::FuseFileMapEntry(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg,
    const std::string& profile_path_arg,
    bool readable_arg,
    bool writable_arg,
    bool use_temp_file_arg,
    bool temp_file_starts_with_copy_arg)
    : fs_context_(fs_context_arg),
      histogram_enum_file_system_type_(
          GetHistogramEnumFileSystemType(fs_url_arg)),
      readable_(readable_arg),
      writable_(writable_arg),
      seqbnd_read_writer_(content::GetIOThreadTaskRunner({}),
                          fs_url_arg,
                          profile_path_arg,
                          use_temp_file_arg,
                          temp_file_starts_with_copy_arg) {}

Server::FuseFileMapEntry::FuseFileMapEntry(FuseFileMapEntry&&) = default;

Server::FuseFileMapEntry::~FuseFileMapEntry() = default;

void Server::FuseFileMapEntry::DoFlush(const FlushRequestProto& request,
                                       Server::FlushCallback callback) {
  seqbnd_read_writer_.AsyncCall(&ReadWriter::Flush)
      .WithArgs(fs_context_, std::move(callback));
}

void Server::FuseFileMapEntry::DoRead2(const Read2RequestProto& request,
                                       Server::Read2Callback callback) {
  int64_t offset = request.has_offset() ? request.offset() : 0;
  int64_t length = request.has_length() ? request.length() : 0;
  seqbnd_read_writer_.AsyncCall(&ReadWriter::Read)
      .WithArgs(fs_context_, offset, length, std::move(callback));
}

void Server::FuseFileMapEntry::DoWrite2(const Write2RequestProto& request,
                                        Server::Write2Callback callback) {
  if (!request.has_data() || request.data().empty()) {
    Write2ResponseProto response_proto;
    std::move(callback).Run(response_proto);
    return;
  }
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(request.data());
  int64_t offset = request.has_offset() ? request.offset() : 0;
  seqbnd_read_writer_.AsyncCall(&ReadWriter::Write)
      .WithArgs(fs_context_, std::move(buffer), offset,
                static_cast<int>(request.data().size()), std::move(callback));
}

void Server::FuseFileMapEntry::Do(PendingOp& op,
                                  base::WeakPtr<Server> weak_ptr_server,
                                  uint64_t fuse_handle) {
  if (absl::holds_alternative<PendingFlush>(op)) {
    PendingFlush& pending = absl::get<PendingFlush>(op);
    DoFlush(pending.first,
            base::BindOnce(&Server::OnFlush, weak_ptr_server, fuse_handle,
                           std::move(pending.second)));
  } else if (absl::holds_alternative<PendingRead2>(op)) {
    PendingRead2& pending = absl::get<PendingRead2>(op);
    DoRead2(pending.first,
            base::BindOnce(&Server::OnRead2, weak_ptr_server, fuse_handle,
                           std::move(pending.second)));
  } else if (absl::holds_alternative<PendingWrite2>(op)) {
    PendingWrite2& pending = absl::get<PendingWrite2>(op);
    DoWrite2(pending.first,
             base::BindOnce(&Server::OnWrite2, weak_ptr_server, fuse_handle,
                            std::move(pending.second)));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

Server::PrefixMapEntry::PrefixMapEntry(std::string fs_url_prefix_arg,
                                       bool read_only_arg)
    : fs_url_prefix(fs_url_prefix_arg), read_only(read_only_arg) {}

Server::ReadDir2MapEntry::ReadDir2MapEntry(Server::ReadDir2Callback callback)
    : callback_(std::move(callback)) {}

Server::ReadDir2MapEntry::ReadDir2MapEntry(ReadDir2MapEntry&&) = default;

Server::ReadDir2MapEntry::~ReadDir2MapEntry() = default;

bool Server::ReadDir2MapEntry::Reply(uint64_t cookie,
                                     ReadDir2Callback callback) {
  if (callback) {
    if (callback_) {
      ReadDir2ResponseProto response_proto;
      response_proto.set_posix_error_code(EINVAL);
      std::move(callback_).Run(response_proto);
    }
    callback_ = std::move(callback);
  } else if (!callback_) {
    return false;
  }

  if (posix_error_code_ != 0) {
    response_.set_posix_error_code(posix_error_code_);
  } else if (has_more_) {
    if (response_.entries().empty()) {
      // If we have nothing of interest to say (has_more_ is true but we have
      // no entries yet) other than a non-zero cookie, there's no need to send
      // a response. Otherwise, a non-zero cookie means that the RPC client
      // will just immediately send another request, and we'll immediately send
      // another empty response, and they'll immediately send another request,
      // etc., burning CPU bouncing back and forth until we finally have some
      // entries to send (or hit an error, or has_more_ becomes false).
      return false;
    }
    response_.set_cookie(cookie);
  }
  std::move(callback_).Run(std::move(response_));
  response_ = ReadDir2ResponseProto();
  return (posix_error_code_ != 0) || !has_more_;
}

// static
Server* Server::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return g_server_instance;
}

// static
uint32_t Server::MakeModeBits(bool is_directory, bool read_only) {
  uint32_t mode_bits = is_directory
                           ? (S_IFDIR | 0110)  // 0110 are the "--x--x---" bits.
                           : S_IFREG;
  mode_bits |= read_only ? 0440 : 0660;  // "r--r-----" versus "rw-rw----".
  return mode_bits;
}

Server::Server(Delegate* delegate) : delegate_(delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_server_instance);
  g_server_instance = this;
}

Server::~Server() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(g_server_instance);
  g_server_instance = nullptr;
}

fusebox::Moniker Server::CreateMoniker(const storage::FileSystemURL& target,
                                       bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return moniker_map_.CreateMoniker(target, read_only);
}

void Server::DestroyMoniker(fusebox::Moniker moniker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  moniker_map_.DestroyMoniker(moniker);
}

void Server::RegisterFSURLPrefix(const std::string& subdir,
                                 const std::string& fs_url_prefix,
                                 bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (subdir.find('/') != std::string::npos) {
    LOG(ERROR) << "Invalid subdir: " << subdir;
    return;
  }
  std::string trimmed =
      std::string(base::TrimString(fs_url_prefix, "/", base::TRIM_TRAILING));
  prefix_map_.insert({subdir, PrefixMapEntry(trimmed, read_only)});
  if (delegate_) {
    delegate_->OnRegisterFSURLPrefix(subdir);
  }
}

void Server::UnregisterFSURLPrefix(const std::string& subdir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = prefix_map_.find(subdir);
  if (iter != prefix_map_.end()) {
    prefix_map_.erase(iter);
  }
  if (delegate_) {
    delegate_->OnUnregisterFSURLPrefix(subdir);
  }
}

storage::FileSystemURL Server::ResolveFilename(Profile* profile,
                                               const std::string& filename) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::StartsWith(filename, file_manager::util::kFuseBoxMediaSlashPath)) {
    return storage::FileSystemURL();
  }
  auto resolved = ResolvePrefixMap(
      prefix_map_,
      filename.substr(strlen(file_manager::util::kFuseBoxMediaSlashPath)));
  if (resolved.first.empty()) {
    return storage::FileSystemURL();
  }
  return file_manager::util::GetFileManagerFileSystemContext(profile)
      ->CrackURLInFirstPartyContext(GURL(resolved.first));
}

base::FilePath Server::InverseResolveFSURL(
    const storage::FileSystemURL& fs_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string fs_url_as_string = fs_url.ToGURL().spec();

  // Find the longest registered (in the "called Server::RegisterFSURLPrefix"
  // sense) FileSystemURL that is a prefix of fs_url.
  size_t best_size = 0;
  std::string_view best_subdir;
  for (const auto& i : prefix_map_) {
    if ((best_size < i.second.fs_url_prefix.size()) &&
        base::StartsWith(fs_url_as_string, i.second.fs_url_prefix)) {
      best_size = i.second.fs_url_prefix.size();
      best_subdir = i.first;
    }
  }

  if (best_size > 0) {
    const std::string relative_path = base::UnescapeURLComponent(
        fs_url_as_string.substr(best_size),
        base::UnescapeRule::SPACES |
            base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
    return storage::StringToFilePath(
        base::StrCat({file_manager::util::kFuseBoxMediaSlashPath, best_subdir,
                      relative_path}));
  }

  return base::FilePath();
}

void Server::GetDebugJSONForKey(
    std::string_view key,
    base::OnceCallback<void(JSONKeyValuePair)> callback) {
  base::Value::Dict subdirs;
  subdirs.Set(kMonikerSubdir, base::Value("[special]"));
  for (const auto& i : prefix_map_) {
    subdirs.Set(i.first,
                base::Value(base::StrCat(
                    {i.second.fs_url_prefix,
                     i.second.read_only ? " (read-only)" : " (read-write)"})));
  }

  base::Value::Dict dict;
  dict.Set("monikers", moniker_map_.GetDebugJSON());
  dict.Set("subdirs", std::move(subdirs));
  std::move(callback).Run(std::make_pair(key, base::Value(std::move(dict))));
}

void Server::Close2(const Close2RequestProto& request_proto,
                    Close2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    Close2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<Close2Callback, Close2ResponseProto>(
      iter->second.histogram_enum_file_system_type_, "Close2",
      std::move(callback));
  FuseFileMapEntry& entry = iter->second;
  base::circular_deque<PendingOp> pending_ops = std::move(entry.pending_ops_);
  entry.seqbnd_read_writer_.AsyncCall(&ReadWriter::Close)
      .WithArgs(entry.fs_context_, std::move(callback));

  fuse_file_map_.erase(iter);

  for (auto& pending_op : pending_ops) {
    if (absl::holds_alternative<PendingRead2>(pending_op)) {
      Read2ResponseProto read2_response_proto;
      read2_response_proto.set_posix_error_code(EBUSY);
      std::move(absl::get<PendingRead2>(pending_op).second)
          .Run(read2_response_proto);
    } else if (absl::holds_alternative<PendingWrite2>(pending_op)) {
      Write2ResponseProto write2_response_proto;
      write2_response_proto.set_posix_error_code(EBUSY);
      std::move(absl::get<PendingWrite2>(pending_op).second)
          .Run(write2_response_proto);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

void Server::Create(const CreateRequestProto& request_proto,
                    CreateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    CreateResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<CreateCallback, CreateResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "Create",
      std::move(callback));
  if (parsed->read_only) {
    CreateResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr bool readable = true;
  constexpr bool writable = true;
  bool use_temp_file = writable && UseTempFile(fs_url_as_string);

  uint64_t fuse_handle = InsertFuseFileMapEntry(FuseFileMapEntry(
      parsed->fs_context, parsed->fs_url,
      use_temp_file
          ? ProfileManager::GetActiveUserProfile()->GetPath().AsUTF8Unsafe()
          : std::string(),
      readable, writable, use_temp_file, false));

  if (use_temp_file) {
    base::Time now = base::Time::Now();
    base::File::Info info;
    info.last_modified = now;
    info.last_accessed = now;
    info.creation_time = now;
    CreateResponseProto response_proto;
    response_proto.set_fuse_handle(fuse_handle);
    FillInDirEntryProto(response_proto.mutable_stat(), info, parsed->read_only);
    std::move(callback).Run(response_proto);
    return;
  }

  auto on_failure = base::BindOnce(&Server::EraseFuseFileMapEntry,
                                   weak_ptr_factory_.GetWeakPtr(), fuse_handle);

  auto outer_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RunCreateCallback, std::move(callback), parsed->fs_context,
      parsed->fs_url, parsed->read_only, fuse_handle, std::move(on_failure)));

  constexpr bool exclusive = true;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::CreateFile),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, exclusive, std::move(outer_callback)));
}

void Server::Flush(const FlushRequestProto& request_proto,
                   FlushCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    FlushResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<FlushCallback, FlushResponseProto>(
      iter->second.histogram_enum_file_system_type_, "Flush",
      std::move(callback));
  if (!iter->second.writable_) {
    FlushResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  } else if (iter->second.has_in_flight_op_) {
    iter->second.pending_ops_.emplace_back(
        PendingFlush(request_proto, std::move(callback)));
    return;
  }
  iter->second.has_in_flight_op_ = true;
  iter->second.DoFlush(
      request_proto,
      base::BindOnce(&Server::OnFlush, weak_ptr_factory_.GetWeakPtr(),
                     fuse_handle, std::move(callback)));
}

void Server::MkDir(const MkDirRequestProto& request_proto,
                   MkDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<MkDirCallback, MkDirResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "MkDir",
      std::move(callback));
  if (parsed->read_only) {
    MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunMkDirCallback, std::move(callback), parsed->fs_context,
                     parsed->fs_url, parsed->read_only));

  constexpr bool exclusive = true;
  constexpr bool recursive = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::CreateDirectory),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, exclusive, recursive, std::move(outer_callback)));
}

void Server::Open2(const Open2RequestProto& request_proto,
                   Open2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();
  AccessMode access_mode = request_proto.has_access_mode()
                               ? request_proto.access_mode()
                               : AccessMode::NO_ACCESS;

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    Open2ResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<Open2Callback, Open2ResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "Open2",
      std::move(callback));

  bool readable = (access_mode == AccessMode::READ_ONLY) ||
                  (access_mode == AccessMode::READ_WRITE);
  bool writable =
      !parsed->read_only && ((access_mode == AccessMode::WRITE_ONLY) ||
                             (access_mode == AccessMode::READ_WRITE));
  bool use_temp_file = writable && UseTempFile(fs_url_as_string);

  uint64_t fuse_handle = InsertFuseFileMapEntry(FuseFileMapEntry(
      std::move(parsed->fs_context), parsed->fs_url,
      use_temp_file
          ? ProfileManager::GetActiveUserProfile()->GetPath().AsUTF8Unsafe()
          : std::string(),
      readable, writable, use_temp_file, true));

  Open2ResponseProto response_proto;
  response_proto.set_fuse_handle(fuse_handle);
  std::move(callback).Run(response_proto);
}

void Server::Read2(const Read2RequestProto& request_proto,
                   Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    Read2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<Read2Callback, Read2ResponseProto>(
      iter->second.histogram_enum_file_system_type_, "Read2",
      std::move(callback));
  if (!iter->second.readable_) {
    Read2ResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  } else if (iter->second.has_in_flight_op_) {
    iter->second.pending_ops_.emplace_back(
        PendingRead2(request_proto, std::move(callback)));
    return;
  }
  iter->second.has_in_flight_op_ = true;
  iter->second.DoRead2(
      request_proto,
      base::BindOnce(&Server::OnRead2, weak_ptr_factory_.GetWeakPtr(),
                     fuse_handle, std::move(callback)));
}

void Server::ReadDir2(const ReadDir2RequestProto& request_proto,
                      ReadDir2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();
  uint64_t cookie = request_proto.has_cookie() ? request_proto.cookie() : 0;
  int32_t cancel_error_code = request_proto.has_cancel_error_code()
                                  ? request_proto.cancel_error_code()
                                  : 0;

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    ReadDir2ResponseProto response_proto;
    if (parsed.error().is_moniker_root) {
      response_proto.set_posix_error_code(0);
    } else {
      response_proto.set_posix_error_code(parsed.error().posix_error_code);
    }
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<ReadDir2Callback, ReadDir2ResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "ReadDir2",
      std::move(callback));
  if (cancel_error_code) {
    ReadDir2ResponseProto response_proto;
    response_proto.set_posix_error_code(cancel_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  if (cookie) {
    auto iter = read_dir_2_map_.find(cookie);
    if (iter == read_dir_2_map_.end()) {
      ReadDir2ResponseProto response_proto;
      response_proto.set_posix_error_code(EINVAL);
      std::move(callback).Run(response_proto);
    } else if (iter->second.Reply(cookie, std::move(callback))) {
      read_dir_2_map_.erase(iter);
    }
    return;
  }

  static uint64_t next_cookie = 0;
  cookie = ++next_cookie;
  read_dir_2_map_.insert({cookie, ReadDir2MapEntry(std::move(callback))});

  auto outer_callback = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &Server::OnReadDirectory, weak_ptr_factory_.GetWeakPtr(),
      parsed->fs_context, parsed->read_only, cookie));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindRepeating(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::ReadDirectory),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, std::move(outer_callback)));
}

void Server::Rename(const RenameRequestProto& request_proto,
                    RenameCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string src_fs_url_as_string = request_proto.has_src_file_system_url()
                                         ? request_proto.src_file_system_url()
                                         : std::string();
  std::string dst_fs_url_as_string = request_proto.has_dst_file_system_url()
                                         ? request_proto.dst_file_system_url()
                                         : std::string();
  auto src_parsed =
      ParseFileSystemURL(moniker_map_, prefix_map_, src_fs_url_as_string);
  if (!src_parsed.has_value()) {
    RenameResponseProto response_proto;
    response_proto.set_posix_error_code(src_parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<RenameCallback, RenameResponseProto>(
      GetHistogramEnumFileSystemType(src_parsed->fs_url), "Rename",
      std::move(callback));
  if (src_parsed->read_only) {
    RenameResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto dst_parsed =
      ParseFileSystemURL(moniker_map_, prefix_map_, dst_fs_url_as_string);
  if (!dst_parsed.has_value()) {
    RenameResponseProto response_proto;
    response_proto.set_posix_error_code(dst_parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  } else if (dst_parsed->read_only) {
    RenameResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  // Use a temporary file (and CopyInForeignFile), for cross-file-system moves
  // where the destination file system doesn't support incremental writes, but
  // both source and destination are on Fusebox-served subdirs.
  //
  // Otherwise, the storage::FileSystemOperationRunner::Move call further below
  // would require the various backends to support either src
  // CreateSnapshotFile and dst CopyInForeignFile (when using
  // storage::SnapshotCopyOrMoveImpl) or src CreateFileStreamReader and dst
  // CreateFileStreamWriter (when using storage::StreamCopyOrMoveImpl).
  //
  // Care is needed to pick the right approach, based on both source and
  // destination file system types, as many backends subclass (with stub
  // methods to satisfy the C++ compiler) but don't completely satisfy the
  // storage::AsyncFileUtil or ash::FileSystemBackendDelegate interfaces.
  //
  // As of March 2023, these below are all unimplemented, often but not always
  // marked NOTIMPLEMENTED, NOTREACHED, or TODO:
  //   - ArcDocumentsProviderAsyncFileUtil::CopyInForeignFile
  //   - ArcDocumentsProviderAsyncFileUtil::CreateSnapshotFile
  //   - MTPFileSystemBackendDelegate::CreateFileStreamWriter
  //   - ProviderAsyncFileUtil::CopyInForeignFile (*)
  //   - ProviderAsyncFileUtil::CreateSnapshotFile
  //
  // (*) ProviderAsyncFileUtil::CopyInForeignFile was added in August 2023.
  if (!src_parsed->fs_url.IsInSameFileSystem(dst_parsed->fs_url) &&
      UseTempFile(dst_fs_url_as_string)) {
    auto outer_callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&RunRenameCallbackPosixErrorCode, std::move(callback),
                       src_parsed->fs_context));

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DoCrossFileSystemRename, src_parsed->fs_context,
                       std::string(ProfileManager::GetActiveUserProfile()
                                       ->GetPath()
                                       .AsUTF8Unsafe()),
                       std::move(src_parsed->fs_url),
                       std::move(dst_parsed->fs_url),
                       std::move(outer_callback)));
    return;
  }

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunRenameCallbackBaseFileError, std::move(callback),
                     src_parsed->fs_context));

  constexpr storage::FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified,
      storage::FileSystemOperation::CopyOrMoveOption::
          kRemovePartiallyCopiedFilesOnError};

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::Move),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(src_parsed->fs_context->operation_runner()),
          src_parsed->fs_url, dst_parsed->fs_url, options,
          storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
          std::make_unique<storage::CopyOrMoveHookDelegate>(),
          std::move(outer_callback)));
}

void Server::RmDir(const RmDirRequestProto& request_proto,
                   RmDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<RmDirCallback, RmDirResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "RmDir",
      std::move(callback));
  if (parsed->read_only) {
    RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RunRmDirCallback, std::move(callback), parsed->fs_context));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::RemoveDirectory),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, std::move(outer_callback)));
}

void Server::Stat2(const Stat2RequestProto& request_proto,
                   Stat2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    Stat2ResponseProto response_proto;
    if (parsed.error().is_moniker_root) {
      DirEntryProto* stat = response_proto.mutable_stat();
      constexpr bool is_directory = true;
      constexpr bool read_only = true;
      stat->set_mode_bits(Server::MakeModeBits(is_directory, read_only));
    } else {
      response_proto.set_posix_error_code(parsed.error().posix_error_code);
    }
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<Stat2Callback, Stat2ResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "Stat2",
      std::move(callback));

  constexpr storage::FileSystemOperation::GetMetadataFieldSet metadata_fields =
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified};

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunStat2Callback, std::move(callback), parsed->fs_context,
                     parsed->read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, metadata_fields, std::move(outer_callback)));
}

void Server::Truncate(const TruncateRequestProto& request_proto,
                      TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<TruncateCallback, TruncateResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "Truncate",
      std::move(callback));
  if (parsed->read_only) {
    TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  int64_t length = request_proto.has_length() ? request_proto.length() : 0;
  if (UseEmptyTruncateWorkaround(fs_url_as_string, length)) {
    DoEmptyTruncateWorkaround(std::move(parsed->fs_context),
                              std::move(parsed->fs_url), std::move(callback));
    return;
  }

  auto outer_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RunTruncateCallback, std::move(callback),
                     parsed->fs_context, parsed->fs_url, parsed->read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::Truncate),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, length, std::move(outer_callback)));
}

void Server::Unlink(const UnlinkRequestProto& request_proto,
                    UnlinkCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto parsed = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (!parsed.has_value()) {
    UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(parsed.error().posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<UnlinkCallback, UnlinkResponseProto>(
      GetHistogramEnumFileSystemType(parsed->fs_url), "Unlink",
      std::move(callback));
  if (parsed->read_only) {
    UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RunUnlinkCallback, std::move(callback), parsed->fs_context));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::RemoveFile),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, std::move(outer_callback)));
}

void Server::Write2(const Write2RequestProto& request_proto,
                    Write2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  }
  callback = HistogramWrap<Write2Callback, Write2ResponseProto>(
      iter->second.histogram_enum_file_system_type_, "Write2",
      std::move(callback));
  if (!iter->second.writable_) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  } else if (request_proto.has_data() &&
             (request_proto.data().size() > INT_MAX)) {
    Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EMSGSIZE);
    std::move(callback).Run(response_proto);
    return;
  } else if (iter->second.has_in_flight_op_) {
    iter->second.pending_ops_.emplace_back(
        PendingWrite2(request_proto, std::move(callback)));
    return;
  }
  iter->second.has_in_flight_op_ = true;
  iter->second.DoWrite2(
      request_proto,
      base::BindOnce(&Server::OnWrite2, weak_ptr_factory_.GetWeakPtr(),
                     fuse_handle, std::move(callback)));
}

void Server::ListStorages(const ListStoragesRequestProto& request,
                          ListStoragesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ListStoragesResponseProto response;
  response.add_storages(kMonikerSubdir);
  for (const auto& i : prefix_map_) {
    response.add_storages(i.first);
  }
  std::move(callback).Run(response);
}

void Server::MakeTempDir(MakeTempDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr auto make_temp_dir_on_worker_thread =
      [](base::WeakPtr<Server> weak_ptr_server, MakeTempDirCallback callback) {
        base::ScopedTempDir scoped_temp_dir;
        bool create_succeeded = scoped_temp_dir.CreateUniqueTempDir();
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&Server::ReplyToMakeTempDir,
                                      std::move(weak_ptr_server),
                                      std::move(scoped_temp_dir),
                                      create_succeeded, std::move(callback)));
      };

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(make_temp_dir_on_worker_thread,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Server::ReplyToMakeTempDir(base::ScopedTempDir scoped_temp_dir,
                                bool create_succeeded,
                                MakeTempDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!create_succeeded) {
    std::move(callback).Run("CreateUniqueTempDir failed", "", "");
    return;
  }

  const std::string subdir = SubdirForTempDir(scoped_temp_dir);
  const std::string mount_name =
      base::StrCat({file_manager::util::kFuseBoxMountNamePrefix, subdir});
  const std::string fusebox_file_path =
      base::StrCat({file_manager::util::kFuseBoxMediaSlashPath, subdir});
  const base::FilePath underlying_file_path = scoped_temp_dir.GetPath();

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  if (!mount_points->RegisterFileSystem(
          mount_name, storage::kFileSystemTypeLocal,
          storage::FileSystemMountOption(), underlying_file_path)) {
    std::move(callback).Run("RegisterFileSystem failed", "", "");
    return;
  }

  scoped_refptr<storage::FileSystemContext> fs_context =
      file_manager::util::GetFileManagerFileSystemContext(
          ProfileManager::GetActiveUserProfile());
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(
          "http://fusebox-server.example.com");
  ash::FileSystemBackend::Get(*fs_context)
      ->GrantFileAccessToOrigin(storage_key.origin(),
                                base::FilePath(mount_name));

  storage::FileSystemURL fs_url =
      mount_points->CreateExternalFileSystemURL(storage_key, mount_name, {});
  constexpr bool read_only = false;
  RegisterFSURLPrefix(subdir, fs_url.ToGURL().spec(), read_only);

  temp_subdir_map_.insert({fusebox_file_path, std::move(scoped_temp_dir)});

  std::move(callback).Run("", fusebox_file_path,
                          underlying_file_path.AsUTF8Unsafe());
}

void Server::RemoveTempDir(const std::string& fusebox_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = temp_subdir_map_.find(fusebox_file_path);
  if (iter == temp_subdir_map_.end()) {
    return;
  }
  base::ScopedTempDir scoped_temp_dir = std::move(iter->second);
  const std::string subdir = SubdirForTempDir(scoped_temp_dir);
  const std::string mount_name =
      base::StrCat({file_manager::util::kFuseBoxMountNamePrefix, subdir});
  temp_subdir_map_.erase(iter);
  UnregisterFSURLPrefix(subdir);
  storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      mount_name);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](base::ScopedTempDir) {
            // No-op other than running the base::ScopedTempDir destructor.
          },
          std::move(scoped_temp_dir)));
}

void Server::OnFlush(uint64_t fuse_handle,
                     FlushCallback callback,
                     const FlushResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    FlushResponseProto enoent_response_proto;
    enoent_response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(enoent_response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  entry.has_in_flight_op_ = false;

  std::move(callback).Run(std::move(response_proto));

  if (entry.pending_ops_.empty()) {
    return;
  }
  PendingOp pending_op = std::move(entry.pending_ops_.front());
  entry.pending_ops_.pop_front();
  entry.has_in_flight_op_ = true;
  entry.Do(pending_op, weak_ptr_factory_.GetWeakPtr(), fuse_handle);
}

void Server::OnRead2(uint64_t fuse_handle,
                     Read2Callback callback,
                     const Read2ResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    Read2ResponseProto enoent_response_proto;
    enoent_response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(enoent_response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  entry.has_in_flight_op_ = false;

  std::move(callback).Run(std::move(response_proto));

  if (entry.pending_ops_.empty()) {
    return;
  }
  PendingOp pending_op = std::move(entry.pending_ops_.front());
  entry.pending_ops_.pop_front();
  entry.has_in_flight_op_ = true;
  entry.Do(pending_op, weak_ptr_factory_.GetWeakPtr(), fuse_handle);
}

void Server::OnReadDirectory(
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    uint64_t cookie,
    base::File::Error error_code,
    storage::AsyncFileUtil::EntryList entry_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = read_dir_2_map_.find(cookie);
  if (iter == read_dir_2_map_.end()) {
    return;
  }

  if (iter->second.posix_error_code_ == 0) {
    iter->second.posix_error_code_ = FileErrorToErrno(error_code);
  }

  for (const auto& entry : entry_list) {
    bool is_directory = entry.type == filesystem::mojom::FsFileType::DIRECTORY;
    auto* proto = iter->second.response_.add_entries();
    proto->set_name(entry.name.value());
    proto->set_mode_bits(MakeModeBits(is_directory, read_only));
  }

  iter->second.has_more_ = has_more;

  if (iter->second.Reply(cookie, ReadDir2Callback())) {
    read_dir_2_map_.erase(iter);
  }
}

void Server::OnWrite2(uint64_t fuse_handle,
                      Write2Callback callback,
                      const Write2ResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    Write2ResponseProto enoent_response_proto;
    enoent_response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(enoent_response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  entry.has_in_flight_op_ = false;

  std::move(callback).Run(std::move(response_proto));

  if (entry.pending_ops_.empty()) {
    return;
  }
  PendingOp pending_op = std::move(entry.pending_ops_.front());
  entry.pending_ops_.pop_front();
  entry.has_in_flight_op_ = true;
  entry.Do(pending_op, weak_ptr_factory_.GetWeakPtr(), fuse_handle);
}

void Server::EraseFuseFileMapEntry(uint64_t fuse_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fuse_file_map_.erase(fuse_handle);
}

uint64_t Server::InsertFuseFileMapEntry(FuseFileMapEntry&& entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  static uint64_t next_fuse_handle = 0;
  uint64_t fuse_handle = ++next_fuse_handle;
  // As the fusebox.proto comment says, "The high bit (also known as the 1<<63
  // bit) is also always zero for valid values".
  DCHECK((fuse_handle >> 63) == 0);

  fuse_file_map_.insert({fuse_handle, std::move(entry)});
  return fuse_handle;
}

}  // namespace fusebox
