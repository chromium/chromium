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
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_errno.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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

  // is_moniker_root is used for the special case where the server is passed
  // fusebox::kMonikerSubdir (also known as "moniker"). There is no
  // FileSystemURL registered for "moniker" (as opposed to for
  // "moniker/1234etc"), so ParseFileSystemURL (which returns a valid
  // FileSystemURL on success) must return an error. However, Stat2 or ReadDir2
  // on "moniker" should succeed (but return an empty directory).
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
ParseResult ParseFileSystemURL(const fusebox::MonikerMap& moniker_map,
                               const fusebox::Server::PrefixMap& prefix_map,
                               const std::string& fs_url_as_string) {
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

void FillInDirEntryProto(fusebox_staging::DirEntryProto* dir_entry_proto,
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
    fusebox_staging::CreateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::CreateResponseProto response_proto;
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
    fusebox_staging::CreateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
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
    fusebox_staging::MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::MkDirResponseProto response_proto;
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
    fusebox_staging::MkDirResponseProto response_proto;
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

void RunRead2CallbackFailure(Server::Read2Callback callback,
                             base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox_staging::Read2ResponseProto response_proto;
  response_proto.set_posix_error_code(FileErrorToErrno(error_code));
  std::move(callback).Run(response_proto);
}

void RunRead2CallbackTypical(Server::Read2Callback callback,
                             scoped_refptr<net::IOBuffer> buffer,
                             int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox_staging::Read2ResponseProto response_proto;
  if (length < 0) {
    response_proto.set_posix_error_code(NetErrorToErrno(length));
  } else {
    *response_proto.mutable_data() = std::string(buffer->data(), length);
  }
  std::move(callback).Run(response_proto);

  content::GetIOThreadTaskRunner({})->ReleaseSoon(FROM_HERE, std::move(buffer));
}

void RunRmDirCallback(
    Server::RmDirCallback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int posix_error_code = FileErrorToErrno(error_code);
  if (posix_error_code) {
    fusebox_staging::RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::RmDirResponseProto response_proto;
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
    fusebox_staging::TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::TruncateResponseProto response_proto;
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
    fusebox_staging::TruncateResponseProto response_proto;
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
    fusebox_staging::UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::UnlinkResponseProto response_proto;
  std::move(callback).Run(response_proto);
}

void RunWrite2CallbackFailure(Server::Write2Callback callback,
                              base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox_staging::Write2ResponseProto response_proto;
  response_proto.set_posix_error_code(FileErrorToErrno(error_code));
  std::move(callback).Run(response_proto);
}

void RunWrite2CallbackTypical(Server::Write2Callback callback, int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox_staging::Write2ResponseProto response_proto;
  if (length < 0) {
    response_proto.set_posix_error_code(NetErrorToErrno(length));
  }
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
    fusebox_staging::Stat2ResponseProto response_proto;
    response_proto.set_posix_error_code(posix_error_code);
    std::move(callback).Run(response_proto);
    return;
  }

  fusebox_staging::Stat2ResponseProto response_proto;
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

Server::ReadWriter::ReadWriter(const storage::FileSystemURL& fs_url)
    : fs_url_(fs_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

Server::ReadWriter::~ReadWriter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void Server::ReadWriter::Read(
    scoped_refptr<storage::FileSystemContext> fs_context,
    int64_t offset,
    int64_t length,
    Server::Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // See if we can re-use the previous storage::FileStreamReader.
  std::unique_ptr<storage::FileStreamReader> fs_reader;
  if (fs_reader_ && (read_offset_ == offset)) {
    fs_reader = std::move(fs_reader_);
    read_offset_ = -1;
  } else {
    fs_reader = fs_context->CreateFileStreamReader(fs_url_, offset, INT64_MAX,
                                                   base::Time());
    if (!fs_reader) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&RunRead2CallbackFailure, std::move(callback),
                         base::File::Error::FILE_ERROR_INVALID_URL));
      return;
    }
  }

  constexpr int64_t min_length = 256;
  constexpr int64_t max_length = 262144;  // 256 KiB.
  scoped_refptr<net::IOBuffer> buffer = base::MakeRefCounted<net::IOBuffer>(
      std::max(min_length, std::min(max_length, length)));

  // Save the pointer before we std::move fs_reader into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamReader alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamReader would get destroyed at the end of this function.
  auto* saved_fs_reader = fs_reader.get();

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &Server::ReadWriter::OnRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), fs_context, std::move(fs_reader), buffer, offset));

  int result =
      saved_fs_reader->Read(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The read was synchronous.
    std::move(pair.second).Run(result);
  }
}

