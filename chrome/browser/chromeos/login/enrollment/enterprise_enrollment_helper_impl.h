// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/policy/device_account_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {
class DMAuth;
class PolicyOAuth2TokenFetcher;
}

namespace chromeos {

class EnterpriseEnrollmentHelperImpl
    : public EnterpriseEnrollmentHelper,
      public policy::DeviceAccountInitializer::Delegate,
      public policy::DeviceCloudPolicyManagerChromeOS::Observer {
 public:
  EnterpriseEnrollmentHelperImpl();
  ~EnterpriseEnrollmentHelperImpl() override;

  // EnterpriseEnrollmentHelper:
  void EnrollUsingAuthCode(const std::string& auth_code) override;
  void EnrollUsingToken(const std::string& token) override;
  void EnrollUsingEnrollmentToken(const std::string& token) override;
  void EnrollUsingAttestation() override;
  void EnrollForOfflineDemo() override;
  void RestoreAfterRollback() override;
  void ClearAuth(base::OnceClosure callback) override;
  void UseLicenseType(policy::LicenseType type) override;
  void GetDeviceAttributeUpdatePermission() override;
  void UpdateDeviceAttributes(const std::string& asset_id,
                              const std::string& location) override;
  void Setup(ActiveDirectoryJoinDelegate* ad_join_delegate,
             const policy::EnrollmentConfig& enrollment_config,
             const std::string& enrolling_user_domain) override;

  // DeviceCloudPolicyManagerChromeOS::Observer:
  void OnDeviceCloudPolicyManagerConnected() override;
  void OnDeviceCloudPolicyManagerDisconnected() override;

  // policy::DeviceAccountInitializer::Delegate:
  void OnDeviceAccountTokenFetched(bool empty_token) override;
  void OnDeviceAccountTokenStored() override;
  void OnDeviceAccountTokenError(policy::EnrollmentStatus status) override;
  void OnDeviceAccountClientError(
      policy::DeviceManagementStatus status) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestProperPageGetsLoadedOnEnrollmentSuccess);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestAttributePromptPageGetsLoaded);

  // Checks if license type selection should be performed during enrollment.
  bool ShouldCheckLicenseType() const;

  // Attempt enrollment using |auth_data| for authentication.
  void DoEnroll(std::unique_ptr<policy::DMAuth> auth_data);

  // Handles completion of the OAuth2 token fetch attempt.
  void OnTokenFetched(const std::string& token,
                      const GoogleServiceAuthError& error);

  // Handles multiple license types case.
  void OnLicenseMapObtained(const EnrollmentLicenseMap& licenses);

  // Handles completion of the enrollment attempt.
  void OnEnrollmentFinished(policy::EnrollmentStatus status);

  // Handles completion of the device attribute update permission request.
  void OnDeviceAttributeUpdatePermission(bool granted);

  // Handles completion of the device attribute update attempt.
  void OnDeviceAttributeUploadCompleted(bool success);

  void ReportAuthStatus(const GoogleServiceAuthError& error);
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on |enrollment_mode_|.
  void UMA(policy::MetricEnrollment sample);

  // Called by ProfileHelper when a signin profile clearance has finished.
  // |callback| is a callback, that was passed to ClearAuth() before.
  void OnSigninProfileCleared(base::OnceClosure callback);

  // Called when CloudPolicyClient exists, so device account can be initialized.
  void RestoreAfterRollbackInitialized();

  policy::EnrollmentConfig enrollment_config_;
  std::string enrolling_user_domain_;

  enum {
    OAUTH_NOT_STARTED,
    OAUTH_STARTED_WITH_AUTH_CODE,
    OAUTH_STARTED_WITH_TOKEN,
    OAUTH_FINISHED
  } oauth_status_ = OAUTH_NOT_STARTED;
  bool oauth_data_cleared_ = false;
  std::unique_ptr<policy::DMAuth> auth_data_;
  bool success_ = false;
  ActiveDirectoryJoinDelegate* ad_join_delegate_ = nullptr;

  std::unique_ptr<policy::PolicyOAuth2TokenFetcher> oauth_fetcher_;
  std::unique_ptr<policy::DeviceAccountInitializer> device_account_initializer_;

  base::WeakPtrFactory<EnterpriseEnrollmentHelperImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentHelperImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
