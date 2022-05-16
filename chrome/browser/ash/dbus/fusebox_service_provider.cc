// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fusebox_service_provider.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/fusebox/fusebox_reverse_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/message.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// ParseResult is the type returned by ParseCommonDBusMethodArguments. It is
// a result type (see https://en.wikipedia.org/wiki/Result_type), being
// either an error or a value. In this case, the error type is a
// base::File::Error (a numeric code) and the value type is a pair of
// storage::FileSystemContext and storage::FileSystemURL.
struct ParseResult {
  explicit ParseResult(base::File::Error error_code_arg);
  ParseResult(scoped_refptr<storage::FileSystemContext> fs_context_arg,
              storage::FileSystemURL fs_url_arg);
  ~ParseResult();

  base::File::Error error_code;
  scoped_refptr<storage::FileSystemContext> fs_context;
  storage::FileSystemURL fs_url;
};

ParseResult::ParseResult(base::File::Error error_code_arg)
    : error_code(error_code_arg), fs_context(), fs_url() {}

ParseResult::ParseResult(
    scoped_refptr<storage::FileSystemContext> fs_context_arg,
    storage::FileSystemURL fs_url_arg)
    : error_code(base::File::Error::FILE_OK),
      fs_context(std::move(fs_context_arg)),
      fs_url(std::move(fs_url_arg)) {}

ParseResult::~ParseResult() = default;

// All of the FuseBoxServiceProvider D-Bus methods' arguments start with a
// FileSystemURL (as a string). This function consumes and parses that first
// argument, as well as finding the FileSystemContext we will need to serve
// those methods.
ParseResult ParseCommonDBusMethodArguments(dbus::MessageReader* reader) {
  scoped_refptr<storage::FileSystemContext> fs_context =
      file_manager::util::GetFileManagerFileSystemContext(
          ProfileManager::GetActiveUserProfile());
  if (!fs_context) {
    LOG(ERROR) << "No FileSystemContext";
    return ParseResult(base::File::Error::FILE_ERROR_FAILED);
  }

  std::string fs_url_as_string;
  if (!reader->PopString(&fs_url_as_string)) {
    LOG(ERROR) << "No FileSystemURL";
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  }

  storage::FileSystemURL fs_url =
      fs_context->CrackURLInFirstPartyContext(GURL(fs_url_as_string));
  if (!fs_url.is_valid()) {
    LOG(ERROR) << "Invalid FileSystemURL";
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  } else if (!fs_context->external_backend()->CanHandleType(fs_url.type())) {
    LOG(ERROR) << "Backend cannot handle "
               << storage::GetFileSystemTypeString(fs_url.type());
    return ParseResult(base::File::Error::FILE_ERROR_INVALID_URL);
  }

  return ParseResult(std::move(fs_context), std::move(fs_url));
}

void OnExportedCallback(const std::string& interface_name,
                        const std::string& method_name,
                        bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

// For the ReplyToEtc functions here and (§) below, the fs_context argument may
// look unused, but we need to keep the storage::FileSystemContext reference
// alive until these functions are called back.
//
// The ReplyToReadDir function is an exception. It does not have an fs_context
// argument, since it's largely a placeholder D-Bus reply and calling
// ReplyToReadDir does not coincide with the end of any file system operation.
// The real reply (with its ongoing file system operation) is in
// CallReverseReplyToReadDir.

void ReplyToClose(scoped_refptr<storage::FileSystemContext> fs_context,
                  dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender,
                  base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));

  std::move(sender).Run(std::move(response));
}

void ReplyToOpenFailure(scoped_refptr<storage::FileSystemContext> fs_context,
                        dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender sender,
                        base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  writer.AppendUint64(0);

  std::move(sender).Run(std::move(response));
}

void ReplyToReadFailure(scoped_refptr<storage::FileSystemContext> fs_context,
                        dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender sender,
                        base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  writer.AppendArrayOfBytes(nullptr, 0);

  std::move(sender).Run(std::move(response));
}

