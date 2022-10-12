// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_server.h"

#include <sys/stat.h>
#include <utility>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/fusebox/dbus-constants.h"

// This file provides the "business logic" half of the FuseBox server, coupled
// with the "D-Bus protocol logic" half in fusebox_service_provider.cc.

namespace fusebox {

namespace {

Server* g_server_instance = nullptr;

std::pair<std::string, bool> ResolvePrefixMap(
    fusebox::Server::PrefixMap& prefix_map,
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

// ParseResult is the type returned by ParseFileSystemURL. It is a result type
// (see https://en.wikipedia.org/wiki/Result_type), being either an error or a
// value. In this case, the error type is a base::File::Error (a numeric code)
// and the value type is the storage::FileSystemContext and the
// storage::FileSystemURL (and some other incidental fields).
struct ParseResult {
  explicit ParseResult(base::File::Error error_code_arg);
  ParseResult(scoped_refptr<storage::FileSystemContext> fs_context_arg,
              storage::FileSystemURL fs_url_arg,
              bool read_only_arg);
  ~ParseResult();

  base::File::Error error_code;
  scoped_refptr<storage::FileSystemContext> fs_context;
  storage::FileSystemURL fs_url;
  bool read_only = false;

  // is_moniker_root is used for the special case where
  // fusebox::kMonikerFileSystemURL (also known as "dummy://moniker", with no
  // trailing slash) is passed to ReadDir. There is no FileSystemURL linked to
  // that fs_url_as_string (there is no base::Token in the string), so
  // ParseFileSystemURL (which returns a valid FileSystemURL on success) must
  // return an error. However, ReadDir on "dummy://moniker" should succeed (but
  // send an empty directory listing back over D-Bus).
  bool is_moniker_root = false;
};

ParseResult::ParseResult(base::File::Error error_code_arg)
    : error_code(error_code_arg) {}

ParseResult::ParseResult(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg,
    bool read_only_arg)
    : error_code(base::File::Error::FILE_OK),
      fs_context(std::move(fs_context_arg)),
      fs_url(std::move(fs_url_arg)),
      read_only(read_only_arg) {}

ParseResult::~ParseResult() = default;

// All of the Server methods' arguments start with a FileSystemURL (as a
// string). This function parses that first argument as well as finding the
// FileSystemContext we will need to serve those methods.
ParseResult ParseFileSystemURL(fusebox::MonikerMap& moniker_map,
                               fusebox::Server::PrefixMap& prefix_map,
                               std::string fs_url_as_string) {
  scoped_refptr<storage::FileSystemContext> fs_context =
      file_manager::util::GetFileManagerFileSystemContext(
          ProfileManager::GetActiveUserProfile());
  if (fs_url_as_string.empty()) {
    LOG(ERROR) << "No FileSystemURL";
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  } else if (!fs_context) {
    LOG(ERROR) << "No FileSystemContext";
    return ParseResult(base::File::Error::FILE_ERROR_FAILED);
  }

  storage::FileSystemURL fs_url;
  bool read_only = false;

  // Intercept any moniker names and replace them by their linked target.
  using ResultType = fusebox::MonikerMap::ExtractTokenResult::ResultType;
  auto extract_token_result =
      fusebox::MonikerMap::ExtractToken(fs_url_as_string);
  switch (extract_token_result.result_type) {
    case ResultType::OK: {
      auto resolved = moniker_map.Resolve(extract_token_result.token);
      if (!resolved.first.is_valid()) {
        LOG(ERROR) << "Unresolvable Moniker";
        return ParseResult(base::File::Error::FILE_ERROR_NOT_FOUND);
      }
      fs_url = std::move(resolved.first);
      read_only = resolved.second;
      break;
    }
    case ResultType::NOT_A_MONIKER_FS_URL: {
      auto resolved = ResolvePrefixMap(prefix_map, fs_url_as_string);
      if (resolved.first.empty()) {
        LOG(ERROR) << "Unresolvable Prefix";
        return ParseResult(base::File::Error::FILE_ERROR_NOT_FOUND);
      }
      read_only = resolved.second;
      fs_url = fs_context->CrackURLInFirstPartyContext(GURL(resolved.first));
      if (!fs_url.is_valid()) {
        LOG(ERROR) << "Invalid FileSystemURL";
        return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
      }
      break;
    }
    case ResultType::MONIKER_FS_URL_BUT_ONLY_ROOT: {
      ParseResult result = ParseResult(base::File::Error::FILE_ERROR_NOT_FOUND);
      result.is_moniker_root = true;
      return result;
    }
    case ResultType::MONIKER_FS_URL_BUT_NOT_WELL_FORMED:
      return ParseResult(base::File::Error::FILE_ERROR_NOT_FOUND);
  }

  if (!fs_context->external_backend()->CanHandleType(fs_url.type())) {
    LOG(ERROR) << "Backend cannot handle "
               << storage::GetFileSystemTypeString(fs_url.type());
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  }
  return ParseResult(std::move(fs_context), std::move(fs_url), read_only);
}

// Some functions (marked with a §) below, take an fs_context argument that
// looks unused, but we need to keep the storage::FileSystemContext reference
// alive until the callbacks are run.

void RunReadCallbackFailure(Server::ReadCallback callback,
                            base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(FileErrorToErrno(error_code), nullptr, 0);
}

void RunReadCallbackTypical(
    Server::ReadCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamReader> fs_reader,
    scoped_refptr<net::IOBuffer> buffer,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (length < 0) {
    std::move(callback).Run(NetErrorToErrno(length), nullptr, 0);
  } else {
    std::move(callback).Run(0, reinterpret_cast<uint8_t*>(buffer->data()),
                            length);
  }

  auto task_runner = content::GetIOThreadTaskRunner({});
  task_runner->DeleteSoon(FROM_HERE, fs_reader.release());
  task_runner->ReleaseSoon(FROM_HERE, std::move(buffer));
}

void ReadOnIOThread(scoped_refptr<storage::FileSystemContext> fs_context,
                    storage::FileSystemURL fs_url,
                    int64_t offset,
                    int64_t length,
                    Server::ReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::unique_ptr<storage::FileStreamReader> fs_reader =
      fs_context->CreateFileStreamReader(fs_url, offset, length, base::Time());
  if (!fs_reader) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&RunReadCallbackFailure, std::move(callback),
                                  base::File::Error::FILE_ERROR_INVALID_URL));
    return;
  }

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBuffer>(length);

