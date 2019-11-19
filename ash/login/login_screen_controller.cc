// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include <utility>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_screen_client.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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

LoginScreenController::LoginScreenController(
    SystemTrayNotifier* system_tray_notifier)
    : system_tray_notifier_(system_tray_notifier) {
  system_tray_notifier_->AddSystemTrayFocusObserver(this);
}

LoginScreenController::~LoginScreenController() {
  system_tray_notifier_->RemoveSystemTrayFocusObserver(this);
}

// static
void LoginScreenController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                 bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
    return;
  }
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

  if (!client_) {
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

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;

  // Checking if the password is only formed of numbers with base::StringToInt
  // will easily fail due to numeric limits. ContainsOnlyChars is used instead.
  const bool is_pin =
      authenticated_by_pin && base::ContainsOnlyChars(password, "0123456789");
  client_->AuthenticateUserWithPasswordOrPin(
      account_id, password, is_pin,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LoginScreenController::AuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    OnAuthenticateCallback callback) {
  // It is an error to call this function while an authentication is in
  // progress.
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;
  client_->AuthenticateUserWithExternalBinary(
      account_id,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginScreenController::EnrollUserWithExternalBinary(
    OnAuthenticateCallback callback) {
  if (!client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  client_->EnrollUserWithExternalBinary(base::BindOnce(
      [](OnAuthenticateCallback callback, bool success) {
        std::move(callback).Run(base::make_optional<bool>(success));
      },
      std::move(callback)));
}

void LoginScreenController::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  // TODO(jdufault): integrate this into authenticate stage after mojom is
  // refactored to use a callback.
  if (!client_)
    return;
  client_->AuthenticateUserWithEasyUnlock(account_id);
}

void LoginScreenController::AuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    OnAuthenticateCallback callback) {
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!client_) {
    std::move(callback).Run(/*success=*/base::nullopt);
    return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;
  client_->AuthenticateUserWithChallengeResponse(
      account_id,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool LoginScreenController::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& code,
    base::Time validation_time) {
  if (!client_)
    return false;

  return client_->ValidateParentAccessCode(account_id, code, validation_time);
}

void LoginScreenController::HardlockPod(const AccountId& account_id) {
  if (!client_)
    return;
  client_->HardlockPod(account_id);
}

void LoginScreenController::OnFocusPod(const AccountId& account_id) {
  if (!client_)
    return;
  client_->OnFocusPod(account_id);
}

void LoginScreenController::OnNoPodFocused() {
  if (!client_)
    return;
  client_->OnNoPodFocused();
}

void LoginScreenController::LoadWallpaper(const AccountId& account_id) {
  if (!client_)
    return;
  client_->LoadWallpaper(account_id);
}

void LoginScreenController::SignOutUser() {
  if (!client_)
    return;
  client_->SignOutUser();
}

void LoginScreenController::CancelAddUser() {
  if (!client_)
    return;
  client_->CancelAddUser();
}

void LoginScreenController::LoginAsGuest() {
  if (!client_)
    return;
  client_->LoginAsGuest();
}

void LoginScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!client_)
    return;
  client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LoginScreenController::FocusLockScreenApps(bool reverse) {
  if (!client_)
    return;
  client_->FocusLockScreenApps(reverse);
}

void LoginScreenController::ShowGaiaSignin(bool can_close,
                                           const AccountId& prefilled_account) {
  if (!client_)
    return;
  client_->ShowGaiaSignin(can_close, prefilled_account);
}

void LoginScreenController::OnRemoveUserWarningShown() {
  if (!client_)
    return;
  client_->OnRemoveUserWarningShown();
}

void LoginScreenController::RemoveUser(const AccountId& account_id) {
  if (!client_)
    return;
  client_->RemoveUser(account_id);
}

void LoginScreenController::LaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!client_)
    return;
  client_->LaunchPublicSession(account_id, locale, input_method);
}

void LoginScreenController::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  if (!client_)
    return;
  client_->RequestPublicSessionKeyboardLayouts(account_id, locale);
}

void LoginScreenController::ShowFeedback() {
  if (!client_)
    return;
  client_->ShowFeedback();
}

void LoginScreenController::SetClient(LoginScreenClient* client) {
  client_ = client;
}

LoginScreenModel* LoginScreenController::GetModel() {
  return &login_data_dispatcher_;
}

