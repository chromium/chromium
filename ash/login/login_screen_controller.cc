// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/focus_cycler.h"
#include "ash/login/security_token_request_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_screen_client.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/account_id/account_id.h"
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
    if (!status_area) {
      continue;
    }
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
  system_tray_notifier_->AddSystemTrayObserver(this);
}

LoginScreenController::~LoginScreenController() {
  system_tray_notifier_->RemoveSystemTrayObserver(this);
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
    std::move(callback).Run(absl::nullopt);
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
      LOG(WARNING) << "crbug.com/1339004 : Dummy auth state";
      authentication_stage_ = AuthenticationStage::kDoAuthenticate;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         false),
          base::Seconds(1));
      return;
  }

  LOG(WARNING) << "crbug.com/1339004 : started authentication";
  authentication_stage_ = AuthenticationStage::kDoAuthenticate;

  if (authenticated_by_pin) {
    DCHECK(base::ContainsOnlyChars(password, "0123456789"));
  }

  client_->AuthenticateUserWithPasswordOrPin(
      account_id, password, authenticated_by_pin,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginScreenController::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  // TODO(jdufault): integrate this into authenticate stage after mojom is
  // refactored to use a callback.
  if (!client_) {
    return;
  }
  client_->AuthenticateUserWithEasyUnlock(account_id);
}

void LoginScreenController::AuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    OnAuthenticateCallback callback) {
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!client_) {
    std::move(callback).Run(/*success=*/absl::nullopt);
    return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;
  client_->AuthenticateUserWithChallengeResponse(
      account_id,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

ParentCodeValidationResult LoginScreenController::ValidateParentAccessCode(
    const AccountId& account_id,
    base::Time validation_time,
    const std::string& code) {
  DCHECK(!validation_time.is_null());

  if (!client_) {
    return ParentCodeValidationResult::kInternalError;
  }

  return client_->ValidateParentAccessCode(account_id, code, validation_time);
}

bool LoginScreenController::GetSecurityTokenPinRequestCanceled() const {
  return security_token_request_controller_.request_canceled();
}

void LoginScreenController::OnFocusPod(const AccountId& account_id) {
  GetModel()->NotifyFocusPod(account_id);
  if (!client_) {
    return;
  }
  client_->OnFocusPod(account_id);
}

void LoginScreenController::OnNoPodFocused() {
  GetModel()->NotifyFocusPod(EmptyAccountId());
  if (!client_) {
    return;
  }
  client_->OnNoPodFocused();
}

void LoginScreenController::LoadWallpaper(const AccountId& account_id) {
  if (!client_) {
    return;
  }
  client_->LoadWallpaper(account_id);
}

void LoginScreenController::SignOutUser() {
  if (!client_) {
    return;
  }
  client_->SignOutUser();
}

void LoginScreenController::CancelAddUser() {
  if (!client_) {
    return;
  }
  client_->CancelAddUser();
}

void LoginScreenController::LoginAsGuest() {
  if (!client_) {
    return;
  }
  client_->LoginAsGuest();
}

void LoginScreenController::ShowGuestTosScreen() {
  if (!client_) {
    return;
  }
  client_->ShowGuestTosScreen();
}

void LoginScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!client_) {
    return;
  }
  client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LoginScreenController::FocusLockScreenApps(bool reverse) {
  if (!client_) {
    return;
  }
  client_->FocusLockScreenApps(reverse);
}

void LoginScreenController::ShowGaiaSignin(const AccountId& prefilled_account) {
  if (!client_) {
    return;
  }
  client_->ShowGaiaSignin(prefilled_account);
}

void LoginScreenController::ShowOsInstallScreen() {
  if (!client_) {
    return;
  }
  client_->ShowOsInstallScreen();
}

void LoginScreenController::OnRemoveUserWarningShown() {
  if (!client_) {
    return;
  }
  client_->OnRemoveUserWarningShown();
}

void LoginScreenController::RemoveUser(const AccountId& account_id) {
  if (!client_) {
    return;
  }
  client_->RemoveUser(account_id);
}

void LoginScreenController::LaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!client_) {
    return;
  }
  client_->LaunchPublicSession(account_id, locale, input_method);
}

void LoginScreenController::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  if (!client_) {
    return;
  }
  client_->RequestPublicSessionKeyboardLayouts(account_id, locale);
}

void LoginScreenController::SetClient(LoginScreenClient* client) {
  client_ = client;
}

LoginScreenModel* LoginScreenController::GetModel() {
  return &login_data_dispatcher_;
}

void LoginScreenController::ShowKioskAppError(const std::string& message) {
  ToastData toast_data("KioskAppError", ToastCatalogName::kKioskAppError,
                       base::UTF8ToUTF16(message), ToastData::kInfiniteDuration,
                       /*visible_on_lock_screen=*/true,
                       /*has_dismiss_button=*/true);
  Shell::Get()->toast_manager()->Show(std::move(toast_data));
}

