// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drivefs_native_message_host_ash.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_errors.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace drive {
namespace {

void CreateNativeHostSession(
    Profile* profile,
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) {
  auto* drive_service = DriveIntegrationServiceFactory::GetForProfile(profile);
  if (!drive_service || !drive_service->GetDriveFsInterface()) {
    // Mojo uses unsigned int for disconnect reason, but file errors are
    // negative, so negate the error to pass as it a positive int.
    port.ResetWithReason(-FILE_ERROR_SERVICE_UNAVAILABLE,
                         "Drivefs is unavailable.");
    return;
  }

  drive_service->GetDriveFsInterface()->CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParams::New(
          GURL(kDriveFsNativeMessageHostOrigins[0]).host()),
      std::move(session), std::move(port));
}

}  // namespace

std::unique_ptr<extensions::NativeMessageHost>
CreateDriveFsNativeMessageHostAsh(content::BrowserContext* browser_context) {
  return CreateDriveFsNativeMessageHost(base::BindOnce(
      CreateNativeHostSession, Profile::FromBrowserContext(browser_context)));
}

}  // namespace drive
