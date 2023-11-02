// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_disk_management_service_provider.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_disk_manager_dispatcher.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/vm_disk_management/disk_management.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

VmDiskManagementServiceProvider::VmDiskManagementServiceProvider() = default;

VmDiskManagementServiceProvider::~VmDiskManagementServiceProvider() = default;

void VmDiskManagementServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::disk_management::kVmDiskManagementServiceInterface,
      vm_tools::disk_management::kVmDiskManagementServiceGetDiskInfoMethod,
      base::BindRepeating(&VmDiskManagementServiceProvider::GetDiskInfo,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmDiskManagementServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::disk_management::kVmDiskManagementServiceInterface,
      vm_tools::disk_management::kVmDiskManagementServiceRequestSpaceMethod,
      base::BindRepeating(&VmDiskManagementServiceProvider::RequestSpace,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmDiskManagementServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::disk_management::kVmDiskManagementServiceInterface,
      vm_tools::disk_management::kVmDiskManagementServiceReleaseSpaceMethod,
      base::BindRepeating(&VmDiskManagementServiceProvider::ReleaseSpace,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmDiskManagementServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmDiskManagementServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void VmDiskManagementServiceProvider::GetDiskInfo(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::disk_management::GetDiskInfoRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse GetDiskInfoRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  if (request.origin().vm_name().empty() ||
      request.origin().container_name().empty() ||
      request.origin().owner_id().empty()) {
    OnGetDiskInfo(std::move(response), std::move(response_sender),
                  ExpectedGetDiskInfoResponse::Unexpected(
                      borealis::Described<borealis::BorealisGetDiskInfoResult>(
                          borealis::BorealisGetDiskInfoResult::kInvalidRequest,
                          "GetDiskInfoRequest failed: request has missing or "
                          "incomplete origin")));
    return;
  }

  borealis::BorealisService::GetForProfile(
      ProfileManager::GetPrimaryUserProfile())
      ->DiskManagerDispatcher()
      .GetDiskInfo(
          request.origin().vm_name(), request.origin().container_name(),
          base::BindOnce(&VmDiskManagementServiceProvider::OnGetDiskInfo,
                         weak_ptr_factory_.GetWeakPtr(), std::move(response),
                         std::move(response_sender)));
}

void VmDiskManagementServiceProvider::RequestSpace(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::disk_management::RequestSpaceRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse RequestSpaceRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  if (request.origin().vm_name().empty() ||
      request.origin().container_name().empty() ||
      request.origin().owner_id().empty()) {
    OnRequestSpace(
        std::move(response), std::move(response_sender),
        ExpectedRequestDeltaResponse::Unexpected(
            borealis::Described<borealis::BorealisResizeDiskResult>(
                borealis::BorealisResizeDiskResult::kInvalidRequest,
                "RequestSpaceRequest failed: request has missing or incomplete "
                "origin")));
    return;
  }

  borealis::BorealisService::GetForProfile(
      ProfileManager::GetPrimaryUserProfile())
      ->DiskManagerDispatcher()
      .RequestSpace(
          request.origin().vm_name(), request.origin().container_name(),
          request.space_requested(),
          base::BindOnce(&VmDiskManagementServiceProvider::OnRequestSpace,
                         weak_ptr_factory_.GetWeakPtr(), std::move(response),
                         std::move(response_sender)));
}

void VmDiskManagementServiceProvider::ReleaseSpace(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::disk_management::ReleaseSpaceRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse ReleaseSpaceRequest from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);

  if (request.origin().vm_name().empty() ||
      request.origin().container_name().empty() ||
      request.origin().owner_id().empty()) {
    OnReleaseSpace(
        std::move(response), std::move(response_sender),
        ExpectedRequestDeltaResponse::Unexpected(
            borealis::Described<borealis::BorealisResizeDiskResult>(
                borealis::BorealisResizeDiskResult::kInvalidRequest,
                "ReleaseSpaceRequest failed: request has missing or incomplete "
                "origin")));
    return;
  }

  borealis::BorealisService::GetForProfile(
      ProfileManager::GetPrimaryUserProfile())
      ->DiskManagerDispatcher()
      .ReleaseSpace(
          request.origin().vm_name(), request.origin().container_name(),
          request.space_to_release(),
          base::BindOnce(&VmDiskManagementServiceProvider::OnReleaseSpace,
                         weak_ptr_factory_.GetWeakPtr(), std::move(response),
                         std::move(response_sender)));
}

void VmDiskManagementServiceProvider::OnGetDiskInfo(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    ExpectedGetDiskInfoResponse response_or_error) {
  vm_tools::disk_management::GetDiskInfoResponse payload;
  if (!response_or_error) {
    LOG(ERROR) << "GetDiskInfoRequest failed: "
               << response_or_error.Error().description();
    payload.set_error(int(response_or_error.Error().error()));
  } else {
    payload.set_available_space(response_or_error.Value().available_bytes);
    payload.set_expandable_space(response_or_error.Value().expandable_bytes);
    payload.set_disk_size(response_or_error.Value().disk_size);
  }
  // Reply to the original D-Bus method call.
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmDiskManagementServiceProvider::OnRequestSpace(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    ExpectedRequestDeltaResponse response_or_error) {
  vm_tools::disk_management::RequestSpaceResponse payload;
  if (!response_or_error) {
    LOG(ERROR) << "RequestSpaceRequest failed: "
               << response_or_error.Error().description();
    payload.set_error(int(response_or_error.Error().error()));
  } else {
    payload.set_space_granted(response_or_error.Value());
  }
  // Reply to the original D-Bus method call.
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void VmDiskManagementServiceProvider::OnReleaseSpace(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    ExpectedRequestDeltaResponse response_or_error) {
  vm_tools::disk_management::ReleaseSpaceResponse payload;
  if (!response_or_error) {
    LOG(ERROR) << "ReleaseSpaceRequest failed: "
               << response_or_error.Error().description();
    payload.set_error(int(response_or_error.Error().error()));
  } else {
    payload.set_space_released(response_or_error.Value());
  }
  // Reply to the original D-Bus method call.
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
