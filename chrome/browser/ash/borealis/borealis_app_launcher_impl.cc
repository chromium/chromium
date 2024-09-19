// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/borealis/borealis_launch_error_dialog.h"
#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_dialog.h"

namespace borealis {
BorealisAppLauncherImpl::~BorealisAppLauncherImpl() = default;

BorealisAppLauncherImpl::BorealisAppLauncherImpl(Profile* profile)
    : profile_(profile) {}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     BorealisLaunchSource source,
                                     OnLaunchedCallback callback) {
  Launch(std::move(app_id), {}, std::move(source), std::move(callback));
}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     const std::vector<std::string>& args,
                                     BorealisLaunchSource source,
                                     OnLaunchedCallback callback) {
  if (!borealis::BorealisServiceFactory::GetForProfile(profile_)
           ->Features()
           .IsEnabled()) {
    ash::BorealisInstallerDialog::Show(profile_);
    RecordBorealisInstallSourceHistogram(source);
    std::move(callback).Run(LaunchResult::kSuccess);
    return;
  }
  if (!borealis::BorealisServiceFactory::GetForProfile(profile_)
           ->ContextManager()
           .IsRunning()) {
    borealis::ShowBorealisSplashScreenView(profile_);
  }
  RecordBorealisLaunchSourceHistogram(source);
  BorealisServiceFactory::GetForProfile(profile_)
      ->ContextManager()
      .StartBorealis(base::BindOnce(
          [](Profile* profile, std::string app_id,
             const std::vector<std::string>& args,
             BorealisAppLauncherImpl::OnLaunchedCallback callback,
             BorealisContextManager::ContextOrFailure result) {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to launch " << app_id << "(code "
                         << result.error().error()
                         << "): " << result.error().description();
              // If splash screen is showing and borealis did not launch
              // properly, close it.
              borealis::CloseBorealisSplashScreenView();
              views::borealis::ShowBorealisLaunchErrorView(
                  profile, result.error().error());
              std::move(callback).Run(LaunchResult::kError);
              return;
            }
            BorealisAppLauncher::Launch(*result.value(), std::move(app_id),
                                        std::move(args), std::move(callback));
          },
          profile_, std::move(app_id), std::move(args), std::move(callback)));
}

}  // namespace borealis
