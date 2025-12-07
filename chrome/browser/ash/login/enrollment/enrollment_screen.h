// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/backoff_entry.h"

namespace base {
class ElapsedTimer;
}

namespace ash {

class ErrorScreen;
class ScreenManager;

namespace test {
class EnrollmentHelperMixin;
}

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public BaseScreen,
      public EnrollmentLauncher::EnrollmentStatusConsumer,
      public EnrollmentScreenView::Controller,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum class Result {
    COMPLETED,
    BACK,
    SKIPPED_FOR_TESTS,
    TPM_ERROR,
    TPM_DBUS_ERROR,
    BACK_TO_AUTO_ENROLLMENT_CHECK,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  using TpmStatusCallback = chromeos::TpmManagerClient::TakeOwnershipCallback;
  EnrollmentScreen(base::WeakPtr<EnrollmentScreenView> view,
                   ErrorScreen* error_screen,
                   const ScreenExitCallback& exit_callback);

  EnrollmentScreen(const EnrollmentScreen&) = delete;
  EnrollmentScreen& operator=(const EnrollmentScreen&) = delete;

  ~EnrollmentScreen() override;

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Setup how this screen will handle enrollment.
  void SetEnrollmentConfig(const policy::EnrollmentConfig& enrollment_config);

  // EnrollmentScreenView::Controller implementation:
  void OnLoginDone(login::OnlineSigninArtifacts signin_artifacts,
                   int license_type,
                   const std::string& auth_code) override;
  void OnRetry() override;
  void OnCancel() override;
  void OnConfirmationClosed() override;
  void OnDeviceAttributeProvided(const std::string& asset_id,
                                 const std::string& location) override;
  void OnIdentifierEntered(const std::string& email) override;
  void OnFirstShow() override;
  void OnFrameLoadingCompleted() override;

  // Shows skip enrollment dialogue confiromation for license packaged devices.
  void ShowSkipEnrollmentDialogue();

  // EnrollmentLauncher::EnrollmentStatusConsumer implementation:
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnOtherError(EnrollmentLauncher::OtherError error) override;
  void OnDeviceEnrolled() override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;

  // Notification that the browser is being restarted.
  void OnBrowserRestart();

  // Used for testing.
  EnrollmentScreenView* GetView() { return view_.get(); }

  // NetworkStateInformer::NetworkStateInformerObserver
  void UpdateState(NetworkError::ErrorReason reason) override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  void set_tpm_ownership_callback_for_testing(TpmStatusCallback&& callback) {
    tpm_ownership_callback_for_testing_ = std::move(callback);
  }

  TpmStatusCallback get_tpm_ownership_callback_for_testing() {
    return base::BindOnce(&EnrollmentScreen::OnTpmStatusResponse,
                          weak_ptr_factory_.GetWeakPtr());
  }

  void set_tpm_updater_for_testing(
      base::RepeatingClosure tpm_updater_for_testing) {
    tpm_updater_ = std::move(tpm_updater_for_testing);
  }

  // Changes network state. Useful for simulating network issues in tests.
  void SetNetworkStateForTesting(const NetworkState* state);

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;
  void OnUserAction(const base::Value::List& args) override;

  // Expose the exit_callback to test screen overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  friend class EnrollmentScreenBaseTest;
  friend class test::EnrollmentHelperMixin;

  FRIEND_TEST_ALL_PREFIXES(AttestationAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ForcedAttestationAuthEnrollmentScreenTest,
                           TestCancel);
  FRIEND_TEST_ALL_PREFIXES(MultiAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenUnitTest,
                           ZeroTouchFlowShouldNotRetryOnTopOfUser);
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenUnitTest,
                           ZeroTouchFlowShouldNotRetryAfterSuccess);

  // The authentication mechanisms that this class can use.
  enum Auth {
    AUTH_ATTESTATION,
    AUTH_OAUTH,
    AUTH_ENROLLMENT_TOKEN,
  };

  // Updates view GAIA flow type which is used to modify visual appearance
  // of GAIA webview,
  void UpdateFlowType();

  // Sets the current config to use for enrollment.
  void SetConfig();

  // Called after account status is fetched.
  void OnAccountStatusFetched(const std::string& email,
                              bool fetch_succeeded,
                              policy::AccountStatus status);

  // Creates an enrollment launcher if needed.
  void CreateEnrollmentLauncher();

  // Clears auth in `enrollment_launcher_`. Deletes
  // `enrollment_launcher_` and runs `callback` on completion. See the
  // comment for EnrollmentLauncher::ClearAuth for details.
  void ClearAuth(base::OnceClosure callback);

