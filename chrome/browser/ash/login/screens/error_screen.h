// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

namespace ash {

class CaptivePortalWindowProxy;
class ErrorScreenView;

// Controller for the error screen.
class ErrorScreen : public BaseScreen,
                    public NetworkConnectionObserver {
 public:
  explicit ErrorScreen(base::WeakPtr<ErrorScreenView> view);

  ErrorScreen(const ErrorScreen&) = delete;
  ErrorScreen& operator=(const ErrorScreen&) = delete;

  ~ErrorScreen() override;

  CaptivePortalWindowProxy* captive_portal_window_proxy() {
    return captive_portal_window_proxy_.get();
  }

  // Toggles the guest sign-in prompt.
  void AllowGuestSignin(bool allowed);

  // Disallows offline login option. We can't expose publicly an opportunity to
  // allow offline login as it can be controlled by policy.
  // TODO(https://crbug.com/1241511): Should be removed or refactored together
  // with removing the global variables for the offline login allowance.
  void DisallowOfflineLogin();

  // Toggles the offline sign-in.
  static void AllowOfflineLogin(bool allowed);

  // Sets offline flag for focused user.
  static void AllowOfflineLoginPerUser(bool allowed);

  // Initializes captive portal dialog and shows that if needed.
  virtual void FixCaptivePortal();

  NetworkError::UIState GetUIState() const;
  NetworkError::ErrorState GetErrorState() const;

  // Returns id of the screen behind error screen ("caller" screen).
  // Returns `OOBE_SCREEN_UNKNOWN` if error screen isn't the current screen.
  OobeScreenId GetParentScreen() const;

  // Called when we're asked to hide captive portal dialog.
  void HideCaptivePortal();

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

  // Toggles the connection pending indicator.
  void ShowConnectingIndicator(bool show);

  // Makes error persistent (e.g. non-closeable).
  void SetIsPersistentError(bool is_persistent);

  // Register a callback to be invoked when the user indicates that an attempt
  // to connect to the network should be made.
  base::CallbackListSubscription RegisterConnectRequestCallback(
      base::RepeatingClosure callback);

  // Creates an instance of CaptivePortalWindowProxy, if one has not already
  // been created.
  void MaybeInitCaptivePortalWindowProxy(content::WebContents* web_contents);

  void ShowNetworkErrorMessage(NetworkStateInformer::State state,
                               NetworkError::ErrorReason reason);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  // Handle user action to open captive portal page.
  void ShowCaptivePortal();

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

  // Handle uses action to reboot device.
  void OnRebootButtonClicked();

  // Handle user action to cancel the screen and return to user pods.
  void OnCancelButtonClicked();

  // Handle user action to reload gaia.
  void OnReloadGaiaClicked();

  // Handle user action to continue app launch.
  void OnContinueAppLaunchButtonClicked();

  // Handle user action to open learn more.
  void LaunchHelpApp(int help_topic_id);

  // If show is true offline login flow is enabled from the error screen.
  void ShowOfflineLoginOption(bool show);

  // Handle user action to login in offline mode.
  void OnOfflineLoginClicked();

  // Handles the response of an ownership check and starts the guest session if
  // applicable.
  void StartGuestSessionAfterOwnershipCheck(
      DeviceSettingsService::OwnershipStatus ownership_status);

  bool is_persistent_ = false;

  base::WeakPtr<ErrorScreenView> view_;

  // Proxy which manages showing of the window for captive portal entering.
  std::unique_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Network state informer used to keep error screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  NetworkError::UIState ui_state_ = NetworkError::UI_STATE_UNKNOWN;
  NetworkError::ErrorState error_state_ = NetworkError::ERROR_STATE_UNKNOWN;

  OobeScreenId parent_screen_ = OOBE_SCREEN_UNKNOWN;

  // Optional callback that is called when NetworkError screen is hidden.
  base::OnceClosure on_hide_callback_;

  // Callbacks to be invoked when a connection attempt is requested.
  base::RepeatingCallbackList<void()> connect_request_callbacks_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  base::WeakPtrFactory<ErrorScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ERROR_SCREEN_H_
