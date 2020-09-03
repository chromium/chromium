// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/vm_applications_service_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {

VmApplicationsServiceProvider::VmApplicationsServiceProvider() {}

VmApplicationsServiceProvider::~VmApplicationsServiceProvider() = default;

void VmApplicationsServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceUpdateApplicationListMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::UpdateApplicationList,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceLaunchTerminalMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::LaunchTerminal,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      vm_tools::apps::kVmApplicationsServiceInterface,
      vm_tools::apps::kVmApplicationsServiceUpdateMimeTypesMethod,
      base::BindRepeating(&VmApplicationsServiceProvider::UpdateMimeTypes,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&VmApplicationsServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VmApplicationsServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void VmApplicationsServiceProvider::UpdateApplicationList(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::ApplicationList request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse ApplicationList from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile) ||
      plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile)) {
    auto* registry_service =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
    registry_service->UpdateApplicationList(request);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void VmApplicationsServiceProvider::LaunchTerminal(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::TerminalParams request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse TerminalParams from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile) &&
      request.owner_id() == crostini::CryptohomeIdForProfile(profile)) {
    // kInvalidDisplayId will launch terminal on the current active display.
    crostini::LaunchTerminal(
        profile, display::kInvalidDisplayId,
        crostini::ContainerId(request.vm_name(), request.container_name()),
        request.cwd(),
        std::vector<std::string>(request.params().begin(),
                                 request.params().end()));
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void VmApplicationsServiceProvider::UpdateMimeTypes(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  vm_tools::apps::MimeTypes request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] = "Unable to parse MimeTypes from message";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile)) {
    crostini::CrostiniMimeTypesService* mime_types_service =
        crostini::CrostiniMimeTypesServiceFactory::GetForProfile(profile);
    mime_types_service->UpdateMimeTypes(request);
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos
