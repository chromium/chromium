// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/syslog_logging.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/extension_downloader.h"

namespace chromeos {

StartupAppLauncherUpdateChecker::StartupAppLauncherUpdateChecker(
    Profile* profile)
    : profile_(profile) {}

StartupAppLauncherUpdateChecker::~StartupAppLauncherUpdateChecker() = default;

bool StartupAppLauncherUpdateChecker::Run(UpdateCheckCallback callback) {
  if (callback_) {
    DLOG(WARNING) << "Running multiple update check is not supported.";
    return false;
  }

  extensions::ExtensionUpdater* updater =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->updater();
  if (!updater) {
    return false;
  }

  callback_ = std::move(callback);

  update_found_ = false;

  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  params.update_found_callback =
      base::BindRepeating(&StartupAppLauncherUpdateChecker::MarkUpdateFound,
                          weak_ptr_factory_.GetWeakPtr());
  params.callback =
      base::BindOnce(&StartupAppLauncherUpdateChecker::OnExtensionUpdaterDone,
                     weak_ptr_factory_.GetWeakPtr());
  updater->CheckNow(std::move(params));
  return true;
}

void StartupAppLauncherUpdateChecker::MarkUpdateFound(
    const std::string& id,
    const base::Version& version) {
  SYSLOG(INFO) << "Found extension update id=" << id
               << " version=" << version.GetString();
  update_found_ = true;
}

void StartupAppLauncherUpdateChecker::OnExtensionUpdaterDone() {
  // It is not safe to use `this` after the callback has been run.
  std::move(callback_).Run(update_found_);
}

}  // namespace chromeos