void ReplyToReadTypical(scoped_refptr<storage::FileSystemContext> fs_context,
                        dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender sender,
                        std::unique_ptr<storage::FileStreamReader> fs_reader,
                        scoped_refptr<net::IOBuffer> buffer,
                        int length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  if (length < 0) {
    writer.AppendInt32(
        static_cast<int32_t>(storage::NetErrorToFileError(length)));
    writer.AppendArrayOfBytes(nullptr, 0);
  } else {
    writer.AppendInt32(static_cast<int32_t>(base::File::Error::FILE_OK));
    writer.AppendArrayOfBytes(reinterpret_cast<uint8_t*>(buffer->data()),
                              length);
  }

  std::move(sender).Run(std::move(response));

  // Clean-up / destroy I/O things on the I/O thread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<storage::FileStreamReader>,
                        scoped_refptr<net::IOBuffer>) {
                       // No-op other than smart pointers calling destructors.
                     },
                     std::move(fs_reader), std::move(buffer)));
}

void ReadOnIOThread(scoped_refptr<storage::FileSystemContext> fs_context,
                    storage::FileSystemURL fs_url,
                    dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender sender,
                    int64_t offset,
                    int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::unique_ptr<storage::FileStreamReader> fs_reader =
      fs_context->CreateFileStreamReader(fs_url, offset, length, base::Time());
  if (!fs_reader) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ReplyToReadFailure, std::move(fs_context),
                                  method_call, std::move(sender),
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
      base::BindOnce(&ReplyToReadTypical, fs_context, method_call,
                     std::move(sender), std::move(fs_reader), buffer)));

  int result =
      saved_fs_reader->Read(buffer.get(), length, std::move(pair.first));
  if (result != net::ERR_IO_PENDING) {  // The read was synchronous.
    std::move(pair.second).Run(result);
  }
}

// ReplyToReadDir and CallReverseReplyToReadDir form two halves of how the
// FuseBoxServiceProvider class (which implements the FBS D-Bus interface)
// serves an incoming ReadDir request. Here, FBS and FBRS denote the
// FuseBoxService and FuseBoxReverseService D-Bus interfaces.
//
// For an incoming FBS.ReadDir D-Bus call, the result is returned by calling
// FBRS.ReplyToReadDir repeatedly instead of in a single FBS.ReadDir reply. A
// storage::FileSystemOperation::ReadDirectoryCallback is a
// base::RepeatingCallback but a dbus::ExportedObject::ResponseSender is a
// base::OnceCallback.

void ReplyToReadDir(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender sender,
                    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));

  std::move(sender).Run(std::move(response));
}

void CallReverseReplyToReadDir(
    scoped_refptr<storage::FileSystemContext> fs_context,  // See (§) above.
    uint64_t cookie,
    base::File::Error error_code,
    storage::AsyncFileUtil::EntryList entry_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  fusebox::DirEntryListProto protos;
  for (const auto& entry : entry_list) {
    auto* proto = protos.add_entries();
    constexpr auto directory = filesystem::mojom::FsFileType::DIRECTORY;
    proto->set_is_directory(entry.type == directory);
    proto->set_name(entry.name.value());
  }

  if (auto* client = FuseBoxReverseClient::Get(); client) {
    client->ReplyToReadDir(cookie, static_cast<int32_t>(error_code),
                           std::move(protos), has_more);
  }
}

void ReplyToStat(scoped_refptr<storage::FileSystemContext> fs_context,
                 dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 base::File::Error error_code,
                 const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  mode_t mode = info.is_directory ? S_IFDIR : S_IFREG;
  writer.AppendInt32(mode);
  writer.AppendInt64(info.size);
  writer.AppendDouble(info.last_accessed.ToDoubleT());
  writer.AppendDouble(info.last_modified.ToDoubleT());
  writer.AppendDouble(info.creation_time.ToDoubleT());

  std::move(sender).Run(std::move(response));
}

}  // namespace