void Server::ReadWriter::OnRead(
    Server::Read2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,  // See § above.
    std::unique_ptr<storage::FileStreamReader> fs_reader,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (length >= 0) {
    fs_reader_ = std::move(fs_reader);
    read_offset_ = offset + length;
  } else {
    fs_reader_.reset();
    read_offset_ = -1;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RunRead2CallbackTypical, std::move(callback),
                                std::move(buffer), length));
}

void Server::ReadWriter::Write(
    scoped_refptr<storage::FileSystemContext> fs_context,
    scoped_refptr<net::StringIOBuffer> buffer,
    int64_t offset,
    int length,
    Server::Write2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // See if we can re-use the previous storage::FileStreamWriter.
  std::unique_ptr<storage::FileStreamWriter> fs_writer;
  if (fs_writer_ && (write_offset_ == offset)) {
    fs_writer = std::move(fs_writer_);
    write_offset_ = -1;
  } else {
    fs_writer = fs_context->CreateFileStreamWriter(fs_url_, offset);
    if (!fs_writer) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&RunWrite2CallbackFailure, std::move(callback),
                         base::File::Error::FILE_ERROR_INVALID_URL));
      return;
    }
  }

  // Save the pointer before we std::move fs_writer into a base::OnceCallback.
  // The std::move keeps the underlying storage::FileStreamWriter alive while
  // any network I/O is pending. Without the std::move, the underlying
  // storage::FileStreamWriter would get destroyed at the end of this function.
  auto* saved_fs_writer = fs_writer.get();

  auto pair = base::SplitOnceCallback(base::BindOnce(
      &Server::ReadWriter::OnWrite, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), fs_context, std::move(fs_writer), buffer, offset));

  int result =
      saved_fs_writer->Write(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The write was synchronous.
    std::move(pair.second).Run(result);
  }
}

void Server::ReadWriter::OnWrite(
    Server::Write2Callback callback,
    scoped_refptr<storage::FileSystemContext> fs_context,
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (length >= 0) {
    fs_writer_ = std::move(fs_writer);
    write_offset_ = offset + length;
  } else {
    fs_writer_.reset();
    write_offset_ = -1;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RunWrite2CallbackTypical, std::move(callback), length));
}

Server::FuseFileMapEntry::FuseFileMapEntry(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg,
    bool readable_arg,
    bool writable_arg)
    : fs_context_(fs_context_arg),
      readable_(readable_arg),
      writable_(writable_arg),
      seqbnd_read_writer_(content::GetIOThreadTaskRunner({}), fs_url_arg) {}

Server::FuseFileMapEntry::FuseFileMapEntry(FuseFileMapEntry&&) = default;

Server::FuseFileMapEntry::~FuseFileMapEntry() = default;

void Server::FuseFileMapEntry::DoRead2(
    const fusebox_staging::Read2RequestProto& request,
    Server::Read2Callback callback) {
  int64_t offset = request.has_offset() ? request.offset() : 0;
  int64_t length = request.has_length() ? request.length() : 0;
  seqbnd_read_writer_.AsyncCall(&Server::ReadWriter::Read)
      .WithArgs(fs_context_, offset, length, std::move(callback));
}

