// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include <utility>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

namespace {

enum class SystemTrayVisibility {
  kNone,     // Tray not visible anywhere.
  kPrimary,  // Tray visible only on primary display.
  kAll,      // Tray visible on all displays.
};

void SetSystemTrayVisibility(SystemTrayVisibility visibility) {
  RootWindowController* primary_window_controller =
      Shell::GetPrimaryRootWindowController();
  for (RootWindowController* window_controller :
       Shell::GetAllRootWindowControllers()) {
    StatusAreaWidget* status_area = window_controller->GetStatusAreaWidget();
    if (!status_area)
      continue;
    if (window_controller == primary_window_controller) {
      status_area->SetSystemTrayVisibility(
          visibility == SystemTrayVisibility::kPrimary ||
          visibility == SystemTrayVisibility::kAll);
    } else {
      status_area->SetSystemTrayVisibility(visibility ==
                                           SystemTrayVisibility::kAll);
    }
  }
}

}  // namespace

LoginScreenController::LoginScreenController() : weak_factory_(this) {}

LoginScreenController::~LoginScreenController() = default;

// static
void LoginScreenController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                 bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
    return;
  }

  // Pref is owned by chrome and flagged as PUBLIC.
  registry->RegisterForeignPref(prefs::kQuickUnlockPinSalt);
}

void LoginScreenController::BindRequest(mojom::LoginScreenRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool LoginScreenController::IsAuthenticating() const {
  return authentication_stage_ != AuthenticationStage::kIdle;
}

void LoginScreenController::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    OnAuthenticateCallback callback) {
  // It is an error to call this function while an authentication is in
  // progress.
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // If auth is disabled by the debug overlay bypass the mojo call entirely, as
  // it will dismiss the lock screen if the password is correct.
  switch (force_fail_auth_for_debug_overlay_) {
    case ForceFailAuth::kOff:
      break;
    case ForceFailAuth::kImmediate:
      OnAuthenticateComplete(std::move(callback), false /*success*/);
      return;
    case ForceFailAuth::kDelayed:
      // Set a dummy authentication stage so that |IsAuthenticating| returns
      // true.
      authentication_stage_ = AuthenticationStage::kDoAuthenticate;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                         weak_factory_.GetWeakPtr(), base::Passed(&callback),
                         false),
          base::TimeDelta::FromSeconds(1));
      return;
  }

  // |DoAuthenticateUser| requires the system salt.
  authentication_stage_ = AuthenticationStage::kGetSystemSalt;
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(
      base::AdaptCallbackForRepeating(
          base::BindOnce(&LoginScreenController::DoAuthenticateUser,
                         weak_factory_.GetWeakPtr(), account_id, password,
                         authenticated_by_pin, std::move(callback))));
}

void LoginScreenController::AuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    OnAuthenticateCallback callback) {
  // It is an error to call this function while an authentication is in
  // progress.
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;
  login_screen_client_->AuthenticateUserWithExternalBinary(
      account_id,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginScreenController::EnrollUserWithExternalBinary(
    OnAuthenticateCallback callback) {
  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  login_screen_client_->EnrollUserWithExternalBinary(base::BindOnce(
      [](OnAuthenticateCallback callback, bool success) {
        std::move(callback).Run(base::make_optional<bool>(success));
      },
      std::move(callback)));
}

void LoginScreenController::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  // TODO(jdufault): integrate this into authenticate stage after mojom is
  // refactored to use a callback.
  if (!login_screen_client_)
    return;
  login_screen_client_->AuthenticateUserWithEasyUnlock(account_id);
}

void LoginScreenController::HardlockPod(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->HardlockPod(account_id);
}

void LoginScreenController::OnFocusPod(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnFocusPod(account_id);
}

void LoginScreenController::OnNoPodFocused() {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnNoPodFocused();
}

void LoginScreenController::LoadWallpaper(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->LoadWallpaper(account_id);
}

void LoginScreenController::SignOutUser() {
  if (!login_screen_client_)
    return;
  login_screen_client_->SignOutUser();
}

void LoginScreenController::CancelAddUser() {
  if (!login_screen_client_)
    return;
  login_screen_client_->CancelAddUser();
}

void LoginScreenController::LoginAsGuest() {
  if (!login_screen_client_)
    return;
  login_screen_client_->LoginAsGuest();
}

void LoginScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LoginScreenController::FocusLockScreenApps(bool reverse) {
  if (!login_screen_client_)
    return;
  login_screen_client_->FocusLockScreenApps(reverse);
}

void LoginScreenController::ShowGaiaSignin(
    bool can_close,
    const base::Optional<AccountId>& prefilled_account) {
  if (!login_screen_client_)
    return;
  login_screen_client_->ShowGaiaSignin(can_close, prefilled_account);
}

void LoginScreenController::OnRemoveUserWarningShown() {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnRemoveUserWarningShown();
}

void LoginScreenController::RemoveUser(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->RemoveUser(account_id);
}

void LoginScreenController::LaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!login_screen_client_)
    return;
  login_screen_client_->LaunchPublicSession(account_id, locale, input_method);
}