FuseBoxServiceProvider::FuseBoxServiceProvider() = default;

FuseBoxServiceProvider::~FuseBoxServiceProvider() = default;

void FuseBoxServiceProvider::Start(scoped_refptr<dbus::ExportedObject> object) {
  if (!ash::features::IsFileManagerFuseBoxEnabled()) {
    LOG(ERROR) << "Not enabled";
    return;
  }

  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kCloseMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Close,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kOpenMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Open,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kReadMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Read,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface,
                       fusebox::kReadDirMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::ReadDir,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kStatMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Stat,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
}

void FuseBoxServiceProvider::Close(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    ReplyToClose(common.fs_context, method_call, std::move(sender),
                 common.error_code);
    return;
  }

  // Fail with an invalid operation error for now. TODO(crbug.com/1249754)
  // implement MTP device writing.
  ReplyToClose(common.fs_context, method_call, std::move(sender),
               base::File::Error::FILE_ERROR_INVALID_OPERATION);
}

void FuseBoxServiceProvider::Open(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    ReplyToOpenFailure(common.fs_context, method_call, std::move(sender),
                       common.error_code);
    return;
  }

  // Fail with an invalid operation error for now. TODO(crbug.com/1249754)
  // implement MTP device writing.
  ReplyToOpenFailure(common.fs_context, method_call, std::move(sender),
                     base::File::Error::FILE_ERROR_INVALID_OPERATION);
}

void FuseBoxServiceProvider::Read(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    ReplyToReadFailure(std::move(common.fs_context), method_call,
                       std::move(sender), common.error_code);
    return;
  }

  int64_t offset = 0;
  if (!reader.PopInt64(&offset)) {
    LOG(ERROR) << "No Offset";
    ReplyToReadFailure(std::move(common.fs_context), method_call,
                       std::move(sender),
                       base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  int32_t length = 0;
  if (!reader.PopInt32(&length)) {
    LOG(ERROR) << "No Length";
    ReplyToReadFailure(std::move(common.fs_context), method_call,
                       std::move(sender),
                       base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ReadOnIOThread, common.fs_context,
                                common.fs_url, method_call, std::move(sender),
                                offset, static_cast<int64_t>(length)));
}

void FuseBoxServiceProvider::ReadDir(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    ReplyToReadDir(method_call, std::move(sender), common.error_code);
    return;
  }

  uint64_t cookie = 0;
  if (!reader.PopUint64(&cookie)) {
    LOG(ERROR) << "No Cookie";
    ReplyToReadDir(method_call, std::move(sender),
                   base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // The ReadDir D-Bus method call deserves a reply, even if we don't have any
  // directory entries yet. Those entries will be sent back separately, in
  // batches, by CallReverseReplyToReadDir.
  ReplyToReadDir(method_call, std::move(sender), base::File::Error::FILE_OK);

  auto callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindRepeating(&CallReverseReplyToReadDir,
                                             common.fs_context, cookie));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindRepeating(
          base::IgnoreResult(
              &storage::FileSystemOperationRunner::ReadDirectory),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, std::move(callback)));
}

void FuseBoxServiceProvider::Stat(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  auto common = ParseCommonDBusMethodArguments(&reader);
  if (common.error_code != base::File::Error::FILE_OK) {
    base::File::Info info;
    ReplyToStat(std::move(common.fs_context), method_call, std::move(sender),
                common.error_code, info);
    return;
  }

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED;

  auto callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&ReplyToStat, common.fs_context,
                                        method_call, std::move(sender)));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&storage::FileSystemOperationRunner::GetMetadata),
          // Unretained is safe: common.fs_context owns its operation_runner.
          base::Unretained(common.fs_context->operation_runner()),
          common.fs_url, metadata_fields, std::move(callback)));
}

}  // namespace ash
