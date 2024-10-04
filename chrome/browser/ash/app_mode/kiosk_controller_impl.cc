// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/crash_recovery_launcher.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/wm/core/wm_core_switches.h"

namespace ash {

namespace {

std::optional<KioskApp> WebAppById(const WebKioskAppManager& manager,
                                   const AccountId& account_id) {
  const WebKioskAppData* data = manager.GetAppByAccountId(account_id);
  if (!data) {
    return std::nullopt;
  }
  return KioskApp(KioskAppId::ForWebApp(account_id), data->name(), data->icon(),
                  data->install_url());
}

std::optional<KioskApp> ChromeAppById(const KioskChromeAppManager& manager,
                                      std::string_view chrome_app_id) {
  KioskChromeAppManager::App manager_app;
  if (!manager.GetApp(std::string(chrome_app_id), &manager_app)) {
    return std::nullopt;
  }
  return KioskApp(
      KioskAppId::ForChromeApp(chrome_app_id, manager_app.account_id),
      manager_app.name, manager_app.icon);
}

std::optional<KioskApp> IsolatedWebAppById(const KioskIwaManager& manager,
                                           const AccountId& account_id) {
  const KioskIwaData* app_data = manager.GetApp(account_id);

  if (!app_data) {
    return std::nullopt;
  }

  return KioskApp(KioskAppId::ForIsolatedWebApp(account_id), app_data->name(),
                  app_data->icon());
}

KioskApp EmptyKioskApp(const KioskAppId& app_id) {
  switch (app_id.type) {
    case KioskAppType::kChromeApp:
    case KioskAppType::kIsolatedWebApp:
      return KioskApp{app_id,
                      /*name=*/"",
                      /*icon=*/gfx::ImageSkia(),
                      /*url=*/std::nullopt};
    case KioskAppType::kWebApp:
      return KioskApp{app_id,
                      /*name=*/"",
                      /*icon=*/gfx::ImageSkia(),
                      /*url=*/GURL()};
  }
  NOTREACHED();
}

}  // namespace

KioskControllerImpl::KioskControllerImpl(
    user_manager::UserManager* user_manager) {
  user_manager_observation_.Observe(user_manager);
}

KioskControllerImpl::~KioskControllerImpl() = default;

std::vector<KioskApp> KioskControllerImpl::GetApps() const {
  std::vector<KioskApp> apps;
  AppendWebApps(apps);
  AppendChromeApps(apps);
  if (ash::features::IsIsolatedWebAppKioskEnabled()) {
    AppendIsolatedWebApps(apps);
  }
  return apps;
}

std::optional<KioskApp> KioskControllerImpl::GetAppById(
    const KioskAppId& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (app_id.type) {
    case KioskAppType::kWebApp:
      return WebAppById(web_app_manager_, app_id.account_id);
    case KioskAppType::kChromeApp:
      return ChromeAppById(chrome_app_manager_, app_id.app_id.value());
    case KioskAppType::kIsolatedWebApp:
      return IsolatedWebAppById(iwa_manager_, app_id.account_id);
  }
}

std::optional<KioskApp> KioskControllerImpl::GetAutoLaunchApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (const auto& web_account_id = web_app_manager_.GetAutoLaunchAccountId();
      web_account_id.is_valid()) {
    return WebAppById(web_app_manager_, web_account_id);
  } else if (std::string chrome_app_id = chrome_app_manager_.GetAutoLaunchApp();
             !chrome_app_id.empty()) {
    return ChromeAppById(chrome_app_manager_, chrome_app_id);
  }
  return std::nullopt;
}

