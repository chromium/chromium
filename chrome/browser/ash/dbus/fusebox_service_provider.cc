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

// The "fusebox_staging" concept is described in
// chrome/browser/ash/fusebox/fusebox_staging.proto
//
// TODO(b/255520194): remove this section.
namespace fusebox_staging {
const char kStat2Method[] = "Stat2";
}  // namespace fusebox_staging

namespace ash {

namespace {

void OnExportedCallback(const std::string& interface_name,
                        const std::string& method_name,
                        bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

template <typename T>
void ReplyToProtoMethod(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender sender,
                        const T& proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());

  writer.AppendProtoAsArrayOfBytes(proto);

  std::move(sender).Run(std::move(response));
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

}  // namespace

FuseBoxServiceProvider::FuseBoxServiceProvider() : server_(this) {}

FuseBoxServiceProvider::~FuseBoxServiceProvider() = default;

void FuseBoxServiceProvider::Start(scoped_refptr<dbus::ExportedObject> object) {
  exported_object_ = object;

  // TODO(b/255520194): remove the deprecated Stat, Open, Read and Close
  // methods. They have been replaced by Stat2, Open2, Read2 and Close2.
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
  object->ExportMethod(fusebox::kFuseBoxServiceInterface, fusebox::kStatMethod,
                       base::BindRepeating(&FuseBoxServiceProvider::Stat,
                                           weak_ptr_factory_.GetWeakPtr()),
                       base::BindOnce(&OnExportedCallback));

  ExportProtoMethod(fusebox::kClose2Method, &fusebox::Server::Close2);
  ExportProtoMethod(fusebox::kCreateMethod, &fusebox::Server::Create);
  ExportProtoMethod(fusebox::kMkDirMethod, &fusebox::Server::MkDir);
  ExportProtoMethod(fusebox::kOpen2Method, &fusebox::Server::Open2);
  ExportProtoMethod(fusebox::kRead2Method, &fusebox::Server::Read2);
  ExportProtoMethod(fusebox::kReadDir2Method, &fusebox::Server::ReadDir2);
  ExportProtoMethod(fusebox::kRmDirMethod, &fusebox::Server::RmDir);
  ExportProtoMethod(fusebox_staging::kStat2Method, &fusebox::Server::Stat2);
  ExportProtoMethod(fusebox::kTruncateMethod, &fusebox::Server::Truncate);
  ExportProtoMethod(fusebox::kUnlinkMethod, &fusebox::Server::Unlink);
  ExportProtoMethod(fusebox::kWrite2Method, &fusebox::Server::Write2);

  ExportProtoMethod(fusebox::kListStoragesMethod,
                    &fusebox::Server::ListStorages);
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

template <typename RequestProto, typename ResponseProto>
void FuseBoxServiceProvider::ServeProtoMethod(
    ServerMethodPtr<RequestProto, ResponseProto> method,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender sender) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dbus::MessageReader reader(method_call);
  RequestProto request_proto;
  if (!reader.PopArrayOfBytesAsProto(&request_proto)) {
    ResponseProto response_proto;
    response_proto.set_posix_error_code(EINVAL);
    ReplyToProtoMethod(method_call, std::move(sender), response_proto);
    return;
  }
  (server_.*method)(request_proto,
                    base::BindOnce(&ReplyToProtoMethod<ResponseProto>,
                                   method_call, std::move(sender)));
}

template <typename RequestProto, typename ResponseProto>
void FuseBoxServiceProvider::ExportProtoMethod(
    const std::string& method_name,
    ServerMethodPtr<RequestProto, ResponseProto> method) {
  exported_object_->ExportMethod(
      fusebox::kFuseBoxServiceInterface, method_name,
      base::BindRepeating(
          &FuseBoxServiceProvider::ServeProtoMethod<RequestProto,
                                                    ResponseProto>,
          weak_ptr_factory_.GetWeakPtr(), method),
      base::BindOnce(&OnExportedCallback));
}

}  // namespace ash
