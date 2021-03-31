// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_launcher.h"

#include "base/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace borealis {

void BorealisAppLauncher::Launch(const BorealisContext& ctx,
                                 const std::string& app_id,
                                 OnLaunchedCallback callback) {
  Launch(std::move(ctx), std::move(app_id), {}, std::move(callback));
}

void BorealisAppLauncher::Launch(const BorealisContext& ctx,
                                 const std::string& app_id,
                                 const std::vector<std::string>& args,
                                 OnLaunchedCallback callback) {
  // Do not launch anything when using the installer app.
  //
  // TODO(b/170677773): Launch a _certain_ application...
  if (app_id == kBorealisAppId) {
    std::move(callback).Run(LaunchResult::kSuccess);
    return;
  }

  base::Optional<guest_os::GuestOsRegistryService::Registration> reg =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(ctx.profile())
          ->GetRegistration(app_id);
  if (!reg) {
    std::move(callback).Run(LaunchResult::kUnknownApp);
    return;
  }

  vm_tools::cicerone::LaunchContainerApplicationRequest request;
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(ctx.profile()));
  request.set_vm_name(ctx.vm_name());
  request.set_container_name(ctx.container_name());
  request.set_desktop_file_id(reg->DesktopFileId());
  std::copy(
      args.begin(), args.end(),
      google::protobuf::RepeatedFieldBackInserter(request.mutable_files()));

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
                  std::move(callback).Run(LaunchResult::kNoResponse);
                  return;
                }
                if (!response->success()) {
                  LOG(ERROR)
                      << "Failed to launch app: " << response->failure_reason();
                  std::move(callback).Run(LaunchResult::kError);
                  return;
                }
                std::move(callback).Run(LaunchResult::kSuccess);
              },
              std::move(callback)));
}

BorealisAppLauncher::BorealisAppLauncher(Profile* profile)
    : profile_(profile) {}

void BorealisAppLauncher::Launch(std::string app_id,
                                 OnLaunchedCallback callback) {
  Launch(std::move(app_id), {}, std::move(callback));
}

void BorealisAppLauncher::Launch(std::string app_id,
                                 const std::vector<std::string>& args,
                                 OnLaunchedCallback callback) {
  DCHECK(BorealisService::GetForProfile(profile_)->Features().IsAllowed());
  if (!borealis::BorealisService::GetForProfile(profile_)
           ->Features()
           .IsEnabled()) {
    borealis::ShowBorealisInstallerView(profile_);
    return;
  }
  if (!borealis::BorealisService::GetForProfile(profile_)
           ->ContextManager()
           .IsRunning())
    borealis::ShowBorealisSplashScreenView(profile_);
  BorealisService::GetForProfile(profile_)->ContextManager().StartBorealis(
      base::BindOnce(
          [](std::string app_id, const std::vector<std::string>& args,
             BorealisAppLauncher::OnLaunchedCallback callback,
             BorealisContextManager::ContextOrFailure result) {
            if (!result) {
              LOG(ERROR) << "Failed to launch " << app_id << "(code "
                         << result.Error().error()
                         << "): " << result.Error().description();
              // If splash screen is showing and borealis did not launch
              // properly, close it.
              borealis::CloseBorealisSplashScreenView();
              std::move(callback).Run(LaunchResult::kError);
              return;
            }
            BorealisAppLauncher::Launch(*result.Value(), std::move(app_id),
                                        std::move(args), std::move(callback));
          },
          std::move(app_id), std::move(args), std::move(callback)));
}

}  // namespace borealis
