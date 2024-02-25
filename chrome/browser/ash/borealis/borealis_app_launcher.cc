// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"

namespace borealis {

void BorealisAppLauncher::Launch(const BorealisContext& ctx,
                                 const std::string& app_id,
                                 OnLaunchedCallback callback) {
  Launch(ctx, app_id, {}, std::move(callback));
}

void BorealisAppLauncher::Launch(const BorealisContext& ctx,
                                 const std::string& app_id,
                                 const std::vector<std::string>& args,
                                 OnLaunchedCallback callback) {
  // Launching the borealis app is a legacy way of launching its main app
  if (app_id == kInstallerAppId) {
    Launch(ctx, kClientAppId, args, std::move(callback));
    return;
  }

  std::optional<guest_os::GuestOsRegistryService::Registration> reg =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(ctx.profile())
          ->GetRegistration(app_id);
  if (!reg) {
    std::move(callback).Run(LaunchResult::kUnknownApp);
    return;
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(ctx.profile()));
  request.set_vm_name(ctx.vm_name());
  request.set_container_name(ctx.container_name());
  request.set_desktop_file_id(reg->DesktopFileId());
  base::ranges::copy(args, google::protobuf::RepeatedFieldBackInserter(
                               request.mutable_files()));

  ash::CiceroneClient::Get()->LaunchContainerApplication(
      std::move(request),
      base::BindOnce(
          [](OnLaunchedCallback callback,
             std::optional<
                 vm_tools::cicerone::LaunchContainerApplicationResponse>
                 response) {
            if (!response) {
              LOG(ERROR) << "Failed to launch app: No response from cicerone";
              std::move(callback).Run(LaunchResult::kNoResponse);
              return;
            }
            if (!response->success()) {
              LOG(ERROR) << "Failed to launch app: "
                         << response->failure_reason();
              std::move(callback).Run(LaunchResult::kError);
              return;
            }
            std::move(callback).Run(LaunchResult::kSuccess);
          },
          std::move(callback)));
}

}  // namespace borealis
