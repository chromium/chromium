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
  // Borealis is no longer available.
  LOG(WARNING) << "Borealis is no longer available";
  std::move(callback).Run(LaunchResult::kError);
}

void BorealisAppLauncherImpl::Launch(std::string app_id,
                                     const std::vector<std::string>& args,
                                     BorealisLaunchSource source,
                                     OnLaunchedCallback callback) {
  LOG(WARNING) << "Borealis is no longer available";
  std::move(callback).Run(LaunchResult::kError);
}

}  // namespace borealis
