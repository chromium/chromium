// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {
class EnrollmentHandler;
class PolicyOAuth2TokenFetcher;
}  // namespace policy

namespace ash {

class EnterpriseEnrollmentHelperImpl : public EnterpriseEnrollmentHelper {
 public:
  EnterpriseEnrollmentHelperImpl();

  EnterpriseEnrollmentHelperImpl(const EnterpriseEnrollmentHelperImpl&) =
      delete;
  EnterpriseEnrollmentHelperImpl& operator=(
      const EnterpriseEnrollmentHelperImpl&) = delete;

  ~EnterpriseEnrollmentHelperImpl() override;

  // EnterpriseEnrollmentHelper:
  void EnrollUsingAuthCode(const std::string& auth_code) override;
  void EnrollUsingToken(const std::string& token) override;
  void EnrollUsingAttestation() override;
  void ClearAuth(base::OnceClosure callback) override;
  void GetDeviceAttributeUpdatePermission() override;
  void UpdateDeviceAttributes(const std::string& asset_id,
                              const std::string& location) override;
  void Setup(const policy::EnrollmentConfig& enrollment_config,
             const std::string& enrolling_user_domain,
             policy::LicenseType license_type) override;
  bool InProgress() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestProperPageGetsLoadedOnEnrollmentSuccess);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseEnrollmentTest,
                           TestAttributePromptPageGetsLoaded);

  // Attempt enrollment using `auth_data` for authentication.
  void DoEnroll(policy::DMAuth auth_data);

  // Handles completion of the OAuth2 token fetch attempt.
  void OnTokenFetched(const std::string& token,
                      const GoogleServiceAuthError& error);

  // Handles completion of the enrollment attempt.
  void OnEnrollmentFinished(policy::EnrollmentStatus status);

  // Handles completion of the device attribute update permission request.
  void OnDeviceAttributeUpdatePermission(bool granted);

  // Handles completion of the device attribute update attempt.
  void OnDeviceAttributeUploadCompleted(bool success);

  void ReportAuthStatus(const GoogleServiceAuthError& error);
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on `enrollment_mode_`.
  void UMA(policy::MetricEnrollment sample);

  // Called by ProfileHelper when a signin profile clearance has finished.
  // `callback` is a callback, that was passed to ClearAuth() before.
  void OnSigninProfileCleared(base::OnceClosure callback);

  // Returns either OAuth token or DM token needed for the device attribute
  // update permission request.
  absl::optional<policy::DMAuth> GetDMAuthForDeviceAttributeUpdate(
      policy::CloudPolicyClient* device_cloud_policy_client);

  policy::EnrollmentConfig enrollment_config_;
  std::string enrolling_user_domain_;
  policy::LicenseType license_type_;

  enum {
    OAUTH_NOT_STARTED,
    OAUTH_STARTED_WITH_AUTH_CODE,
    OAUTH_STARTED_WITH_TOKEN,
    OAUTH_FINISHED
  } oauth_status_ = OAUTH_NOT_STARTED;
  bool oauth_data_cleared_ = false;
  policy::DMAuth auth_data_;
  bool success_ = false;

  std::unique_ptr<policy::PolicyOAuth2TokenFetcher> oauth_fetcher_;

  // Non-nullptr from DoEnroll till OnEnrollmentFinished.
  std::unique_ptr<policy::EnrollmentHandler> enrollment_handler_;

  base::WeakPtrFactory<EnterpriseEnrollmentHelperImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_IMPL_H_
