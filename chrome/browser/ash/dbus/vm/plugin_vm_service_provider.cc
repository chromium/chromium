// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/plugin_vm_service_provider.h"

#include <memory>
#include <utility>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/dbus/plugin_vm_service/plugin_vm_service.pb.h"
#include "components/prefs/pref_service.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// A fixed UUID where the first 4 bytes spell 'test', reported when under test.
constexpr char kFakeUUID[] = "74657374-4444-4444-8888-888888888888";

// These are the accepted inputs to the ShowSettingsPage D-Bus method.
constexpr char kShowSettingsPageDetails[] = "pluginVm/details";
constexpr char kShowSettingsPageSharedPaths[] = "pluginVm/sharedPaths";

namespace ash {

PluginVmServiceProvider::PluginVmServiceProvider() {}

PluginVmServiceProvider::~PluginVmServiceProvider() = default;

void PluginVmServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kPluginVmServiceInterface,
      chromeos::kPluginVmServiceGetLicenseDataMethod,
      base::BindRepeating(&PluginVmServiceProvider::GetLicenseData,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PluginVmServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kPluginVmServiceInterface,
      chromeos::kPluginVmServiceShowSettingsPage,
      base::BindRepeating(&PluginVmServiceProvider::ShowSettingsPage,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PluginVmServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kPluginVmServiceInterface,
      chromeos::kPluginVmServiceGetAppLicenseUserId,
      base::BindRepeating(&PluginVmServiceProvider::GetUserId,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&PluginVmServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      chromeos::kPluginVmServiceInterface,
      chromeos::kPluginVmServiceGetPermissionsMethod,
      base::BindRepeating(&PluginVmServiceProvider::GetPermissions,
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

  if (plugin_vm::FakeLicenseKeyIsSet()) {
    payload.set_device_id(kFakeUUID);
    payload.set_license_key(plugin_vm::GetFakeLicenseKey());
  } else {
    payload.set_device_id(g_browser_process->platform_part()
                              ->browser_policy_connector_ash()
                              ->GetDirectoryApiID());
  }
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
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
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (request.subpage_path() == kShowSettingsPageDetails) {
    chrome::ShowAppManagementPage(
        primary_profile, plugin_vm::kPluginVmShelfAppId,
        settings::AppManagementEntryPoint::kDBusServicePluginVm);
  } else if (request.subpage_path() == kShowSettingsPageSharedPaths) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        primary_profile,
        chromeos::settings::mojom::kPluginVmSharedPathsSubpagePath);
  } else {
    constexpr char error_message[] = "Invalid subpage_path";
    LOG(ERROR) << error_message;
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, error_message));
    return;
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void PluginVmServiceProvider::GetUserId(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  plugin_vm_service::GetAppLicenseUserIdResponse payload;
  payload.set_user_id(plugin_vm::GetPluginVmUserIdForProfile(
      ProfileManager::GetPrimaryUserProfile()));
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

void PluginVmServiceProvider::GetPermissions(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  plugin_vm_service::GetPermissionsResponse payload;
  payload.set_data_collection_enabled(
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmDataCollectionAllowed));
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(payload);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace ash