  // Save the pointer before we std::move fs_reader into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamReader alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamReader would get destroyed at the end of this function.
  auto* saved_fs_reader = fs_reader.get();

  auto pair = base::SplitOnceCallback(base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&RunReadCallbackTypical, std::move(callback), fs_context,
                     std::move(fs_reader), buffer)));

  int result =
      saved_fs_reader->Read(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The read was synchronous.
    std::move(pair.second).Run(result);
  }
}

void RunReadDirCallback(
    Server::ReadDirCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    uint64_t cookie,
    base::File::Error error_code,
    storage::AsyncFileUtil::EntryList entry_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox::DirEntryListProto protos;
  for (const auto& entry : entry_list) {
    bool is_directory = entry.type == filesystem::mojom::FsFileType::DIRECTORY;
    auto* proto = protos.add_entries();
    proto->set_is_directory(is_directory);
    proto->set_name(entry.name.value());
    proto->set_mode_bits(Server::MakeModeBits(is_directory, read_only));
  }

  callback.Run(cookie, FileErrorToErrno(error_code), std::move(protos),
               has_more);
}

void RunStatCallback(
    Server::StatCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    bool read_only,
    base::File::Error error_code,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(FileErrorToErrno(error_code), info, read_only);
}

std::string SubdirForTempDir(base::ScopedTempDir& scoped_temp_dir) {
  std::string basename = scoped_temp_dir.GetPath().BaseName().AsUTF8Unsafe();
  while (!basename.empty() && (basename[0] == '.')) {  // Strip leading dots.
    basename = basename.substr(1);
  }
  return base::StrCat({file_manager::util::kFuseBoxSubdirPrefixTMP, basename});
}

}  // namespace

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
  } else {
    response_.set_cookie(has_more_ ? cookie : 0);
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

fusebox::Moniker Server::CreateMoniker(storage::FileSystemURL target,
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

base::Value Server::GetDebugJSON() {
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
  return base::Value(std::move(dict));
}

void Server::Close(std::string fs_url_as_string, CloseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    std::move(callback).Run(FileErrorToErrno(common.error_code));
    return;
  }

  // Fail with an invalid operation error for now. TODO(crbug.com/1249754)
  // implement MTP device writing.
  std::move(callback).Run(ENOTSUP);
}

void Server::Open(std::string fs_url_as_string, OpenCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    std::move(callback).Run(FileErrorToErrno(common.error_code));
    return;
  }

  // Fail with an invalid operation error for now. TODO(crbug.com/1249754)
  // implement MTP device writing.
  std::move(callback).Run(ENOTSUP);
}

