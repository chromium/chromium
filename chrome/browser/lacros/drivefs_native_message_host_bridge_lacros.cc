// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/drivefs_native_message_host_bridge_lacros.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace drive {

namespace {

void ConnectToExtensionWithProfile(
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote,
    DriveFsNativeMessageHostBridge::ConnectToExtensionCallback callback,
    Profile* profile) {
  if (g_browser_process->IsShuttingDown() || !profile) {
    std::move(callback).Run(
        drivefs::mojom::ExtensionConnectionStatus::kUnknownError);
    return;
  }

  std::move(callback).Run(ConnectToDriveFsNativeMessageExtension(
      profile, params->extension_id, std::move(extension_receiver),
      std::move(drivefs_remote)));
}

}  // namespace

DriveFsNativeMessageHostBridge::DriveFsNativeMessageHostBridge()
    : receiver_(this) {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::DriveIntegrationService>()) {
    return;
  }

  const int drive_service_version =
      lacros_service
          ->GetInterfaceVersion<crosapi::mojom::DriveIntegrationService>();
  constexpr int min_required_version = static_cast<int>(
      crosapi::mojom::DriveIntegrationService::MethodMinVersions::
          kRegisterDriveFsNativeMessageHostBridgeMinVersion);
  if (drive_service_version < min_required_version) {
    LOG(WARNING) << "Could not register the DriveFS native message host bridge "
                    "as ash is too old.";
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DriveIntegrationService>()
      ->RegisterDriveFsNativeMessageHostBridge(
          receiver_.BindNewPipeAndPassRemote());
}

DriveFsNativeMessageHostBridge::~DriveFsNativeMessageHostBridge() = default;

void DriveFsNativeMessageHostBridge::ConnectToExtension(
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingPort>
        extension_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingHost> drivefs_remote,
    ConnectToExtensionCallback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(ConnectToExtensionWithProfile, std::move(params),
                     std::move(extension_receiver), std::move(drivefs_remote),
                     std::move(callback)));
}

}  // namespace drive
