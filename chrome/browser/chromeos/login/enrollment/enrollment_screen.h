// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/policy/active_directory_join_delegate.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "net/base/backoff_entry.h"

namespace base {
class ElapsedTimer;
}

namespace chromeos {

class ScreenManager;

namespace test {
class EnrollmentHelperMixin;
}

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public BaseScreen,
      public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public EnrollmentScreenView::Controller,
      public ActiveDirectoryJoinDelegate {
 public:
  enum class Result { COMPLETED, BACK };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  EnrollmentScreen(EnrollmentScreenView* view,
                   const ScreenExitCallback& exit_callback);
  ~EnrollmentScreen() override;

  static EnrollmentScreen* Get(ScreenManager* manager);

  // Setup how this screen will handle enrollment.
  void SetEnrollmentConfig(const policy::EnrollmentConfig& enrollment_config);

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

  // EnrollmentScreenView::Controller implementation:
  void OnLoginDone(const std::string& user,
                   const std::string& auth_code) override;
  void OnLicenseTypeSelected(const std::string& license_type) override;
  void OnRetry() override;
  void OnCancel() override;
  void OnConfirmationClosed() override;
  void OnActiveDirectoryCredsProvided(const std::string& machine_name,
                                      const std::string& distinguished_name,
                                      int encryption_types,
                                      const std::string& username,
                                      const std::string& password) override;
  void OnDeviceAttributeProvided(const std::string& asset_id,
                                 const std::string& location) override;

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer implementation:
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceEnrolled() override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnRestoreAfterRollbackCompleted() override;

  // ActiveDirectoryJoinDelegate implementation:
  void JoinDomain(const std::string& dm_token,
                  const std::string& domain_join_config,
                  OnDomainJoinedCallback on_joined_callback) override;

  // Used for testing.
  EnrollmentScreenView* GetView() { return view_; }

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 protected:
  // Expose the exit_callback to test screen overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  friend class MultiLicenseEnrollmentScreenUnitTest;
  friend class ZeroTouchEnrollmentScreenUnitTest;
  friend class AutomaticReenrollmentScreenUnitTest;
  friend class test::EnrollmentHelperMixin;

  FRIEND_TEST_ALL_PREFIXES(AttestationAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ForcedAttestationAuthEnrollmentScreenTest,
                           TestCancel);
  FRIEND_TEST_ALL_PREFIXES(MultiAuthEnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest, Retry);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest,
                           DoNotRetryOnTopOfUser);
  FRIEND_TEST_ALL_PREFIXES(ZeroTouchEnrollmentScreenUnitTest,
                           DoNotRetryAfterSuccess);

  // The authentication mechanisms that this class can use.
  enum Auth {
    AUTH_ATTESTATION,
    AUTH_OAUTH,
  };

  // Sets the current config to use for enrollment.
  void SetConfig();

  // Creates an enrollment helper if needed.
  void CreateEnrollmentHelper();

  // Clears auth in |enrollment_helper_|. Deletes |enrollment_helper_| and runs
  // |callback| on completion. See the comment for
  // EnterpriseEnrollmentHelper::ClearAuth for details.
  void ClearAuth(const base::Closure& callback);

  // Used as a callback for EnterpriseEnrollmentHelper::ClearAuth.
  virtual void OnAuthCleared(const base::Closure& callback);

  // Shows successful enrollment status after all enrollment related file
  // operations are completed.
  void ShowEnrollmentStatusOnSuccess();

  // Logs an UMA event in one of the "Enrollment.*" histograms, depending on
  // |enrollment_mode_|.
  void UMA(policy::MetricEnrollment sample);

  // Do attestation based enrollment.
  void AuthenticateUsingAttestation();

  // Starts flow that would handle necessary steps to restore after version
  // rollback.
  void RestoreAfterRollback();

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
  // pressing the Retry button.
  void AutomaticRetry();

  // Processes a request to retry enrollment.
  // Called by OnRetry() and AutomaticRetry().
  void ProcessRetry();

  // Callback for Active Directory domain join.
  void OnActiveDirectoryJoined(const std::string& machine_name,
                               const std::string& username,
                               authpolicy::ErrorType error,
                               const std::string& machine_domain);

  EnrollmentScreenView* view_;
  ScreenExitCallback exit_callback_;
  policy::EnrollmentConfig config_;
  policy::EnrollmentConfig enrollment_config_;
  Auth current_auth_ = AUTH_OAUTH;
  Auth last_auth_ = AUTH_OAUTH;
  bool enrollment_failed_once_ = false;
  bool enrollment_succeeded_ = false;
  std::string enrolling_user_domain_;
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;
  net::BackoffEntry::Policy retry_policy_;
  std::unique_ptr<net::BackoffEntry> retry_backoff_;
  base::CancelableClosure retry_task_;
  int num_retries_ = 0;
  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;
  OnDomainJoinedCallback on_joined_callback_;

  // Helper to call AuthPolicyClient and cancel calls if needed. Used to join
  // Active Directory domain.
  std::unique_ptr<AuthPolicyHelper> authpolicy_login_helper_;

  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
