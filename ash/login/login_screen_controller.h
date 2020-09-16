// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
#define ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/login/security_token_request_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/system_tray_focus_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace ash {

class SystemTrayNotifier;
enum class SupervisedAction;

// LoginScreenController implements LoginScreen and wraps the LoginScreenClient
// interface. This lets a consumer of ash provide a LoginScreenClient, which we
// will dispatch to if one has been provided to us. This could send requests to
// LoginScreenClient and also handle requests from the consumer via the
// LoginScreen interface.
class ASH_EXPORT LoginScreenController : public LoginScreen,
                                         public KioskAppMenu,
                                         public SystemTrayFocusObserver {
 public:
  // The current authentication stage. Used to get more verbose logging.
  enum class AuthenticationStage {
    kIdle,
    kDoAuthenticate,
    kUserCallback,
  };

  using OnShownCallback = base::OnceCallback<void(bool did_show)>;
  // Callback for authentication checks. |success| is nullopt if an
  // authentication check did not run, otherwise it is true/false if auth
  // succeeded/failed.
  using OnAuthenticateCallback =
      base::OnceCallback<void(base::Optional<bool> success)>;

  explicit LoginScreenController(SystemTrayNotifier* system_tray_notifier);
  ~LoginScreenController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Check to see if an authentication attempt is in-progress.
  bool IsAuthenticating() const;

  // Hash the password and send AuthenticateUser request to LoginScreenClient.
  // LoginScreenClient (in the chrome process) will do the authentication and
  // request to show error messages in the screen if auth fails, or request to
  // clear errors if auth succeeds.
  void AuthenticateUserWithPasswordOrPin(const AccountId& account_id,
                                         const std::string& password,
                                         bool authenticated_by_pin,
                                         OnAuthenticateCallback callback);
  void AuthenticateUserWithEasyUnlock(const AccountId& account_id);
  void AuthenticateUserWithChallengeResponse(const AccountId& account_id,
                                             OnAuthenticateCallback callback);
  bool ValidateParentAccessCode(const AccountId& account_id,
                                base::Time validation_time,
                                const std::string& code);
  bool GetSecurityTokenPinRequestCanceled() const;
  void HardlockPod(const AccountId& account_id);
  void OnFocusPod(const AccountId& account_id);
  void OnNoPodFocused();
  void LoadWallpaper(const AccountId& account_id);
  void SignOutUser();
  void CancelAddUser();
  void LoginAsGuest();
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id);
  void FocusLockScreenApps(bool reverse);
  void ShowGaiaSignin(const AccountId& prefilled_account);
  void OnRemoveUserWarningShown();
  void RemoveUser(const AccountId& account_id);
  void LaunchPublicSession(const AccountId& account_id,
                           const std::string& locale,
                           const std::string& input_method);
  void RequestPublicSessionKeyboardLayouts(const AccountId& account_id,
                                           const std::string& locale);
  void HandleAccelerator(ash::LoginAcceleratorAction action);
  void ShowAccountAccessHelpApp(gfx::NativeWindow parent_window);
  void ShowParentAccessHelpApp(gfx::NativeWindow parent_window);
  void ShowLockScreenNotificationSettings();
  void FocusOobeDialog();
  void NotifyUserActivity();

  // Enable or disable authentication for the debug overlay.
  enum class ForceFailAuth { kOff, kImmediate, kDelayed };
  void set_force_fail_auth_for_debug_overlay(ForceFailAuth force_fail) {
    force_fail_auth_for_debug_overlay_ = force_fail;
  }

  // LoginScreen:
  void SetClient(LoginScreenClient* client) override;
  LoginScreenModel* GetModel() override;
  void ShowLockScreen() override;
  void ShowLoginScreen() override;
  void ShowKioskAppError(const std::string& message) override;
  void FocusLoginShelf(bool reverse) override;
  bool IsReadyForPassword() override;
  void EnableAddUserButton(bool enable) override;
  void EnableShutdownButton(bool enable) override;
  void ShowGuestButtonInOobe(bool show) override;
  void ShowParentAccessButton(bool show) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  std::unique_ptr<ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker()
      override;

  void RequestSecurityTokenPin(SecurityTokenPinRequest request) override;
  void ClearSecurityTokenPinRequest() override;
  bool SetLoginShelfGestureHandler(const base::string16& nudge_text,
                                   const base::RepeatingClosure& fling_callback,
                                   base::OnceClosure exit_callback) override;
  void ClearLoginShelfGestureHandler() override;

  // KioskAppMenu:
  void SetKioskApps(
      const std::vector<KioskAppMenuEntry>& kiosk_apps,
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu) override;

  AuthenticationStage authentication_stage() const {
    return authentication_stage_;
  }

  LoginDataDispatcher* data_dispatcher() { return &login_data_dispatcher_; }

  void NotifyLoginScreenShown();

 private:
  void OnAuthenticateComplete(OnAuthenticateCallback callback, bool success);

  // Common code that is called when the login/lock screen is shown.
  void OnShow();

  // SystemTrayFocusObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  LoginDataDispatcher login_data_dispatcher_;

  LoginScreenClient* client_ = nullptr;

  AuthenticationStage authentication_stage_ = AuthenticationStage::kIdle;

  SystemTrayNotifier* system_tray_notifier_;

  SecurityTokenRequestController security_token_request_controller_;

  // If set to false, all auth requests will forcibly fail.
  ForceFailAuth force_fail_auth_for_debug_overlay_ = ForceFailAuth::kOff;

  base::WeakPtrFactory<LoginScreenController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginScreenController);
};

}  // namespace ash

#endif  // ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
