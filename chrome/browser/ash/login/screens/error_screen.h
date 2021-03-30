// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_

#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/login/auth/login_performer.h"
#include "chromeos/network/network_connection_observer.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

namespace chromeos {

class CaptivePortalWindowProxy;
class ErrorScreenView;

// Controller for the error screen.
class ErrorScreen : public BaseScreen,
                    public LoginPerformer::Delegate,
                    public NetworkConnectionObserver {
 public:
  // TODO(jdufault): Some of these are no longer used and can be removed. See
  // crbug.com/672142.
  static const char kUserActionConfigureCertsButtonClicked[];
  static const char kUserActionDiagnoseButtonClicked[];
  static const char kUserActionLaunchOobeGuestSessionClicked[];
  static const char kUserActionLocalStateErrorPowerwashButtonClicked[];
  static const char kUserActionRebootButtonClicked[];
  static const char kUserActionShowCaptivePortalClicked[];
  static const char kUserActionNetworkConnected[];
  static const char kUserActionReloadGaia[];
  static const char kUserActionCancelReset[];
  static const char kUserActionCancel[];

  explicit ErrorScreen(ErrorScreenView* view);
  ~ErrorScreen() override;

  CaptivePortalWindowProxy* captive_portal_window_proxy() {
    return captive_portal_window_proxy_.get();
  }

  // Toggles the guest sign-in prompt.
  void AllowGuestSignin(bool allowed);

  // Toggles the offline sign-in.
  void AllowOfflineLogin(bool allowed);

  // Sets offline flag for focused user.
  void AllowOfflineLoginPerUser(bool allowed);

  // Initializes captive portal dialog and shows that if needed.
  virtual void FixCaptivePortal();

  NetworkError::UIState GetUIState() const;
  NetworkError::ErrorState GetErrorState() const;

  // Returns id of the screen behind error screen ("caller" screen).
  // Returns OobeScreen::SCREEN_UNKNOWN if error screen isn't the current
  // screen.
  OobeScreenId GetParentScreen() const;

  // Called when we're asked to hide captive portal dialog.
  void HideCaptivePortal();

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call Unbind().
  void OnViewDestroyed(ErrorScreenView* view);

  // Sets current UI state.
  virtual void SetUIState(NetworkError::UIState ui_state);

  // Sets current error screen content according to current UI state,
  // `error_state`, and `network`.
  virtual void SetErrorState(NetworkError::ErrorState error_state,
                             const std::string& network);

  // Sets "parent screen" i.e. one that has initiated this network error screen
  // instance.
  void SetParentScreen(OobeScreenId parent_screen);

  // Sets callback that is called on hide.
  void SetHideCallback(base::OnceClosure on_hide);

  // Shows captive portal dialog.
  void ShowCaptivePortal();

  // Toggles the connection pending indicator.
  void ShowConnectingIndicator(bool show);

  // Makes error persistent (e.g. non-closable).
  void SetIsPersistentError(bool is_persistent);

  // Register a callback to be invoked when the user indicates that an attempt
  // to connect to the network should be made.
  base::CallbackListSubscription RegisterConnectRequestCallback(
      base::RepeatingClosure callback);

  // Creates an instance of CaptivePortalWindowProxy, if one has not already
  // been created.
  void MaybeInitCaptivePortalWindowProxy(content::WebContents* web_contents);

  // Actually show or hide the screen. These are called by ErrorScreenHandler;
  // having two show methods (Show/Hide from BaseScreen below) is confusing
  // and this should be cleaned up.
  void DoShow();
  void DoHide();

  void ShowNetworkErrorMessage(NetworkStateInformer::State state,
                               NetworkError::ErrorReason reason);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  // LoginPerformer::Delegate overrides:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnPasswordChangeDetected(const UserContext& user_context) override;
  void AllowlistCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;

  // NetworkConnectionObserver overrides:
  void ConnectToNetworkRequested(const std::string& service_path) override;

  // Default hide_closure for Hide().
  void DefaultHideCallback();

  // Handle user action to configure certificates.
  void OnConfigureCerts();

  // Handle user action to diagnose network configuration.
  void OnDiagnoseButtonClicked();

  // Handle user action to launch guest session from out-of-box.
  void OnLaunchOobeGuestSession();

  // Handle user action to launch Powerwash in case of
  // Local State critical error.
  void OnLocalStateErrorPowerwashButtonClicked();

  // Handle uses action to reboot device.
  void OnRebootButtonClicked();

  // Handle user action to cancel the screen and return to user pods.
  void OnCancelButtonClicked();

  // Handle user action to reload gaia.
  void OnReloadGaiaClicked();

  // If show is true offline login flow is enabled from the error screen.
  void ShowOfflineLoginOption(bool show);

  // Handles the response of an ownership check and starts the guest session if
  // applicable.
  void StartGuestSessionAfterOwnershipCheck(
      DeviceSettingsService::OwnershipStatus ownership_status);

  ErrorScreenView* view_ = nullptr;

  std::unique_ptr<LoginPerformer> guest_login_performer_;

  bool offline_login_allowed_ = false;

  // Additional flag applied on top of offline_login_allowed_ that can block
  // visibility of offline login link when a focused pod user has offline login
  // timer (set by policy) expired. If that happens flag is flipped to false.
  // In all other cases it should be set to a default value of true.
  // Even if a user gets to the (hidden) flow, the offline login may be blocked
  // by checking the policy value there.
  bool offline_login_per_user_allowed_ = true;

  // Proxy which manages showing of the window for captive portal entering.
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Network state informer used to keep error screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  NetworkError::UIState ui_state_ = NetworkError::UI_STATE_UNKNOWN;
  NetworkError::ErrorState error_state_ = NetworkError::ERROR_STATE_UNKNOWN;

  OobeScreenId parent_screen_ = OobeScreen::SCREEN_UNKNOWN;

  // Optional callback that is called when NetworkError screen is hidden.
  base::OnceClosure on_hide_callback_;

  // Callbacks to be invoked when a connection attempt is requested.
  base::RepeatingCallbackList<void()> connect_request_callbacks_;

  base::WeakPtrFactory<ErrorScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ErrorScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_
