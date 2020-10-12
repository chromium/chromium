// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_common.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/gaia_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/webui_accelerator_mapping.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/features/feature_session_type.h"
#include "ui/base/ui_base_features.h"

namespace chromeos {
namespace {

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
constexpr int64_t kPolicyServiceInitializationDelayMilliseconds = 100;

void ScheduleCompletionCallbacks(std::vector<base::OnceClosure>&& callbacks) {
  for (auto& callback : callbacks) {
    if (callback.is_null())
      continue;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }
}

}  // namespace

LoginDisplayHostCommon::LoginDisplayHostCommon()
    : keep_alive_(KeepAliveOrigin::LOGIN_DISPLAY_HOST_WEBUI,
                  KeepAliveRestartOption::DISABLED) {
  // Close the login screen on NOTIFICATION_APP_TERMINATING (for the case where
  // shutdown occurs before login completes).
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  BrowserList::AddObserver(this);
}

LoginDisplayHostCommon::~LoginDisplayHostCommon() {
  ScheduleCompletionCallbacks(std::move(completion_callbacks_));
}

void LoginDisplayHostCommon::BeforeSessionStart() {
  session_starting_ = true;
}

void LoginDisplayHostCommon::Finalize(base::OnceClosure completion_callback) {
  // If finalize is called twice the LoginDisplayHost instance will be deleted
  // multiple times.
  CHECK(!is_finalizing_);
  is_finalizing_ = true;

  completion_callbacks_.push_back(std::move(completion_callback));
  OnFinalize();
}

void LoginDisplayHostCommon::FinalizeImmediately() {
  CHECK(!is_finalizing_);
  CHECK(!shutting_down_);
  is_finalizing_ = true;
  shutting_down_ = true;
  OnFinalize();
  Cleanup();
  delete this;
}

KioskLaunchController* LoginDisplayHostCommon::GetKioskLaunchController() {
  return kiosk_launch_controller_.get();
}

void LoginDisplayHostCommon::StartUserAdding(
    base::OnceClosure completion_callback) {
  completion_callbacks_.push_back(std::move(completion_callback));
  OnStartUserAdding();
}

void LoginDisplayHostCommon::StartSignInScreen() {
  PrewarmAuthentication();

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  }

  // Initiate device policy fetching.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  // Run UI-specific logic.
  OnStartSignInScreen();

  // Enable status area after starting sign-in screen, as it may depend on the
  // UI being visible.
  SetStatusAreaVisible(true);
}

void LoginDisplayHostCommon::PrewarmAuthentication() {
  auth_prewarmer_ = std::make_unique<AuthPrewarmer>();
  auth_prewarmer_->PrewarmAuthentication(base::BindOnce(
      &LoginDisplayHostCommon::OnAuthPrewarmDone, weak_factory_.GetWeakPtr()));
}

void LoginDisplayHostCommon::StartDemoAppLaunch() {
  VLOG(1) << "Login >> starting demo app.";
  SetStatusAreaVisible(false);

  demo_app_launcher_ = std::make_unique<DemoAppLauncher>();
  demo_app_launcher_->StartDemoAppLaunch();
}

void LoginDisplayHostCommon::StartKiosk(const KioskAppId& kiosk_app_id,
                                        bool is_auto_launch) {
  VLOG(1) << "Login >> start kiosk of type "
          << static_cast<int>(kiosk_app_id.type);
  SetStatusAreaVisible(false);

  // Wait for the |CrosSettings| to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &LoginDisplayHostCommon::StartKiosk, weak_factory_.GetWeakPtr(),
          kiosk_app_id, is_auto_launch));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the |CrosSettings| are permanently untrusted, refuse to launch a
    // single-app kiosk mode session.
    LOG(ERROR) << "Login >> Refusing to launch single-app kiosk mode.";
    SetStatusAreaVisible(true);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  OnStartAppLaunch();

  int auto_launch_delay = -1;
  if (is_auto_launch) {
    if (!CrosSettings::Get()->GetInteger(
            kAccountsPrefDeviceLocalAccountAutoLoginDelay,
            &auto_launch_delay)) {
      auto_launch_delay = 0;
    }
    DCHECK_EQ(0, auto_launch_delay)
        << "Kiosks do not support non-zero auto-login delays";
  }

  extensions::SetCurrentFeatureSessionType(
      is_auto_launch && auto_launch_delay == 0
          ? extensions::FeatureSessionType::AUTOLAUNCHED_KIOSK
          : extensions::FeatureSessionType::KIOSK);

  kiosk_launch_controller_ =
      std::make_unique<KioskLaunchController>(GetOobeUI());
  kiosk_launch_controller_->Start(kiosk_app_id, is_auto_launch);
}