void LoginScreenController::FocusLoginShelf(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (Shell::GetPrimaryRootWindowController()->IsSystemTrayVisible() &&
      (reverse || !shelf->shelf_widget()->GetLoginShelfView()->IsFocusable())) {
    // Focus goes to system tray (status area) if one of the following is true:
    //  - system tray is visible and tab is in reverse order;
    //  - system tray is visible and there is no visible shelf buttons before.
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else if (shelf->shelf_widget()->GetLoginShelfView()->IsFocusable()) {
    // Otherwise focus goes to login shelf buttons when there is any.
    if (features::IsUseLoginShelfWidgetEnabled()) {
      LoginShelfWidget* login_shelf_widget = shelf->login_shelf_widget();
      login_shelf_widget->SetDefaultLastFocusableChild(reverse);
      Shell::Get()->focus_cycler()->FocusWidget(login_shelf_widget);
    } else {
      shelf->shelf_widget()->set_default_last_focusable_child(reverse);
      Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
    }
  } else {
    // No elements to focus on the shelf.
    NOTREACHED();
  }
}

bool LoginScreenController::IsReadyForPassword() {
  return LockScreen::HasInstance() && !IsAuthenticating();
}

void LoginScreenController::EnableAddUserButton(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetAddUserButtonEnabled(enable);
}

void LoginScreenController::EnableShutdownButton(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetShutdownButtonEnabled(enable);
}

void LoginScreenController::EnableShelfButtons(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetButtonEnabled(enable);
}

void LoginScreenController::SetIsFirstSigninStep(bool is_first) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetIsFirstSigninStep(is_first);
}

void LoginScreenController::ShowParentAccessButton(bool show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->ShowParentAccessButton(show);
}

void LoginScreenController::SetAllowLoginAsGuest(bool allow_guest) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetAllowLoginAsGuest(allow_guest);
}

std::unique_ptr<ScopedGuestButtonBlocker>
LoginScreenController::GetScopedGuestButtonBlocker() {
  return Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->GetScopedGuestButtonBlocker();
}

void LoginScreenController::RequestSecurityTokenPin(
    SecurityTokenPinRequest request) {
  security_token_request_controller_.SetPinUiState(std::move(request));
}

void LoginScreenController::ClearSecurityTokenPinRequest() {
  security_token_request_controller_.ClosePinUi();
}

views::Widget* LoginScreenController::GetLoginWindowWidget() {
  return client_ ? client_->GetLoginWindowWidget() : nullptr;
}

void LoginScreenController::ShowLockScreen() {
  CHECK(!LockScreen::HasInstance());
  OnShow();
  LockScreen::Show(LockScreen::ScreenType::kLock);
}

void LoginScreenController::ShowLoginScreen() {
  CHECK(!LockScreen::HasInstance());
  // Login screen can only be used during login.
  session_manager::SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  CHECK(session_state == session_manager::SessionState::LOGIN_PRIMARY ||
        session_state == session_manager::SessionState::LOGIN_SECONDARY)
      << "Not showing login screen since session state is "
      << static_cast<int>(session_state);

  OnShow();
  // TODO(jdufault): rename LockScreen to LoginScreen.
  LockScreen::Show(LockScreen::ScreenType::kLogin);
}

void LoginScreenController::SetKioskApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->SetKioskApps(kiosk_apps);
}

void LoginScreenController::ConfigureKioskCallbacks(
    const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
    const base::RepeatingClosure& on_show_menu) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->GetLoginShelfView()
      ->ConfigureKioskCallbacks(launch_app, on_show_menu);
}

void LoginScreenController::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  if (!client_) {
    return;
  }
  client_->HandleAccelerator(action);
}

void LoginScreenController::ShowAccountAccessHelpApp(
    gfx::NativeWindow parent_window) {
  client_->ShowAccountAccessHelpApp(parent_window);
}

void LoginScreenController::ShowParentAccessHelpApp() {
  client_->ShowParentAccessHelpApp();
}

void LoginScreenController::ShowLockScreenNotificationSettings() {
  client_->ShowLockScreenNotificationSettings();
}

void LoginScreenController::FocusOobeDialog() {
  if (!client_) {
    return;
  }
  client_->FocusOobeDialog();
}

void LoginScreenController::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  LOG(WARNING) << "crbug.com/1339004 : authentication complete";
  authentication_stage_ = AuthenticationStage::kUserCallback;
  std::move(callback).Run(absl::make_optional<bool>(success));
  LOG(WARNING) << "crbug.com/1339004 : triggered callback";
  authentication_stage_ = AuthenticationStage::kIdle;

  // During smart card login flow, multiple security token requests can be made.
  // If the user cancels one, all others should also be canceled.
  // At this point, the flow is ending and new security token requests are
  // displayed again.
  security_token_request_controller_.ResetRequestCanceled();
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
  if (!client_) {
    return;
  }
  client_->OnFocusLeavingSystemTray(reverse);
}

void LoginScreenController::OnSystemTrayBubbleShown() {
  if (!client_) {
    return;
  }
  client_->OnSystemTrayBubbleShown();
}

void LoginScreenController::OnLockScreenDestroyed() {
  DCHECK_EQ(authentication_stage_, AuthenticationStage::kIdle);

  // Still handle it to avoid crashes during Login/Lock/Unlock flows.
  authentication_stage_ = AuthenticationStage::kIdle;
  SetSystemTrayVisibility(SystemTrayVisibility::kAll);
}

void LoginScreenController::NotifyLoginScreenShown() {
  if (!client_) {
    return;
  }
  client_->OnLoginScreenShown();
}

}  // namespace ash
