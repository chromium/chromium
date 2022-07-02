// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fusebox_service_provider.h"

#include <sys/stat.h>

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/dbus/fusebox/fusebox_reverse_client.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// This file provides the "D-Bus protocol logic" half of the FuseBox server,
// coupled with the "business logic" half in fusebox_server.cc.

namespace ash {

namespace {

void OnExportedCallback(const std::string& interface_name,
                        const std::string& method_name,
                        bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void ReplyToClose(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender,
                  base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));

  std::move(sender).Run(std::move(response));
}

void ReplyToOpen(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  // For historical reasons, append a second parameter that's no longer used.
  writer.AppendUint64(0);

  std::move(sender).Run(std::move(response));
}

void ReplyToRead(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 base::File::Error error_code,
                 const uint8_t* data_ptr,
                 size_t data_len) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  writer.AppendArrayOfBytes(data_ptr, data_len);

  std::move(sender).Run(std::move(response));
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

void CallReverseReplyToReadDir(uint64_t cookie,
                               base::File::Error error_code,
                               fusebox::DirEntryListProto protos,
                               bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (auto* client = FuseBoxReverseClient::Get(); client) {
    client->ReplyToReadDir(cookie, static_cast<int32_t>(error_code),
                           std::move(protos), has_more);
  }
}

void ReplyToStat(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 base::File::Error error_code,
                 const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(static_cast<int32_t>(error_code));
  writer.AppendInt32(info.is_directory ? S_IFDIR : S_IFREG);
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
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToClose(method_call, std::move(sender),
                 base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  server_.Close(fs_url_as_string,
                base::BindOnce(&ReplyToClose, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::Open(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToOpen(method_call, std::move(sender),
                base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  server_.Open(fs_url_as_string,
               base::BindOnce(&ReplyToOpen, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::Read(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  int64_t offset = 0;
  int32_t length = 0;
  if (!reader.PopString(&fs_url_as_string) || !reader.PopInt64(&offset) ||
      !reader.PopInt32(&length)) {
    ReplyToRead(method_call, std::move(sender),
                base::File::Error::FILE_ERROR_INVALID_OPERATION, nullptr, 0);
    return;
  }

  server_.Read(fs_url_as_string, offset, length,
               base::BindOnce(&ReplyToRead, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::ReadDir(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  uint64_t cookie = 0;
  if (!reader.PopString(&fs_url_as_string) || !reader.PopUint64(&cookie)) {
    ReplyToReadDir(method_call, std::move(sender),
                   base::File::Error::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // The ReadDir D-Bus method call deserves a reply, even if we don't have any
  // directory entries yet. Those entries will be sent back separately, in
  // batches, by CallReverseReplyToReadDir.
  ReplyToReadDir(method_call, std::move(sender), base::File::Error::FILE_OK);

  server_.ReadDir(fs_url_as_string, cookie,
                  base::BindRepeating(&CallReverseReplyToReadDir));
}

void FuseBoxServiceProvider::Stat(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToStat(method_call, std::move(sender),
                base::File::Error::FILE_ERROR_INVALID_OPERATION,
                base::File::Info());
    return;
  }

  server_.Stat(fs_url_as_string,
               base::BindOnce(&ReplyToStat, method_call, std::move(sender)));
}

}  // namespace ash