void LoginDisplayHostCommon::CompleteLogin(const UserContext& user_context) {
  if (GetExistingUserController())
    GetExistingUserController()->CompleteLogin(user_context);
}

void LoginDisplayHostCommon::OnGaiaScreenReady() {
  if (GetExistingUserController()) {
    GetExistingUserController()->OnGaiaScreenReady();
  } else {
    // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
    LOG(ERROR) << "OnGaiaScreenReady: there is no existing user controller";
  }
}

void LoginDisplayHostCommon::SetDisplayEmail(const std::string& email) {
  if (GetExistingUserController())
    GetExistingUserController()->SetDisplayEmail(email);
}

void LoginDisplayHostCommon::SetDisplayAndGivenName(
    const std::string& display_name,
    const std::string& given_name) {
  if (GetExistingUserController())
    GetExistingUserController()->SetDisplayAndGivenName(display_name,
                                                        given_name);
}

void LoginDisplayHostCommon::LoadWallpaper(const AccountId& account_id) {
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void LoginDisplayHostCommon::LoadSigninWallpaper() {
  WallpaperControllerClient::Get()->ShowSigninWallpaper();
}

bool LoginDisplayHostCommon::IsUserAllowlisted(
    const AccountId& account_id,
    const base::Optional<user_manager::UserType>& user_type) {
  if (!GetExistingUserController())
    return true;
  return GetExistingUserController()->IsUserAllowlisted(account_id, user_type);
}

void LoginDisplayHostCommon::CancelPasswordChangedFlow() {
  if (GetExistingUserController())
    GetExistingUserController()->CancelPasswordChangedFlow();

  OnCancelPasswordChangedFlow();
}

void LoginDisplayHostCommon::MigrateUserData(const std::string& old_password) {
  if (GetExistingUserController())
    GetExistingUserController()->MigrateUserData(old_password);
}

void LoginDisplayHostCommon::ResyncUserData() {
  if (GetExistingUserController())
    GetExistingUserController()->ResyncUserData();
}

bool LoginDisplayHostCommon::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  DCHECK(GetOobeUI());
  if (WizardController::default_controller() &&
      WizardController::default_controller()->is_initialized()) {
    if (WizardController::default_controller()->HandleAccelerator(action))
      return true;
  }
  // TODO(crbug.com/1102393): Remove once all accelerators handling is migrated
  // to browser side.
  GetOobeUI()->ForwardAccelerator(MapToWebUIAccelerator(action));
  return true;
}

void LoginDisplayHostCommon::OnBrowserAdded(Browser* browser) {
  // Browsers created before session start (windows opened by extensions, for
  // example) are ignored.
  if (session_starting_) {
    // OnBrowserAdded is called when the browser is created, but not shown yet.
    // Lock window has to be closed at this point so that a browser window
    // exists and the window can acquire input focus.
    OnBrowserCreated();
    registrar_.RemoveAll();
    BrowserList::RemoveObserver(this);
  }
}

void LoginDisplayHostCommon::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_TERMINATING)
    ShutdownDisplayHost();
}

void LoginDisplayHostCommon::OnCancelPasswordChangedFlow() {}

void LoginDisplayHostCommon::OnAuthPrewarmDone() {
  auth_prewarmer_.reset();
}

void LoginDisplayHostCommon::ShutdownDisplayHost() {
  if (shutting_down_)
    return;
  shutting_down_ = true;

  Cleanup();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void LoginDisplayHostCommon::OnStartSignInScreenCommon() {
  kiosk_app_menu_controller_.SendKioskApps();
}

void LoginDisplayHostCommon::ShowGaiaDialogCommon(
    const AccountId& prefilled_account) {
  if (prefilled_account.is_valid()) {
    LoadWallpaper(prefilled_account);
    if (GetLoginDisplay()->delegate()->IsSigninInProgress()) {
      return;
    }
  } else {
    LoadSigninWallpaper();
  }

  DCHECK(GetWizardController());
  GaiaScreen* gaia_screen =
      GaiaScreen::Get(GetWizardController()->screen_manager());
  gaia_screen->LoadOnline(prefilled_account);

  if (chromeos::features::IsChildSpecificSigninEnabled() &&
      !prefilled_account.is_valid()) {
    StartWizard(UserCreationView::kScreenId);
  } else {
    StartWizard(GaiaView::kScreenId);
  }
}

void LoginDisplayHostCommon::Cleanup() {
  ProfileHelper::Get()->ClearSigninProfile(base::DoNothing());
  registrar_.RemoveAll();
  BrowserList::RemoveObserver(this);
}

}  // namespace chromeos
