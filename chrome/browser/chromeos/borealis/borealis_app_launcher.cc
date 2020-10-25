// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_app_launcher.h"

#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace borealis {

void BorealisAppLauncher::Launch(const BorealisContext& ctx,
                                 const std::string& app_id,
                                 OnLaunchedCallback callback) {
  // Do not launch anything when using the installer app.
  //
  // TODO(b/170677773): Launch a _certain_ application...
  if (app_id == kBorealisAppId) {
    std::move(callback).Run(kSuccess);
    return;
  }

  base::Optional<guest_os::GuestOsRegistryService::Registration> reg =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(ctx.profile())
          ->GetRegistration(app_id);

  if (!reg) {
    std::move(callback).Run(kUnknownApp);
    return;
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(ctx.profile()));
  request.set_vm_name(ctx.vm_name());
  request.set_container_name(ctx.container_name());
  request.set_desktop_file_id(reg->DesktopFileId());

  chromeos::DBusThreadManager::Get()
      ->GetCiceroneClient()
      ->LaunchContainerApplication(
          std::move(request),
          base::BindOnce(
              [](OnLaunchedCallback callback,
                 base::Optional<
                     vm_tools::cicerone::LaunchContainerApplicationResponse>
                     response) {
                if (!response) {
                  LOG(ERROR)
                      << "Failed to launch app: No response from cicerone";
                  std::move(callback).Run(kNoResponse);
                  return;
                }
                if (!response->success()) {
                  LOG(ERROR)
                      << "Failed to launch app: " << response->failure_reason();
                  std::move(callback).Run(kError);
                  return;
                }
                std::move(callback).Run(kSuccess);
              },
              std::move(callback)));
}

}  // namespace borealis
