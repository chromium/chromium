// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {
class ActiveDirectoryJoinDelegate;
}

namespace ash {
class EnterpriseEnrollmentHelperMock;

namespace test {

// This test mixin covers mocking backend interaction during enterprise
// enrollment on EnterpriseEnrollmentHelper level.
class EnrollmentHelperMixin : public InProcessBrowserTestMixin {
 public:
  static const char kTestAuthCode[];

  explicit EnrollmentHelperMixin(InProcessBrowserTestMixinHost* host);

  EnrollmentHelperMixin(const EnrollmentHelperMixin&) = delete;
  EnrollmentHelperMixin& operator=(const EnrollmentHelperMixin&) = delete;

  ~EnrollmentHelperMixin() override;

  // Resets mock (to be used in tests that retry enrollment.
  void ResetMock();

  // Sets up expectation of no enrollment attempt.
  void ExpectNoEnrollment();

  // Sets up expectation of enrollment mode.
  void ExpectEnrollmentMode(policy::EnrollmentConfig::Mode mode);
  void ExpectEnrollmentModeRepeated(policy::EnrollmentConfig::Mode mode);

  // Configures and sets expectations for successful auth-token based flow
  // without license selection.
  void ExpectSuccessfulOAuthEnrollment();

  // Configures and sets expectations for successful attestation-based flow.
  void ExpectAttestationEnrollmentSuccess();
  // Configures and sets expectations for attestation-based flow resulting in
  // error.
  void ExpectAttestationEnrollmentError(policy::EnrollmentStatus status);
  void ExpectAttestationEnrollmentErrorRepeated(
      policy::EnrollmentStatus status);

  // Configures and sets expectations for successful offline demo flow.
  void ExpectOfflineEnrollmentSuccess();
  // Configures and sets expectations for offline demo flow resulting in error.
  void ExpectOfflineEnrollmentError(policy::EnrollmentStatus status);

  // Sets up expectation of kTestAuthCode as enrollment credentials.
  void ExpectEnrollmentCredentials();
  // Sets up default ClearAuth handling.
  void SetupClearAuth();

  // Configures not to show an attribute prompt.
  void DisableAttributePromptUpdate();
  // Attribute prompt should be displayed during enrollment, and
  // `asset_id` / `location` should be sent back to server.
  void ExpectAttributePromptUpdate(const std::string& asset_id,
                                   const std::string& location);

  // Forces the Active Directory domain join flow during enterprise enrollment.
  void SetupActiveDirectoryJoin(policy::ActiveDirectoryJoinDelegate* delegate,
                                const std::string& expected_domain,
                                const std::string& domain_join_config,
                                const std::string& dm_token);

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

 private:
  // Unowned reference to last created mock.
  EnterpriseEnrollmentHelperMock* mock_ = nullptr;
  base::WeakPtrFactory<EnrollmentHelperMixin> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace test {
using ::ash::test::EnrollmentHelperMixin;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