void Server::Read(std::string fs_url_as_string,
                  int64_t offset,
                  int32_t length,
                  ReadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    std::move(callback).Run(FileErrorToErrno(common.error_code), nullptr, 0);
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadOnIOThread, common.fs_context, common.fs_url, offset,
                     static_cast<int64_t>(length), std::move(callback)));
}

void Server::ReadDir(std::string fs_url_as_string,
                     uint64_t cookie,
                     ReadDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.is_moniker_root ||
      (common.error_code != base::File::Error::FILE_OK)) {
    constexpr bool has_more = false;
    callback.Run(cookie, FileErrorToErrno(common.error_code),
                 fusebox::DirEntryListProto(), has_more);
    return;
  }

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&RunReadDirCallback, callback, common.fs_context,
                          common.read_only, cookie));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindRepeating(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::ReadDirectory),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, std::move(outer_callback)));
}

void Server::ReadDir2(ReadDir2RequestProto request_proto,
                      ReadDir2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();
  uint64_t cookie = request_proto.has_cookie() ? request_proto.cookie() : 0;
  int32_t cancel_error_code = request_proto.has_cancel_error_code()
                                  ? request_proto.cancel_error_code()
                                  : 0;

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.is_moniker_root ||
      (common.error_code != base::File::Error::FILE_OK)) {
    ReadDir2ResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (cancel_error_code) {
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

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(&Server::OnReadDirectory,
                          weak_ptr_factory_.GetWeakPtr(), common.fs_context,
                          common.read_only, cookie));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindRepeating(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::ReadDirectory),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, std::move(outer_callback)));
}

void Server::Stat(std::string fs_url_as_string, StatCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    std::move(callback).Run(FileErrorToErrno(common.error_code),
                            base::File::Info(), false);
    return;
  }

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&RunStatCallback, std::move(callback),
                                        common.fs_context, common.read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, metadata_fields, std::move(outer_callback)));
}

void Server::ListStorages(ListStoragesRequestProto request,
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

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&Server::MakeTempDirOnWorkerThread,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void Server::MakeTempDirOnWorkerThread(MakeTempDirCallback callback) {
  base::ScopedTempDir scoped_temp_dir;
  bool create_succeeded = scoped_temp_dir.CreateUniqueTempDir();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Server::ReplyToMakeTempDir,
                     weak_ptr_factory_.GetWeakPtr(), std::move(scoped_temp_dir),
                     create_succeeded, std::move(callback)));
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
  fs_context->external_backend()->GrantFileAccessToOrigin(
      storage_key.origin(), base::FilePath(mount_name));

  storage::FileSystemURL fs_url =
      mount_points->CreateExternalFileSystemURL(storage_key, mount_name, {});
  constexpr bool read_only = false;
  RegisterFSURLPrefix(subdir, fs_url.ToGURL().spec(), read_only);

  temp_subdir_map_.insert({fusebox_file_path, std::move(scoped_temp_dir)});

  std::move(callback).Run("", fusebox_file_path,
                          underlying_file_path.AsUTF8Unsafe());
}

void Server::RemoveTempDir(std::string fusebox_file_path) {
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
    proto->set_is_directory(is_directory);
    proto->set_name(entry.name.value());
    proto->set_mode_bits(MakeModeBits(is_directory, read_only));
  }

  iter->second.has_more_ = has_more;

  if (iter->second.Reply(cookie, ReadDir2Callback())) {
    read_dir_2_map_.erase(iter);
  }
}

}  // namespace fusebox
