// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/drivefs_native_message_host_lacros.h"

#include "base/functional/bind.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "url/gurl.h"

namespace drive {
namespace {

void CreateNativeHostSession(
    Profile* profile,
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) {
  if (!profile->IsMainProfile()) {
    // Mojo uses unsigned int for disconnect reason, but file errors are
    // negative, so negate the error to pass as it a positive int.
    port.ResetWithReason(
        -FILE_ERROR_SERVICE_UNAVAILABLE,
        "DriveFS native messaging is only supported for the primary profile.");
    return;
  }

  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::DriveIntegrationService>()) {
    port.ResetWithReason(-FILE_ERROR_SERVICE_UNAVAILABLE,
                         "DriveFS native messaging is not available in this "
                         "version of the browser.");
    return;
  }

  const int drive_service_version =
      lacros_service
          ->GetInterfaceVersion<crosapi::mojom::DriveIntegrationService>();
  constexpr int min_required_version = static_cast<int>(
      crosapi::mojom::DriveIntegrationService::MethodMinVersions::
          kCreateNativeHostSessionMinVersion);
  if (drive_service_version < min_required_version) {
    port.ResetWithReason(-FILE_ERROR_SERVICE_UNAVAILABLE,
                         "DriveFS native messaging is not available in this "
                         "version of the browser.");
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DriveIntegrationService>()
      ->CreateNativeHostSession(
          drivefs::mojom::ExtensionConnectionParams::New(
              GURL(kDriveFsNativeMessageHostOrigins[0]).host()),
          std::move(session), std::move(port));
}

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostLacros(content::BrowserContext* browser_context) {
  return CreateDriveFsNativeMessageHost(base::BindOnce(
      CreateNativeHostSession, Profile::FromBrowserContext(browser_context)));
}

}  // namespace drive