void KioskControllerImpl::InitializeKioskSystemSession(
    const KioskAppId& kiosk_app_id,
    Profile* profile,
    const std::optional<std::string>& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!system_session_.has_value())
      << "KioskSystemSession is already initialized";

  system_session_.emplace(profile, kiosk_app_id, app_name);

  switch (kiosk_app_id.type) {
    case KioskAppType::kWebApp:
      web_app_manager_.OnKioskSessionStarted(kiosk_app_id);
      break;
    case KioskAppType::kChromeApp:
      chrome_app_manager_.OnKioskSessionStarted(kiosk_app_id);
      break;
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/361017701): add iwa_manager_.OnKioskSessionStarted.
      NOTIMPLEMENTED();
      break;
  }
}

void KioskControllerImpl::StartSession(const KioskAppId& app_id,
                                       bool is_auto_launch,
                                       LoginDisplayHost* host,
                                       AppLaunchSplashScreen* splash_screen) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(launch_controller_, nullptr);
  CHECK(!system_session_.has_value());

  std::optional<KioskApp> app_maybe = GetAppById(app_id);
  // TODO(b/306117645) change to CHECK and drop `value_or`.
  DUMP_WILL_BE_CHECK(app_maybe.has_value());
  KioskApp app = std::move(app_maybe).value_or(EmptyKioskApp(app_id));

  launch_controller_ = std::make_unique<KioskLaunchController>(
      host,
      /*app_launched_callback=*/
      base::BindOnce(&KioskControllerImpl::OnAppLaunched,
                     base::Unretained(this)),
      /*splash_screen=*/splash_screen,
      /*done_callback=*/
      base::BindOnce(&KioskControllerImpl::OnLaunchComplete,
                     base::Unretained(this)));
  launch_controller_->Start(std::move(app), is_auto_launch);
}

void KioskControllerImpl::StartSessionAfterCrash(const KioskAppId& app,
                                                 Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  crash_recovery_launcher_ =
      std::make_unique<CrashRecoveryLauncher>(CHECK_DEREF(profile), app);
  crash_recovery_launcher_->Start(
      base::BindOnce(&KioskControllerImpl::OnLaunchCompleteAfterCrash,
                     // Safe since `this` owns the `crash_recovery_launcher_`.
                     base::Unretained(this), app, profile));
}

bool KioskControllerImpl::IsSessionStarting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return launch_controller_ != nullptr || crash_recovery_launcher_ != nullptr;
}

void KioskControllerImpl::CancelSessionStart() {
  DeleteLaunchControllerAsync();
}

void KioskControllerImpl::AddProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_NE(launch_controller_, nullptr);
  launch_controller_->AddKioskProfileLoadFailedObserver(observer);
}

void KioskControllerImpl::RemoveProfileLoadFailedObserver(
    KioskProfileLoadFailedObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (launch_controller_) {
    launch_controller_->RemoveKioskProfileLoadFailedObserver(observer);
  }
}

bool KioskControllerImpl::HandleAccelerator(LoginAcceleratorAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return launch_controller_ && launch_controller_->HandleAccelerator(action);
}

void KioskControllerImpl::OnGuestAdded(
    content::WebContents* guest_web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (system_session_.has_value()) {
    system_session_->OnGuestAdded(guest_web_contents);
  }
}

KioskSystemSession* KioskControllerImpl::GetKioskSystemSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!system_session_.has_value()) {
    return nullptr;
  }
  return &system_session_.value();
}

kiosk_vision::TelemetryProcessor*
KioskControllerImpl::GetKioskVisionTelemetryProcessor() {
  return nullptr;
}

kiosk_vision::InternalsPageProcessor*
KioskControllerImpl::GetKioskVisionInternalsPageProcessor() {
  return nullptr;
}