void Server::FuseFileMapEntry::DoWrite2(
    const fusebox_staging::Write2RequestProto& request,
    Server::Write2Callback callback) {
  if (!request.has_data() || request.data().empty()) {
    fusebox_staging::Write2ResponseProto response_proto;
    std::move(callback).Run(response_proto);
    return;
  }
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(request.data());
  int64_t offset = request.has_offset() ? request.offset() : 0;
  seqbnd_read_writer_.AsyncCall(&Server::ReadWriter::Write)
      .WithArgs(fs_context_, std::move(buffer), offset,
                static_cast<int>(request.data().size()), std::move(callback));
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

void Server::Close2(const fusebox_staging::Close2RequestProto& request_proto,
                    Close2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    fusebox_staging::Close2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  base::circular_deque<PendingRead2> pending_reads =
      std::move(entry.pending_reads_);
  base::circular_deque<PendingWrite2> pending_writes =
      std::move(entry.pending_writes_);

  fuse_file_map_.erase(iter);

  fusebox_staging::Close2ResponseProto response_proto;
  std::move(callback).Run(response_proto);

  if (!pending_reads.empty()) {
    fusebox_staging::Read2ResponseProto read2_response_proto;
    read2_response_proto.set_posix_error_code(EBUSY);
    for (auto& pending_read : pending_reads) {
      std::move(pending_read.second).Run(read2_response_proto);
    }
  }
  if (!pending_writes.empty()) {
    fusebox_staging::Write2ResponseProto write2_esponse_proto;
    write2_esponse_proto.set_posix_error_code(EBUSY);
    for (auto& pending_read : pending_writes) {
      std::move(pending_read.second).Run(write2_esponse_proto);
    }
  }
}

void Server::Create(const fusebox_staging::CreateRequestProto& request_proto,
                    CreateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::CreateResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.read_only) {
    fusebox_staging::CreateResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  constexpr bool readable = true;
  constexpr bool writable = true;

  uint64_t fuse_handle = InsertFuseFileMapEntry(
      FuseFileMapEntry(common.fs_context, common.fs_url, readable, writable));

  auto on_failure = base::BindOnce(&Server::EraseFuseFileMapEntry,
                                   weak_ptr_factory_.GetWeakPtr(), fuse_handle);

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&RunCreateCallback, std::move(callback), common.fs_context,
                     common.fs_url, common.read_only, fuse_handle,
                     std::move(on_failure)));

  constexpr bool exclusive = true;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::CreateFile),
          // Unretained is safe: context owns operation runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, exclusive, std::move(outer_callback)));
}

void Server::MkDir(const fusebox_staging::MkDirRequestProto& request_proto,
                   MkDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.read_only) {
    fusebox_staging::MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RunMkDirCallback, std::move(callback), common.fs_context,
                     common.fs_url, common.read_only));

  constexpr bool exclusive = true;
  constexpr bool recursive = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &storage::FileSystemOperationRunner::CreateDirectory),
                     // Unretained is safe: context owns operation runner.
                     base::Unretained(common.fs_context->operation_runner()),
                     common.fs_url, exclusive, recursive,
                     std::move(outer_callback)));
}

void Server::Open2(const fusebox_staging::Open2RequestProto& request_proto,
                   Open2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();
  fusebox_staging::AccessMode access_mode =
      request_proto.has_access_mode() ? request_proto.access_mode()
                                      : fusebox_staging::AccessMode::NO_ACCESS;

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::Open2ResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  }

  bool readable = (access_mode == fusebox_staging::AccessMode::READ_ONLY) ||
                  (access_mode == fusebox_staging::AccessMode::READ_WRITE);
  bool writable = !common.read_only &&
                  ((access_mode == fusebox_staging::AccessMode::WRITE_ONLY) ||
                   (access_mode == fusebox_staging::AccessMode::READ_WRITE));

  uint64_t fuse_handle = InsertFuseFileMapEntry(
      FuseFileMapEntry(std::move(common.fs_context), std::move(common.fs_url),
                       readable, writable));

  fusebox_staging::Open2ResponseProto response_proto;
  response_proto.set_fuse_handle(fuse_handle);
  std::move(callback).Run(response_proto);
}

void Server::Read2(const fusebox_staging::Read2RequestProto& request_proto,
                   Read2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    fusebox_staging::Read2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  } else if (!iter->second.readable_) {
    fusebox_staging::Read2ResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  } else if (iter->second.has_in_flight_read_) {
    iter->second.pending_reads_.emplace_back(request_proto,
                                             std::move(callback));
    return;
  }
  iter->second.has_in_flight_read_ = true;
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

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.is_moniker_root) {
    ReadDir2ResponseProto response_proto;
    response_proto.set_posix_error_code(0);
    std::move(callback).Run(response_proto);
    return;
  } else if (common.error_code != base::File::Error::FILE_OK) {
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
      base::SequencedTaskRunner::GetCurrentDefault(),
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

void Server::RmDir(const fusebox_staging::RmDirRequestProto& request_proto,
                   RmDirCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.read_only) {
    fusebox_staging::RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&RunRmDirCallback, std::move(callback),
                                        common.fs_context));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &storage::FileSystemOperationRunner::RemoveDirectory),
                     // Unretained is safe: context owns operation runner.
                     base::Unretained(common.fs_context->operation_runner()),
                     common.fs_url, std::move(outer_callback)));
}

