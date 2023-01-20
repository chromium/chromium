// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fusebox/fusebox_server.h"

#include <sys/stat.h>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "chrome/browser/ash/fusebox/fusebox_read_writer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/fusebox/dbus-constants.h"

// This file provides the "business logic" half of the FuseBox server, coupled
// with the "D-Bus protocol logic" half in fusebox_service_provider.cc.

namespace fusebox {

namespace {

Server* g_server_instance = nullptr;

bool UseTempFile(const std::string fs_url_as_string) {
  // MTP (the protocol) does not support incremental writes. When creating an
  // MTP file (via FuseBox), we need to supply its contents as a whole. Up
  // until that transfer, spool incremental writes to a temporary file.
  return base::StartsWith(fs_url_as_string,
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
        return base::unexpected(ParseError(ENOENT));
      }
      fs_url = std::move(resolved.first);
      read_only = resolved.second;
      break;
    }
    case ResultType::NOT_A_MONIKER_FS_URL: {
      auto resolved = ResolvePrefixMap(prefix_map, fs_url_as_string);
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

  if (!fs_context->external_backend()->CanHandleType(fs_url.type())) {
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
  dir_entry_proto->set_size(info.size);
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

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RunCreateAndThenStatCallback, std::move(callback),
                     fs_context, read_only, fuse_handle,
                     std::move(on_failure)));

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

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
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

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
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

}  // namespace

Server::FuseFileMapEntry::FuseFileMapEntry(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg,
    const std::string& profile_path_arg,
    bool readable_arg,
    bool writable_arg,
    bool use_temp_file_arg)
    : fs_context_(fs_context_arg),
      readable_(readable_arg),
      writable_(writable_arg),
      seqbnd_read_writer_(content::GetIOThreadTaskRunner({}),
                          fs_url_arg,
                          profile_path_arg,
                          use_temp_file_arg) {}

Server::FuseFileMapEntry::FuseFileMapEntry(FuseFileMapEntry&&) = default;

Server::FuseFileMapEntry::~FuseFileMapEntry() = default;

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
  if (absl::holds_alternative<PendingRead2>(op)) {
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
    NOTREACHED();
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
      NOTREACHED();
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
  } else if (parsed->read_only) {
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
      readable, writable, use_temp_file));

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

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RunCreateCallback, std::move(callback),
                     parsed->fs_context, parsed->fs_url, parsed->read_only,
                     fuse_handle, std::move(on_failure)));

  constexpr bool exclusive = true;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::CreateFile),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, exclusive, std::move(outer_callback)));
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
  } else if (parsed->read_only) {
    MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
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

  bool readable = (access_mode == AccessMode::READ_ONLY) ||
                  (access_mode == AccessMode::READ_WRITE);
  bool writable =
      !parsed->read_only && ((access_mode == AccessMode::WRITE_ONLY) ||
                             (access_mode == AccessMode::READ_WRITE));
  bool use_temp_file = writable && UseTempFile(fs_url_as_string);
  if (use_temp_file) {
    // TODO(b/255703917): allow use_temp_file when modifying existing files,
    // not just creating new ones.
    Open2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOTSUP);
    std::move(callback).Run(response_proto);
    return;
  }

  uint64_t fuse_handle = InsertFuseFileMapEntry(
      FuseFileMapEntry(std::move(parsed->fs_context), std::move(parsed->fs_url),
                       std::string(), readable, writable, use_temp_file));

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
  } else if (!iter->second.readable_) {
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
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&Server::OnReadDirectory,
                          weak_ptr_factory_.GetWeakPtr(), parsed->fs_context,
                          parsed->read_only, cookie));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindRepeating(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::ReadDirectory),
          // Unretained is safe: fs_context owns its operation_runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url, std::move(outer_callback)));
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
  } else if (parsed->read_only) {
    RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&RunRmDirCallback, std::move(callback),
                                        parsed->fs_context));

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

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&RunStat2Callback, std::move(callback),
                                        parsed->fs_context, parsed->read_only));

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
  } else if (parsed->read_only) {
    TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RunTruncateCallback, std::move(callback),
                     parsed->fs_context, parsed->fs_url, parsed->read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::Truncate),
          // Unretained is safe: fs_context owns its operation runner.
          base::Unretained(parsed->fs_context->operation_runner()),
          parsed->fs_url,
          request_proto.has_length() ? request_proto.length() : 0,
          std::move(outer_callback)));
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
  } else if (parsed->read_only) {
    UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&RunUnlinkCallback, std::move(callback),
                                        parsed->fs_context));

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
  } else if (!iter->second.writable_) {
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
