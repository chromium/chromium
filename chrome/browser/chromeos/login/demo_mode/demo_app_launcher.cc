// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
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
const base::FilePath::CharType kDefaultDemoAppPath[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/demo_app");

// static
base::FilePath* DemoAppLauncher::demo_app_path_ = NULL;

DemoAppLauncher::DemoAppLauncher() {
  if (!demo_app_path_)
    demo_app_path_ = new base::FilePath(kDefaultDemoAppPath);
}

DemoAppLauncher::~DemoAppLauncher() {
  delete demo_app_path_;
}

void DemoAppLauncher::StartDemoAppLaunch() {
  DVLOG(1) << "Launching demo app...";
  // user_id = DemoAppUserId, force_emphemeral = true, delegate = this.
  kiosk_profile_loader_.reset(
      new KioskProfileLoader(user_manager::DemoAccountId(), true, this));
  kiosk_profile_loader_->Start();
}

// static
bool DemoAppLauncher::IsDemoAppSession(const AccountId& account_id) {
  return account_id == user_manager::DemoAccountId();
}

// static
void DemoAppLauncher::SetDemoAppPathForTesting(const base::FilePath& path) {
  delete demo_app_path_;
  demo_app_path_ = new base::FilePath(path);
}

void DemoAppLauncher::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting demo app launch.";

  kiosk_profile_loader_.reset();

  // Load our demo app, then launch it.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  CHECK(demo_app_path_);
  const std::string extension_id = extension_service->component_loader()->Add(
      IDR_DEMO_APP_MANIFEST, *demo_app_path_);

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // We've already done too much setup at this point to just return out, it
    // is safer to just restart.
    chrome::AttemptUserExit();
    return;
  }

  // Disable network before launching the app.
  LOG(WARNING) << "Disabling network before launching demo app..";
  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::Physical(), false,
      chromeos::network_handler::ErrorCallback());

  apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
      extension_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceChromeInternal, true));
  KioskAppManager::Get()->InitSession(profile, extension_id);

  session_manager::SessionManager::Get()->SessionStarted();

  LoginDisplayHost::default_host()->Finalize(base::OnceClosure());
}

void DemoAppLauncher::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Loading the Kiosk Profile failed: "
             << KioskAppLaunchError::GetErrorMessage(error);
}

}  // namespace chromeos
