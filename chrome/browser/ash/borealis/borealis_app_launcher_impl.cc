// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/borealis/borealis_installer_view.h"
#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

namespace borealis {
BorealisAppLauncherImpl::~BorealisAppLauncherImpl() = default;

BorealisAppLauncherImpl::BorealisAppLauncherImpl(Profile* profile)
    : profile_(profile) {}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     OnLaunchedCallback callback) {
  Launch(std::move(app_id), {}, std::move(callback));
}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     const std::vector<std::string>& args,
                                     OnLaunchedCallback callback) {
  BorealisFeatures::AllowStatus allow_status =
      borealis::BorealisService::GetForProfile(profile_)
          ->Features()
          .MightBeAllowed();
  if (allow_status != BorealisFeatures::AllowStatus::kAllowed) {
    LOG(WARNING) << "Borealis app launch blocked: " << allow_status;
    std::move(callback).Run(LaunchResult::kError);
    return;
  }
  if (!borealis::BorealisService::GetForProfile(profile_)
           ->Features()
           .IsEnabled()) {
    borealis::ShowBorealisInstallerView(profile_);
    return;
  }
  if (!borealis::BorealisService::GetForProfile(profile_)
           ->ContextManager()
           .IsRunning()) {
    borealis::ShowBorealisSplashScreenView(profile_);
  }
  BorealisService::GetForProfile(profile_)->ContextManager().StartBorealis(
      base::BindOnce(
          [](std::string app_id, const std::vector<std::string>& args,
             BorealisAppLauncherImpl::OnLaunchedCallback callback,
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