void Server::Stat2(const fusebox_staging::Stat2RequestProto& request_proto,
                   Stat2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.is_moniker_root) {
    constexpr bool is_directory = true;
    constexpr bool read_only = true;
    fusebox_staging::Stat2ResponseProto response_proto;
    fusebox_staging::DirEntryProto* stat = response_proto.mutable_stat();
    stat->set_mode_bits(Server::MakeModeBits(is_directory, read_only));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::Stat2ResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
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
                                        common.fs_context, common.read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, metadata_fields, std::move(outer_callback)));
}

void Server::Truncate(
    const fusebox_staging::TruncateRequestProto& request_proto,
    TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.read_only) {
    fusebox_staging::TruncateResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&RunTruncateCallback, std::move(callback),
                     common.fs_context, common.fs_url, common.read_only));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::Truncate),
          // Unretained is safe: context owns operation runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url,
          request_proto.has_length() ? request_proto.length() : 0,
          std::move(outer_callback)));
}

void Server::Unlink(const fusebox_staging::UnlinkRequestProto& request_proto,
                    UnlinkCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string fs_url_as_string = request_proto.has_file_system_url()
                                     ? request_proto.file_system_url()
                                     : std::string();

  auto common = ParseFileSystemURL(moniker_map_, prefix_map_, fs_url_as_string);
  if (common.error_code != base::File::Error::FILE_OK) {
    fusebox_staging::UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(FileErrorToErrno(common.error_code));
    std::move(callback).Run(response_proto);
    return;
  } else if (common.read_only) {
    fusebox_staging::UnlinkResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  }

  auto outer_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&RunUnlinkCallback, std::move(callback),
                                        common.fs_context));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::RemoveFile),
          // Unretained is safe: context owns operation runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, std::move(outer_callback)));
}

void Server::Write2(const fusebox_staging::Write2RequestProto& request_proto,
                    Write2Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uint64_t fuse_handle =
      request_proto.has_fuse_handle() ? request_proto.fuse_handle() : 0;
  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    fusebox_staging::Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(response_proto);
    return;
  } else if (!iter->second.writable_) {
    fusebox_staging::Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EACCES);
    std::move(callback).Run(response_proto);
    return;
  } else if (request_proto.has_data() &&
             (request_proto.data().size() > INT_MAX)) {
    fusebox_staging::Write2ResponseProto response_proto;
    response_proto.set_posix_error_code(EMSGSIZE);
    std::move(callback).Run(response_proto);
    return;
  } else if (iter->second.has_in_flight_write_) {
    iter->second.pending_writes_.emplace_back(request_proto,
                                              std::move(callback));
    return;
  }
  iter->second.has_in_flight_write_ = true;
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

void Server::OnRead2(
    uint64_t fuse_handle,
    Read2Callback callback,
    const fusebox_staging::Read2ResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    fusebox_staging::Read2ResponseProto enoent_response_proto;
    enoent_response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(enoent_response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  entry.has_in_flight_read_ = false;

  std::move(callback).Run(std::move(response_proto));

  if (entry.pending_reads_.empty()) {
    return;
  }
  PendingRead2 pending = std::move(entry.pending_reads_.front());
  entry.pending_reads_.pop_front();
  entry.has_in_flight_read_ = true;
  entry.DoRead2(pending.first,
                base::BindOnce(&Server::OnRead2, weak_ptr_factory_.GetWeakPtr(),
                               fuse_handle, std::move(pending.second)));
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

void Server::OnWrite2(
    uint64_t fuse_handle,
    Write2Callback callback,
    const fusebox_staging::Write2ResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto iter = fuse_file_map_.find(fuse_handle);
  if (iter == fuse_file_map_.end()) {
    fusebox_staging::Write2ResponseProto enoent_response_proto;
    enoent_response_proto.set_posix_error_code(ENOENT);
    std::move(callback).Run(enoent_response_proto);
    return;
  }
  FuseFileMapEntry& entry = iter->second;
  entry.has_in_flight_write_ = false;

  std::move(callback).Run(std::move(response_proto));

  if (entry.pending_writes_.empty()) {
    return;
  }
  PendingWrite2 pending = std::move(entry.pending_writes_.front());
  entry.pending_writes_.pop_front();
  entry.has_in_flight_write_ = true;
  entry.DoWrite2(
      pending.first,
      base::BindOnce(&Server::OnWrite2, weak_ptr_factory_.GetWeakPtr(),
                     fuse_handle, std::move(pending.second)));
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
