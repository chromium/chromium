// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/virtual_file_request_service_provider.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

arc::ArcFileSystemBridge* GetArcFileSystemBridge() {
  arc::ArcSessionManager* session_manager = arc::ArcSessionManager::Get();
  if (!session_manager)
    return nullptr;
  Profile* profile = session_manager->profile();
  if (!profile)
    return nullptr;
  return arc::ArcFileSystemBridge::GetForBrowserContext(profile);
}

}  // namespace

VirtualFileRequestServiceProvider::VirtualFileRequestServiceProvider() {}

VirtualFileRequestServiceProvider::~VirtualFileRequestServiceProvider() =
    default;

void VirtualFileRequestServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kVirtualFileRequestServiceInterface,
      chromeos::kVirtualFileRequestServiceHandleReadRequestMethod,
      base::BindRepeating(&VirtualFileRequestServiceProvider::HandleReadRequest,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce([](const std::string& interface_name,
                        const std::string& method_name, bool success) {
        LOG_IF(ERROR, !success)
            << "Failed to export " << interface_name << "." << method_name;
      }));
  exported_object->ExportMethod(
      chromeos::kVirtualFileRequestServiceInterface,
      chromeos::kVirtualFileRequestServiceHandleIdReleasedMethod,
      base::BindRepeating(&VirtualFileRequestServiceProvider::HandleIdReleased,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce([](const std::string& interface_name,
                        const std::string& method_name, bool success) {
        LOG_IF(ERROR, !success)
            << "Failed to export " << interface_name << "." << method_name;
      }));
}

void VirtualFileRequestServiceProvider::HandleReadRequest(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::string id;
  int64_t offset = 0;
  int64_t size = 0;
  base::ScopedFD pipe_write_end;
  dbus::MessageReader reader(method_call);
  if (!reader.PopString(&id) || !reader.PopInt64(&offset) ||
      !reader.PopInt64(&size) || !reader.PopFileDescriptor(&pipe_write_end)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, std::string()));
    return;
  }

  arc::ArcFileSystemBridge* bridge = GetArcFileSystemBridge();
  if (bridge &&
      bridge->HandleReadRequest(id, offset, size, std::move(pipe_write_end))) {
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  } else {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 std::string()));
  }
}

void VirtualFileRequestServiceProvider::HandleIdReleased(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::string id;
  dbus::MessageReader reader(method_call);
  if (!reader.PopString(&id)) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, std::string()));
    return;
  }

  arc::ArcFileSystemBridge* bridge = GetArcFileSystemBridge();
  if (bridge && bridge->HandleIdReleased(id)) {
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
  } else {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 std::string()));
  }
}

}  // namespace ash
