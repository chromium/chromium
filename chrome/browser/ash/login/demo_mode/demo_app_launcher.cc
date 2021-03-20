// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_app_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/window_open_disposition.h"

namespace chromeos {

const char DemoAppLauncher::kDemoAppId[] = "klimoghijjogocdbaikffefjfcfheiel";

DemoAppLauncher::DemoAppLauncher() {}

DemoAppLauncher::~DemoAppLauncher() {}

void DemoAppLauncher::StartDemoAppLaunch() {
  DVLOG(1) << "Launching demo app...";
  // user_id = DemoAppUserId, force_emphemeral = true, delegate = this.
  kiosk_profile_loader_.reset(new KioskProfileLoader(
      user_manager::DemoAccountId(), KioskAppType::kChromeApp, true, this));
  kiosk_profile_loader_->Start();
}

// static
bool DemoAppLauncher::IsDemoAppSession(const AccountId& account_id) {
  return account_id == user_manager::DemoAccountId();
}

void DemoAppLauncher::OnProfileLoaded(Profile* profile) {}

void DemoAppLauncher::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Loading the Kiosk Profile failed: "
             << KioskAppLaunchError::GetErrorMessage(error);
}

void DemoAppLauncher::OnOldEncryptionDetected(const UserContext& user_context) {
  NOTREACHED();
}

}  // namespace chromeos
