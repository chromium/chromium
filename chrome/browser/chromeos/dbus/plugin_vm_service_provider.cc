// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/plugin_vm_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/dbus/plugin_vm_service/plugin_vm_service.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

PluginVmServiceProvider::PluginVmServiceProvider() {}

PluginVmServiceProvider::~PluginVmServiceProvider() = default;

void PluginVmServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kPluginVmServiceInterface, kPluginVmServiceGetLicenseDataMethod,
      base::BindRepeating(&PluginVmServiceProvider::GetLicenseData,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kPluginVmServiceInterface, kPluginVmServiceShowSettingsPage,
      base::BindRepeating(&PluginVmServiceProvider::ShowSettingsPage,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void PluginVmServiceProvider::GetLicenseData(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  plugin_vm_service::GetLicenseDataResponse payload;
  payload.set_device_id(g_browser_process->platform_part()
                            ->browser_policy_connector_chromeos()
                            ->GetDirectoryApiID());
  payload.set_license_key(plugin_vm::GetPluginVmLicenseKey());
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  response_sender.Run(std::move(response));
}

void PluginVmServiceProvider::ShowSettingsPage(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  plugin_vm_service::ShowSettingsPageRequest request;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    constexpr char error_message[] =
        "Unable to parse ShowSettingsPageRequest from message";
    LOG(ERROR) << error_message;
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  // Validate subpage path.
  if ((request.subpage_path() != chrome::kPluginVmDetailsSubPage) &&
      (request.subpage_path() != chrome::kPluginVmSharedPathsSubPage)) {
    constexpr char error_message[] = "Invalid subpage_path";
    LOG(ERROR) << error_message;
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetPrimaryUserProfile(), request.subpage_path());
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos
