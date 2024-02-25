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

}  // namespace

FuseBoxServiceProvider::FuseBoxServiceProvider() : server_(this) {}

FuseBoxServiceProvider::~FuseBoxServiceProvider() = default;

void FuseBoxServiceProvider::Start(scoped_refptr<dbus::ExportedObject> object) {
  exported_object_ = object;

  ExportProtoMethod(fusebox::kClose2Method, &fusebox::Server::Close2);
  ExportProtoMethod(fusebox::kCreateMethod, &fusebox::Server::Create);
  ExportProtoMethod(fusebox::kFlushMethod, &fusebox::Server::Flush);
  ExportProtoMethod(fusebox::kMkDirMethod, &fusebox::Server::MkDir);
  ExportProtoMethod(fusebox::kOpen2Method, &fusebox::Server::Open2);
  ExportProtoMethod(fusebox::kRead2Method, &fusebox::Server::Read2);
  ExportProtoMethod(fusebox::kReadDir2Method, &fusebox::Server::ReadDir2);
  ExportProtoMethod(fusebox::kRenameMethod, &fusebox::Server::Rename);
  ExportProtoMethod(fusebox::kRmDirMethod, &fusebox::Server::RmDir);
  ExportProtoMethod(fusebox::kStat2Method, &fusebox::Server::Stat2);
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