void KioskControllerImpl::OnUserLoggedIn(const user_manager::User& user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!user.IsKioskType()) {
    return;
  }

  const AccountId& kiosk_app_account_id = user.GetAccountId();

  // TODO(bartfab): Add KioskAppUsers to the users_ list and keep metadata like
  // the kiosk_app_id in these objects, removing the need to re-parse the
  // device-local account list here to extract the kiosk_app_id.
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(CrosSettings::Get());
  const auto account = base::ranges::find(device_local_accounts,
                                          kiosk_app_account_id.GetUserEmail(),
                                          &policy::DeviceLocalAccount::user_id);
  std::string kiosk_app_id;
  if (account != device_local_accounts.end()) {
    kiosk_app_id = account->kiosk_app_id;
  } else {
    LOG(ERROR) << "Logged into nonexistent kiosk-app account: "
               << kiosk_app_account_id.GetUserEmail();
    CHECK_IS_TEST();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(::switches::kForceAppMode);
  // This happens in Web kiosks.
  if (!kiosk_app_id.empty()) {
    command_line->AppendSwitchASCII(::switches::kAppId, kiosk_app_id);
  }

  // Disable window animation since kiosk app runs in a single full screen
  // window and window animation causes start-up janks.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);

  // If restoring auto-launched kiosk session, make sure the app is marked
  // as auto-launched.
  if (command_line->HasSwitch(switches::kLoginUser) &&
      command_line->HasSwitch(switches::kAppAutoLaunched) &&
      !kiosk_app_id.empty()) {
    chrome_app_manager_.SetAppWasAutoLaunchedWithZeroDelay(kiosk_app_id);
  }
}

void KioskControllerImpl::OnAppLaunched(
    const KioskAppId& kiosk_app_id,
    Profile* profile,
    const std::optional<std::string>& app_name) {
  InitializeKioskSystemSession(kiosk_app_id, profile, app_name);
}

void KioskControllerImpl::OnLaunchComplete(KioskAppLaunchError::Error error) {
  if (auto* input_controller =
          ui::OzonePlatform::GetInstance()->GetInputController()) {
    input_controller->DisableKeyboardImposterCheck();
  }
  // Delete the launcher so it doesn't end up with dangling references.
  DeleteLaunchControllerAsync();
}

void KioskControllerImpl::DeleteLaunchControllerAsync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Deleted asynchronously since this method is invoked in a callback called by
  // the launcher itself, but don't use `DeleteSoon` to prevent the launcher
  // from outliving `this`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&KioskControllerImpl::DeleteLaunchController,
                                weak_factory_.GetWeakPtr()));
}

void KioskControllerImpl::DeleteLaunchController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  launch_controller_.reset();
}

void KioskControllerImpl::OnLaunchCompleteAfterCrash(
    const KioskAppId& app,
    Profile* profile,
    bool success,
    const std::optional<std::string>& app_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    if (auto* input_controller =
            ui::OzonePlatform::GetInstance()->GetInputController()) {
      input_controller->DisableKeyboardImposterCheck();
    }
    InitializeKioskSystemSession(app, profile, app_name);
  } else {
    chrome::AttemptUserExit();
  }

  // Delete launcher so it doesn't end up with dangling references.
  crash_recovery_launcher_.reset();
}

void KioskControllerImpl::AppendWebApps(std::vector<KioskApp>& apps) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const KioskAppManagerBase::App& web_app : web_app_manager_.GetApps()) {
    apps.emplace_back(KioskAppId::ForWebApp(web_app.account_id), web_app.name,
                      web_app.icon, web_app.url);
  }
}

void KioskControllerImpl::AppendChromeApps(std::vector<KioskApp>& apps) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const KioskAppManagerBase::App& chrome_app :
       chrome_app_manager_.GetApps()) {
    apps.emplace_back(
        KioskAppId::ForChromeApp(chrome_app.app_id, chrome_app.account_id),
        chrome_app.name, chrome_app.icon);
  }
}

void KioskControllerImpl::AppendIsolatedWebApps(
    std::vector<KioskApp>& apps) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const KioskAppManagerBase::App& iwa_app : iwa_manager_.GetApps()) {
    apps.emplace_back(KioskAppId::ForIsolatedWebApp(iwa_app.account_id),
                      iwa_app.name, iwa_app.icon);
  }
}

}  // namespace ash
