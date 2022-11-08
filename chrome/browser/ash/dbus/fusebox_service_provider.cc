// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/fusebox_service_provider.h"

#include <sys/stat.h>

#include "ash/constants/ash_features.h"
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
                  int32_t posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(posix_error_code);

  std::move(sender).Run(std::move(response));
}

void ReplyToMkDir(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender,
                  const fusebox_staging::MkDirResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendProtoAsArrayOfBytes(response_proto);

  std::move(sender).Run(std::move(response));
}

void ReplyToOpen(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 int32_t posix_error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(posix_error_code);
  // For historical reasons, append a second parameter that's no longer used.
  writer.AppendUint64(0);

  std::move(sender).Run(std::move(response));
}

void ReplyToRead(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 int32_t posix_error_code,
                 const uint8_t* data_ptr,
                 size_t data_len) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(posix_error_code);
  writer.AppendArrayOfBytes(data_ptr, data_len);

  std::move(sender).Run(std::move(response));
}

void ReplyToReadDir2(dbus::MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender sender,
                     const fusebox::ReadDir2ResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendProtoAsArrayOfBytes(response_proto);

  std::move(sender).Run(std::move(response));
}

void ReplyToRmDir(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender sender,
                  const fusebox_staging::RmDirResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendProtoAsArrayOfBytes(response_proto);

  std::move(sender).Run(std::move(response));
}

void ReplyToStat(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender sender,
                 int32_t posix_error_code,
                 const base::File::Info& info,
                 bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendInt32(posix_error_code);
  // For historical reasons, the D-Bus protocol uses a *signed* int32_t, even
  // though /usr/include/x86_64-linux-gnu/bits/typesizes.h says "#define
  // __MODE_T_TYPE __U32_TYPE" (and hence MakeModeBits returns an *unsigned*
  // uint32_t). We use a static_cast to convert between them.
  writer.AppendInt32(static_cast<int32_t>(
      fusebox::Server::MakeModeBits(info.is_directory, read_only)));
  writer.AppendInt64(info.size);
  writer.AppendDouble(info.last_accessed.ToDoubleT());
  writer.AppendDouble(info.last_modified.ToDoubleT());
  writer.AppendDouble(info.creation_time.ToDoubleT());

  std::move(sender).Run(std::move(response));
}

void ReplyToListStorages(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender,
    const fusebox::ListStoragesResponseProto& response_proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendProtoAsArrayOfBytes(response_proto);

  std::move(sender).Run(std::move(response));
}

}  // namespace

FuseBoxServiceProvider::FuseBoxServiceProvider() : server_(this) {}

FuseBoxServiceProvider::~FuseBoxServiceProvider() = default;

void FuseBoxServiceProvider::Start(scoped_refptr<dbus::ExportedObject> object) {
  if (!ash::features::IsFileManagerFuseBoxEnabled()) {
    LOG(ERROR) << "Not enabled";
    return;
  }

  exported_object_ = object;
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kCloseMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Close,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kMkDirMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::MkDir,
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
                       fusebox::kReadDir2Method,
                       base::BindRepeating(&FuseBoxServiceProvider::ReadDir2,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kRmDirMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::RmDir,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kStatMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Stat,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));

  object->ExportMethod(
      fusebox::kFuseBoxServiceInterface, fusebox::kListStoragesMethod,
      base::BindRepeating(&FuseBoxServiceProvider::ListStorages,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExportedCallback));
}

void FuseBoxServiceProvider::OnRegisterFSURLPrefix(const std::string& subdir) {
  if (!exported_object_) {
    return;
  }
  dbus::Signal signal(fusebox::kFuseBoxServiceInterface,
                      fusebox::kStorageAttachedSignal);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(subdir);
  exported_object_->SendSignal(&signal);
}

void FuseBoxServiceProvider::OnUnregisterFSURLPrefix(
    const std::string& subdir) {
  if (!exported_object_) {
    return;
  }
  dbus::Signal signal(fusebox::kFuseBoxServiceInterface,
                      fusebox::kStorageDetachedSignal);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(subdir);
  exported_object_->SendSignal(&signal);
}

void FuseBoxServiceProvider::Close(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToClose(method_call, std::move(sender), EINVAL);
    return;
  }

  server_.Close(fs_url_as_string,
                base::BindOnce(&ReplyToClose, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::MkDir(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  fusebox_staging::MkDirRequestProto request_proto;
  if (!reader.PopArrayOfBytesAsProto(&request_proto)) {
    fusebox_staging::MkDirResponseProto response_proto;
    response_proto.set_posix_error_code(EINVAL);
    ReplyToMkDir(method_call, std::move(sender), response_proto);
    return;
  }

  server_.MkDir(request_proto,
                base::BindOnce(&ReplyToMkDir, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::Open(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToOpen(method_call, std::move(sender), EINVAL);
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
    ReplyToRead(method_call, std::move(sender), EINVAL, nullptr, 0);
    return;
  }

  server_.Read(fs_url_as_string, offset, length,
               base::BindOnce(&ReplyToRead, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::ReadDir2(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  fusebox::ReadDir2RequestProto request_proto;
  if (!reader.PopArrayOfBytesAsProto(&request_proto)) {
    fusebox::ReadDir2ResponseProto response_proto;
    response_proto.set_posix_error_code(EINVAL);
    ReplyToReadDir2(method_call, std::move(sender), response_proto);
    return;
  }

  server_.ReadDir2(request_proto, base::BindOnce(&ReplyToReadDir2, method_call,
                                                 std::move(sender)));
}

void FuseBoxServiceProvider::RmDir(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  fusebox_staging::RmDirRequestProto request_proto;
  if (!reader.PopArrayOfBytesAsProto(&request_proto)) {
    fusebox_staging::RmDirResponseProto response_proto;
    response_proto.set_posix_error_code(EINVAL);
    ReplyToRmDir(method_call, std::move(sender), response_proto);
    return;
  }

  server_.RmDir(request_proto,
                base::BindOnce(&ReplyToRmDir, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::Stat(dbus::MethodCall* method_call,
                                  dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  std::string fs_url_as_string;
  if (!reader.PopString(&fs_url_as_string)) {
    ReplyToStat(method_call, std::move(sender), EINVAL, base::File::Info(),
                false);
    return;
  }

  server_.Stat(fs_url_as_string,
               base::BindOnce(&ReplyToStat, method_call, std::move(sender)));
}

void FuseBoxServiceProvider::ListStorages(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dbus::MessageReader reader(method_call);
  fusebox::ListStoragesRequestProto request_proto;
  if (!reader.PopArrayOfBytesAsProto(&request_proto)) {
    fusebox::ListStoragesResponseProto response_proto;
    response_proto.set_posix_error_code(EINVAL);
    ReplyToListStorages(method_call, std::move(sender), response_proto);
    return;
  }

  server_.ListStorages(
      request_proto,
      base::BindOnce(&ReplyToListStorages, method_call, std::move(sender)));
}

}  // namespace ash
