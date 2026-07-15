// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"

#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"

class Profile;

namespace borealis {

BorealisAppLauncherImpl::BorealisAppLauncherImpl(Profile* profile)
    : profile_(profile) {}

BorealisAppLauncherImpl::~BorealisAppLauncherImpl() = default;

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     BorealisLaunchSource source,
                                     OnLaunchedCallback callback) {
  // Borealis functionality removed. We are keeping only the MOTD dialog
  // and the implementation needed for uninstalling borealis from the dialog.
  LOG(WARNING) << "Borealis is no longer available";
  std::move(callback).Run(LaunchResult::kError);
}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     const std::vector<std::string>& args,
                                     BorealisLaunchSource source,
                                     OnLaunchedCallback callback) {
  // Borealis functionality removed. We are keeping only the MOTD dialog
  // and the implementation needed for uninstalling borealis from the dialog.
  LOG(WARNING) << "Borealis is no longer available";
  std::move(callback).Run(LaunchResult::kError);
}

void BorealisAppLauncherImpl::LaunchAfterMOTD(
    std::string app_id,
    const std::vector<std::string>& args,
    BorealisLaunchSource source,
    OnLaunchedCallback callback,
    UserMotdAction motd_user_action) {
  // Borealis functionality removed. We are keeping only the MOTD dialog
  // and the implementation needed for uninstalling borealis from the dialog.
  LOG(WARNING) << "Borealis is no longer available";
  std::move(callback).Run(LaunchResult::kError);
}

}  // namespace borealis
