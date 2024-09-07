// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
#define ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_

#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login/security_token_request_controller.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/management_disclosure_client.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace ash {

class SystemTrayNotifier;
enum class ParentCodeValidationResult;
enum class SupervisedAction;

// LoginScreenController implements LoginScreen and wraps the LoginScreenClient
// interface. This lets a consumer of ash provide a LoginScreenClient, which we
// will dispatch to if one has been provided to us. This could send requests to
// LoginScreenClient and also handle requests from the consumer via the
// LoginScreen interface.
class ASH_EXPORT LoginScreenController : public LoginScreen,
                                         public KioskAppMenu,
                                         public SystemTrayObserver {
 public:

  using OnShownCallback = base::OnceCallback<void(bool did_show)>;
  // Callback for authentication checks. |success| is nullopt if an
  // authentication check did not run, otherwise it is true/false if auth
  // succeeded/failed.
  using OnAuthenticateCallback =
      base::OnceCallback<void(std::optional<bool> success)>;

  explicit LoginScreenController(SystemTrayNotifier* system_tray_notifier);

  LoginScreenController(const LoginScreenController&) = delete;
  LoginScreenController& operator=(const LoginScreenController&) = delete;

  ~LoginScreenController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Check to see if an authentication attempt is in-progress.
  bool IsAuthenticating() const;

  // Check to see if an authentication callback is executing.
  bool IsAuthenticationCallbackExecuting() const;

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
  ParentCodeValidationResult ValidateParentAccessCode(
      const AccountId& account_id,
      base::Time validation_time,
      const std::string& code);
  bool GetSecurityTokenPinRequestCanceled() const;
  void OnFocusPod(const AccountId& account_id);
  void CancelAddUser();
  void ShowGuestTosScreen();
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id);
  void FocusLockScreenApps(bool reverse);
  void ShowGaiaSignin(const AccountId& prefilled_account);
  void StartUserRecovery(const AccountId& account_to_recover);
  void ShowOsInstallScreen();
  void OnRemoveUserWarningShown();
  void RemoveUser(const AccountId& account_id);
  void LaunchPublicSession(const AccountId& account_id,
                           const std::string& locale,
                           const std::string& input_method);
  void RequestPublicSessionKeyboardLayouts(const AccountId& account_id,
                                           const std::string& locale);
  void HandleAccelerator(ash::LoginAcceleratorAction action);
  void ShowAccountAccessHelpApp(gfx::NativeWindow parent_window);
  void ShowParentAccessHelpApp();
  void ShowLockScreenNotificationSettings();
  void FocusOobeDialog();

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
  void EnableShelfButtons(bool enable) override;
  void SetIsFirstSigninStep(bool is_first) override;
  void ShowParentAccessButton(bool show) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  void SetManagementDisclosureClient(
      ManagementDisclosureClient* client) override;
  std::unique_ptr<ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker()
      override;

  void RequestSecurityTokenPin(SecurityTokenPinRequest request) override;
  void ClearSecurityTokenPinRequest() override;
  views::Widget* GetLoginWindowWidget() override;

  // KioskAppMenu:
  void SetKioskApps(const std::vector<KioskAppMenuEntry>& kiosk_apps) override;
  void ConfigureKioskCallbacks(
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu) override;

  AuthenticationStage authentication_stage() const {
    return authentication_stage_;
  }

  // Set authentication stage. Also notifies the observers.
  void SetAuthenticationStage(AuthenticationStage authentication_stage);

  // Called when Login or Lock screen is destroyed.
  void OnLockScreenDestroyed();

  LoginDataDispatcher* data_dispatcher() { return &login_data_dispatcher_; }

  void NotifyLoginScreenShown();

  // Management disclosure client is used to communicate with chrome for
  // LockScreen disclosure.
  ManagementDisclosureClient* GetManagementDisclosureClient();

 private:
  void OnAuthenticateComplete(OnAuthenticateCallback callback, bool success);

  // Common code that is called when the login/lock screen is shown.
  void OnShow();

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnSystemTrayBubbleShown() override;

  LoginDataDispatcher login_data_dispatcher_;

  raw_ptr<LoginScreenClient, DanglingUntriaged> client_ = nullptr;

  AuthenticationStage authentication_stage_ = AuthenticationStage::kIdle;

  raw_ptr<SystemTrayNotifier> system_tray_notifier_;

  SecurityTokenRequestController security_token_request_controller_;

  // If set to false, all auth requests will forcibly fail.
  ForceFailAuth force_fail_auth_for_debug_overlay_ = ForceFailAuth::kOff;

  // Client to communicate with chrome for displaying the management disclosure.
  raw_ptr<ManagementDisclosureClient> management_disclosure_client_ = nullptr;

  base::WeakPtrFactory<LoginScreenController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_LOGIN_SCREEN_CONTROLLER_H_