void LoginScreenController::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  if (!login_screen_client_)
    return;
  login_screen_client_->RequestPublicSessionKeyboardLayouts(account_id, locale);
}

void LoginScreenController::ShowFeedback() {
  if (!login_screen_client_)
    return;
  login_screen_client_->ShowFeedback();
}

void LoginScreenController::AddObserver(
    LoginScreenControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void LoginScreenController::RemoveObserver(
    LoginScreenControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LoginScreenController::FlushForTesting() {
  login_screen_client_.FlushForTesting();
}

void LoginScreenController::SetClient(mojom::LoginScreenClientPtr client) {
  login_screen_client_ = std::move(client);
}

void LoginScreenController::ShowLockScreen(ShowLockScreenCallback on_shown) {
  OnShow();
  ash::LockScreen::Show(ash::LockScreen::ScreenType::kLock);
  std::move(on_shown).Run(true);
}

void LoginScreenController::ShowLoginScreen(ShowLoginScreenCallback on_shown) {
  // Login screen can only be used during login.
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    LOG(ERROR) << "Not showing login screen since session state is "
               << static_cast<int>(
                      Shell::Get()->session_controller()->GetSessionState());
    std::move(on_shown).Run(false);
    return;
  }

  OnShow();
  // TODO(jdufault): rename ash::LockScreen to ash::LoginScreen.
  ash::LockScreen::Show(ash::LockScreen::ScreenType::kLogin);
  std::move(on_shown).Run(true);
}

void LoginScreenController::ShowErrorMessage(int32_t login_attempts,
                                             const std::string& error_text,
                                             const std::string& help_link_text,
                                             int32_t help_topic_id) {
  NOTIMPLEMENTED();
}

void LoginScreenController::ShowWarningBanner(const base::string16& message) {
  if (DataDispatcher())
    DataDispatcher()->ShowWarningBanner(message);
}

void LoginScreenController::HideWarningBanner() {
  if (DataDispatcher())
    DataDispatcher()->HideWarningBanner();
}

void LoginScreenController::ClearErrors() {
  NOTIMPLEMENTED();
}

void LoginScreenController::ShowUserPodCustomIcon(
    const AccountId& account_id,
    mojom::EasyUnlockIconOptionsPtr icon) {
  DataDispatcher()->ShowEasyUnlockIcon(account_id, icon);
}

void LoginScreenController::HideUserPodCustomIcon(const AccountId& account_id) {
  auto icon_options = mojom::EasyUnlockIconOptions::New();
  icon_options->icon = mojom::EasyUnlockIconId::NONE;
  DataDispatcher()->ShowEasyUnlockIcon(account_id, icon_options);
}

void LoginScreenController::SetAuthType(
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const base::string16& initial_value) {
  if (auth_type == proximity_auth::mojom::AuthType::USER_CLICK) {
    DataDispatcher()->SetTapToUnlockEnabledForUser(account_id,
                                                   true /*enabled*/);
  } else if (auth_type == proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
    DataDispatcher()->SetForceOnlineSignInForUser(account_id);
  } else {
    NOTIMPLEMENTED();
  }
}

void LoginScreenController::SetUserList(
    std::vector<mojom::LoginUserInfoPtr> users) {
  DCHECK(DataDispatcher());

  DataDispatcher()->NotifyUsers(users);
}

void LoginScreenController::SetPinEnabledForUser(const AccountId& account_id,
                                                 bool is_enabled) {
  // Chrome will update pin pod state every time user tries to authenticate.
  // LockScreen is destroyed in the case of authentication success.
  if (DataDispatcher())
    DataDispatcher()->SetPinEnabledForUser(account_id, is_enabled);
}

void LoginScreenController::SetFingerprintState(const AccountId& account_id,
                                                mojom::FingerprintState state) {
  if (DataDispatcher())
    DataDispatcher()->SetFingerprintState(account_id, state);
}

void LoginScreenController::NotifyFingerprintAuthResult(
    const AccountId& account_id,
    bool successful) {
  if (DataDispatcher())
    DataDispatcher()->NotifyFingerprintAuthResult(account_id, successful);
}

void LoginScreenController::SetAvatarForUser(const AccountId& account_id,
                                             mojom::UserAvatarPtr avatar) {
  for (auto& observer : observers_)
    observer.SetAvatarForUser(account_id, avatar);
}

void LoginScreenController::SetAuthEnabledForUser(
    const AccountId& account_id,
    bool is_enabled,
    base::Optional<base::Time> auth_reenabled_time) {
  if (DataDispatcher()) {
    DataDispatcher()->SetAuthEnabledForUser(account_id, is_enabled,
                                            auth_reenabled_time);
  }
}

void LoginScreenController::HandleFocusLeavingLockScreenApps(bool reverse) {
  for (auto& observer : observers_)
    observer.OnFocusLeavingLockScreenApps(reverse);
}

void LoginScreenController::SetSystemInfo(
    bool show_if_hidden,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {
  if (DataDispatcher()) {
    DataDispatcher()->SetSystemInfo(show_if_hidden, os_version_label_text,
                                    enterprise_info_text, bluetooth_name);
  }
}

void LoginScreenController::IsReadyForPassword(
    IsReadyForPasswordCallback callback) {
  std::move(callback).Run(LockScreen::HasInstance() && !IsAuthenticating());
}

void LoginScreenController::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  if (DataDispatcher())
    DataDispatcher()->SetPublicSessionDisplayName(account_id, display_name);
}

void LoginScreenController::SetPublicSessionLocales(
    const AccountId& account_id,
    std::vector<mojom::LocaleItemPtr> locales,
    const std::string& default_locale,
    bool show_advanced_view) {
  if (DataDispatcher()) {
    DataDispatcher()->SetPublicSessionLocales(
        account_id, locales, default_locale, show_advanced_view);
  }
}

void LoginScreenController::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    std::vector<mojom::InputMethodItemPtr> keyboard_layouts) {
  if (DataDispatcher()) {
    DataDispatcher()->SetPublicSessionKeyboardLayouts(account_id, locale,
                                                      keyboard_layouts);
  }
}

