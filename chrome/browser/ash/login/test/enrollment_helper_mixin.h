// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {
class ActiveDirectoryJoinDelegate;
class EnrollmentStatus;
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

  // Re-creates mock. Useful in tests that retry enrollment with different auth
  // mechanism, which causes original mock to be destroyed by EnrollmentScreen.
  void ResetMock();

  // Verifies mock expectations and clears them. Useful in tests that retry
  // enrollment with the same auth mechanism.
  void VerifyAndClear();

  // Sets up expectation of no enrollment attempt.
  void ExpectNoEnrollment();

  // Sets up expectation of enrollment mode.
  void ExpectEnrollmentMode(policy::EnrollmentConfig::Mode mode);
  void ExpectEnrollmentModeRepeated(policy::EnrollmentConfig::Mode mode);

  // Configures and sets expectations for successful auth-token based flow
  // without license selection.
  void ExpectSuccessfulOAuthEnrollment();
  // Configures and sets expectations for an error during auth-token based
  // flow.
  void ExpectOAuthEnrollmentError(policy::EnrollmentStatus status);

  // Configures and sets expectations for successful attestation-based flow.
  void ExpectAttestationEnrollmentSuccess();
  // Configures and sets expectations for attestation-based flow resulting in
  // error.
  void ExpectAttestationEnrollmentError(policy::EnrollmentStatus status);
  void ExpectAttestationEnrollmentErrorRepeated(
      policy::EnrollmentStatus status);

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
  raw_ptr<EnterpriseEnrollmentHelperMock, ExperimentalAsh> mock_ = nullptr;
  base::WeakPtrFactory<EnrollmentHelperMixin> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
