// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_

#include <string>

#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {
class EnrollmentStatus;
}

namespace ash {
namespace test {

// This test mixin covers mocking backend interaction during enterprise
// enrollment on EnrollmentLauncher level.
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

  // Configures and sets expectations for token-based enrollment to succeed.
  void ExpectTokenBasedEnrollmentSuccess();
  // Configures and sets expecations for token-based enrollment resulting in
  // error.
  void ExpectTokenBasedEnrollmentError(policy::EnrollmentStatus status);

  // Sets expectation of enrollment token and proper token-based enrollment
  // mode.
  void ExpectEnrollmentTokenConfig(const std::string& enrollment_token);

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

 private:
  ::testing::NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;
  ScopedEnrollmentLauncherFactoryOverrideForTesting scoped_factory_override_;
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_ENROLLMENT_HELPER_MIXIN_H_
