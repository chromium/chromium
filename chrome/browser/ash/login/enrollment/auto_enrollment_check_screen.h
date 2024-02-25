// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace policy {
class AutoEnrollmentController;
}  // namespace policy

namespace ash {

class ErrorScreensHistogramHelper;

// Handles the control flow after OOBE auto-update completes to wait for the
// enterprise auto-enrollment check that happens as part of OOBE. This includes
// keeping track of current auto-enrollment state and displaying and updating
// the error screen upon failures. Similar to a screen controller, but it
// doesn't actually drive a dedicated screen.
class AutoEnrollmentCheckScreen : public BaseScreen,
                                  public NetworkStateHandlerObserver {
 public:
  enum class Result {
    NEXT,
    NOT_APPLICABLE,
  };
  using TView = AutoEnrollmentCheckScreenView;

  AutoEnrollmentCheckScreen(
      base::WeakPtr<AutoEnrollmentCheckScreenView> view,
      ErrorScreen* error_screen,
      const base::RepeatingCallback<void(Result result)>& exit_callback);

  AutoEnrollmentCheckScreen(const AutoEnrollmentCheckScreen&) = delete;
  AutoEnrollmentCheckScreen& operator=(const AutoEnrollmentCheckScreen&) =
      delete;

  ~AutoEnrollmentCheckScreen() override;

  static std::string GetResultString(Result result);

  void set_auto_enrollment_controller(
      policy::AutoEnrollmentController* auto_enrollment_controller) {
    auto_enrollment_controller_ = auto_enrollment_controller;
  }

  void set_exit_callback_for_testing(
      const base::RepeatingCallback<void(Result result)>& callback) {
    exit_callback_ = callback;
  }

  // NetworkStateHandlerObserver
  void PortalStateChanged(
      const NetworkState* default_network,
      const NetworkState::PortalState portal_state) override;
  void OnShuttingDown() override;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  bool MaybeSkip(WizardContext& context) override;

  // Runs `exit_callback_` - used to prevent `exit_callback_` from running after
  // `this` has been destroyed (by wrapping it with a callback bound to a weak
  // ptr).
  void RunExitCallback(Result result) { exit_callback_.Run(result); }

 private:
  // Handles update notifications regarding the auto-enrollment check.
  void OnAutoEnrollmentCheckProgressed(policy::AutoEnrollmentState state);

  // Handles a state update, updating the UI and saving the state.
  void UpdateState(NetworkState::PortalState new_captive_portal_state);

  // Configures the UI to reflect the updated captive portal state.
  // Returns true if a UI change has been made.
  bool ShowCaptivePortalState(
      NetworkState::PortalState new_captive_portal_state);

  // Configures the UI to reflect `new_auto_enrollment_state`. Returns true if
  // and only if a UI change has been made.
  bool ShowAutoEnrollmentState(
      policy::AutoEnrollmentState new_auto_enrollment_state);

  // Configures the error screen.
  void ShowErrorScreen(NetworkError::ErrorState error_state);

  // Passed as a callback to the error screen when it's shown. Called when the
  // error screen gets hidden.
  void OnErrorScreenHidden();

  // Asynchronously signals completion. The owner might destroy `this` in
  // response, so no code should be run after the completion of a message loop
  // task, in which this function was called.
  void SignalCompletion();

  // Returns whether enrollment check was completed and decision was made.
  bool IsCompleted() const;

  // The user requested a connection attempt to be performed.
  void OnConnectRequested();

  // Returns true if the `error` blocks the state determination process and must
  // be addressed.
  bool IsBlockingError(const policy::AutoEnrollmentError& error) const;

  // Returns true if an error response from the server should cause a network
  // error screen to be displayed and block the wizard from continuing. If false
  // is returned, an error response from the server is treated as "no enrollment
  // necessary".
  bool ShouldBlockOnServerError() const;

  // Clears the cached state so that the check can be retried.
  void ClearState();

  base::WeakPtr<AutoEnrollmentCheckScreenView> view_;
  raw_ptr<ErrorScreen> error_screen_;
  base::RepeatingCallback<void(Result result)> exit_callback_;
  raw_ptr<policy::AutoEnrollmentController> auto_enrollment_controller_ =
      nullptr;

  base::CallbackListSubscription auto_enrollment_progress_subscription_;

  NetworkState::PortalState captive_portal_state_ =
      NetworkState::PortalState::kUnknown;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::CallbackListSubscription connect_request_subscription_;

  base::WeakPtrFactory<AutoEnrollmentCheckScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