  // Used as a callback for EnrollmentLauncher::ClearAuth.
  virtual void OnAuthCleared(base::OnceClosure callback);

  // Shows successful enrollment status after all enrollment related file
  // operations are completed.
  void ShowEnrollmentStatusOnSuccess();

  // Logs an UMA event in one of the "Enrollment.*" histograms, depending on
  // `enrollment_mode_`.
  void UMA(policy::MetricEnrollment sample);

  // Do attestation based enrollment.
  void AuthenticateUsingAttestation();

  void AuthenticateUsingEnrollmentToken();

  // Shows the interactive screen. Resets auth then shows the signin screen.
  void ShowInteractiveScreen();

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  // Shows the device attribute prompt screen.
  // Used as a callback to run after successful enrollment.
  void ShowAttributePromptScreen();

  // Record metrics when we encounter an enrollment error.
  void RecordEnrollmentErrorMetrics();

  // Advance to the next authentication mechanism if possible.
  bool AdvanceToNextAuth();

  // Similar to OnRetry(), but responds to a timer instead of the user
  // pressing the Retry button. Does not retry if `ShouldAutoRetryOnError()`
  // returns false.
  // TODO(b/314130124): Remove if retry logic is not needed.
  void MaybeAutomaticRetry();

  // Processes a request to retry enrollment.
  // Called by OnRetry() and AutomaticRetry().
  void ProcessRetry();

  // Tries to take TPM ownership.
  void TakeTpmOwnership();
  // Processes a reply from tpm_manager.
  void OnTpmStatusResponse(const ::tpm_manager::TakeOwnershipReply& reply);
  // Checks install attribute status to make sure that it is FIRST_INSTALL, in
  // this case we proceed with the enrollment. In other cases we either try to
  // wait for the FIRST_INSTALL status, or show a TpmErrorScreen with an ability
  // to reboot the device.
  void CheckInstallAttributesState();

  // Returns true if enrollment should be automatically retried on error.
  // TODO(b/314130124): Remove if retry logic is not needed.
  bool ShouldAutoRetryOnError() const;

  // Returns true if success screen should be skipped.
  bool AutoCloseEnrollmentConfirmationOnSuccess() const;

  // Returns true if current visible screen is the error screen over
  // enrollment sign-in page.
  bool IsEnrollmentScreenHiddenByError();

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void SetupAndShowOfflineMessage(NetworkStateInformer::State state,
                                  NetworkError::ErrorReason reason);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);

  // Stores the signin artifacts and the refresh token in the wizard context
  // if the appropriate conditions are met.
  void MaybeStoreUserContextInWizardContext();

  base::WeakPtr<EnrollmentScreenView> view_;
  raw_ptr<ErrorScreen> error_screen_ = nullptr;
  ScreenExitCallback exit_callback_;
  std::optional<TpmStatusCallback> tpm_ownership_callback_for_testing_;
  // Evaluates device policy TPMFirmwareUpdateSettings and updates the TPM if
  // the policy is set to "auto-update vulnerable TPM firmware at enrollment".
  base::RepeatingClosure tpm_updater_;
  policy::EnrollmentConfig prescribed_config_;
  policy::EnrollmentConfig effective_config_;
  ErrorScreensHistogramHelper histogram_helper_;

  // 'Current' and 'Next' authentication mechanisms to be used.
  // TODO(b/332529631): Consider moving these values and the corresponding enum
  // to EnrollmentConfig.
  Auth current_auth_ = AUTH_OAUTH;
  Auth next_auth_ = AUTH_OAUTH;

  bool enrollment_failed_once_ = false;
  bool enrollment_succeeded_ = false;

  // Check tpm before enrollment starts if --tpm-is-dynamic switch is enabled.
  bool tpm_checked_ = false;
  // Number of retries to get other than TPM_NOT_OWNED install attributes state.
  int install_state_retries_ = 0;
  // Timer for install attribute to resolve.
  base::OneShotTimer wait_state_timer_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Used to control observation of network errors on enrollment screen
  // depending on whenever signin screen is shown.
  base::ScopedObservation<NetworkStateInformer, NetworkStateInformerObserver>
      scoped_network_observation_{this};

  std::string enrolling_user_domain_;
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;
  net::BackoffEntry::Policy retry_policy_;
  std::unique_ptr<net::BackoffEntry> retry_backoff_;
  base::CancelableOnceClosure retry_task_;
  int num_retries_ = 0;
  std::unique_ptr<EnrollmentLauncher> enrollment_launcher_;
  std::unique_ptr<policy::AccountStatusCheckFetcher> status_checker_;

  std::unique_ptr<login::OnlineSigninArtifacts> signin_artifacts_;
  bool using_saml_api_ = false;

  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