void LoginScreenController::ShowKioskAppError(const std::string& message) {
  ToastData toast_data(
      "KioskAppError", base::UTF8ToUTF16(message), -1 /*duration_ms*/,
      base::Optional<base::string16>(base::string16()) /*dismiss_text*/,
      true /*visible_on_lock_screen*/);
  Shell::Get()->toast_manager()->Show(toast_data);
}

void LoginScreenController::FocusLoginShelf(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse || !ShelfWidget::IsUsingViewsShelf()) {
    if (!Shell::GetPrimaryRootWindowController()->IsSystemTrayVisible())
      return;
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }
}

bool LoginScreenController::IsReadyForPassword() {
  return LockScreen::HasInstance() && !IsAuthenticating();
}

void LoginScreenController::EnableAddUserButton(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAddUserButtonEnabled(enable);
}

void LoginScreenController::EnableShutdownButton(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetShutdownButtonEnabled(enable);
}

void LoginScreenController::ShowGuestButtonInOobe(bool show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->ShowGuestButtonInOobe(show);
}

void LoginScreenController::ShowParentAccessButton(bool show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->ShowParentAccessButton(show);
}

void LoginScreenController::ShowParentAccessWidget(
    const AccountId& child_account_id,
    ParentAccessWidget::OnExitCallback callback,
    ParentAccessRequestReason reason,
    bool extra_dimmer,
    base::Time validation_time) {
  DCHECK(!ParentAccessWidget::Get());
  ParentAccessWidget::Show(child_account_id, std::move(callback), reason,
                           extra_dimmer, validation_time);
}

void LoginScreenController::SetAllowLoginAsGuest(bool allow_guest) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAllowLoginAsGuest(allow_guest);
}

std::unique_ptr<ScopedGuestButtonBlocker>
LoginScreenController::GetScopedGuestButtonBlocker() {
  return Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->GetScopedGuestButtonBlocker();
}

void LoginScreenController::RequestSecurityTokenPin(
    SecurityTokenPinRequest request) {
  if (!LockScreen::HasInstance()) {
    // Corner case: the PIN request is made at inappropriate time, racing with
    // the lock screen showing/hiding.
    std::move(request.pin_ui_closed_callback).Run();
    return;
  }
  LockScreen::Get()->RequestSecurityTokenPin(std::move(request));
}

void LoginScreenController::ClearSecurityTokenPinRequest() {
  if (!LockScreen::HasInstance()) {
    // Corner case: the request is made at inappropriate time, racing with the
    // lock screen showing/hiding.
    return;
  }
  LockScreen::Get()->ClearSecurityTokenPinRequest();
}

void LoginScreenController::ShowLockScreen() {
  OnShow();
  LockScreen::Show(LockScreen::ScreenType::kLock);
}

void LoginScreenController::ShowLoginScreen() {
  // Login screen can only be used during login.
  CHECK_EQ(session_manager::SessionState::LOGIN_PRIMARY,
           Shell::Get()->session_controller()->GetSessionState())
      << "Not showing login screen since session state is "
      << static_cast<int>(
             Shell::Get()->session_controller()->GetSessionState());

  OnShow();
  // TODO(jdufault): rename LockScreen to LoginScreen.
  LockScreen::Show(LockScreen::ScreenType::kLogin);
}

void LoginScreenController::SetKioskApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps,
    const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetKioskApps(kiosk_apps, launch_app);
}

void LoginScreenController::ShowResetScreen() {
  client_->ShowResetScreen();
}

void LoginScreenController::ShowAccountAccessHelpApp() {
  client_->ShowAccountAccessHelpApp();
}

void LoginScreenController::ShowParentAccessHelpApp() {
  client_->ShowParentAccessHelpApp();
}

void LoginScreenController::ShowLockScreenNotificationSettings() {
  client_->ShowLockScreenNotificationSettings();
}

void LoginScreenController::FocusOobeDialog() {
  if (!client_)
    return;
  client_->FocusOobeDialog();
}

void LoginScreenController::NotifyUserActivity() {
  if (!client_)
    return;
  client_->OnUserActivity();
}

void LoginScreenController::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  authentication_stage_ = AuthenticationStage::kUserCallback;
  std::move(callback).Run(base::make_optional<bool>(success));
  authentication_stage_ = AuthenticationStage::kIdle;
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

void LoginScreenController::OnFocusLeavingSystemTray(bool reverse) {
  if (!client_)
    return;
  client_->OnFocusLeavingSystemTray(reverse);
}

}  // namespace ash