void LoginScreenController::SetKioskApps(
    std::vector<mojom::KioskAppInfoPtr> kiosk_apps) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetKioskApps(std::move(kiosk_apps));
}

void LoginScreenController::ShowKioskAppError(const std::string& message) {
  ToastData toast_data(
      "KioskAppError", base::UTF8ToUTF16(message), -1 /*duration_ms*/,
      base::Optional<base::string16>(base::string16()) /*dismiss_text*/,
      true /*visible_on_lock_screen*/);
  Shell::Get()->toast_manager()->Show(toast_data);
}

void LoginScreenController::NotifyOobeDialogState(
    mojom::OobeDialogState state) {
  for (auto& observer : observers_)
    observer.OnOobeDialogStateChanged(state);
}

void LoginScreenController::SetAllowLoginAsGuest(bool allow_guest) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAllowLoginAsGuest(allow_guest);
}

void LoginScreenController::SetShowGuestButtonForGaiaScreen(bool can_show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetShowGuestButtonForGaiaScreen(can_show);
}

void LoginScreenController::FocusLoginShelf(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse) {
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }
}

void LoginScreenController::SetAddUserButtonEnabled(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAddUserButtonEnabled(enable);
}

void LoginScreenController::LaunchKioskApp(const std::string& app_id) {
  login_screen_client_->LaunchKioskApp(app_id);
}

void LoginScreenController::LaunchArcKioskApp(const AccountId& account_id) {
  login_screen_client_->LaunchArcKioskApp(account_id);
}

void LoginScreenController::ShowResetScreen() {
  login_screen_client_->ShowResetScreen();
}

void LoginScreenController::ShowAccountAccessHelpApp() {
  login_screen_client_->ShowAccountAccessHelpApp();
}

void LoginScreenController::FocusOobeDialog() {
  login_screen_client_->FocusOobeDialog();
}

void LoginScreenController::DoAuthenticateUser(const AccountId& account_id,
                                               const std::string& password,
                                               bool authenticated_by_pin,
                                               OnAuthenticateCallback callback,
                                               const std::string& system_salt) {
  // TODO(jdufault): Simplify this, system_salt is no longer used so fetching
  // the system salt can be skipped.
  authentication_stage_ = AuthenticationStage::kDoAuthenticate;

  int dummy_value;
  bool is_pin =
      authenticated_by_pin && base::StringToInt(password, &dummy_value);
  login_screen_client_->AuthenticateUserWithPasswordOrPin(
      account_id, password, is_pin,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LoginScreenController::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  authentication_stage_ = AuthenticationStage::kUserCallback;
  std::move(callback).Run(base::make_optional<bool>(success));
  authentication_stage_ = AuthenticationStage::kIdle;
}

LoginDataDispatcher* LoginScreenController::DataDispatcher() const {
  if (!ash::LockScreen::HasInstance())
    return nullptr;
  return ash::LockScreen::Get()->data_dispatcher();
}

void LoginScreenController::OnShow() {
  SetSystemTrayVisibility(SystemTrayVisibility::kPrimary);
  if (authentication_stage_ != AuthenticationStage::kIdle) {
    AuthenticationStage authentication_stage = authentication_stage_;
    base::debug::Alias(&authentication_stage);
    LOG(FATAL) << "Unexpected authentication stage "
               << static_cast<int>(authentication_stage_);
  }
}

}  // namespace ash
