// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace test {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

MATCHER_P(ConfigModeMatches, mode, "") {
  return arg.mode == mode;
}

MATCHER_P(ConfigModeIsTokenEnrollmentAndTokenMatches, enrollment_token, "") {
  return arg.enrollment_token == enrollment_token &&
         arg.mode == policy::EnrollmentConfig::
                         MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
}

}  // namespace

// static
const char EnrollmentHelperMixin::kTestAuthCode[] = "test_auth_code";

EnrollmentHelperMixin::EnrollmentHelperMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host),
      scoped_factory_override_(
          base::BindRepeating(FakeEnrollmentLauncher::Create,
                              &mock_enrollment_launcher_)) {}

EnrollmentHelperMixin::~EnrollmentHelperMixin() = default;

void EnrollmentHelperMixin::ResetMock() {
  Mock::VerifyAndClearExpectations(&mock_enrollment_launcher_);
}

void EnrollmentHelperMixin::ExpectNoEnrollment() {
  EXPECT_CALL(mock_enrollment_launcher_, Setup(_, _)).Times(0);
}

void EnrollmentHelperMixin::ExpectEnrollmentMode(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(mock_enrollment_launcher_, Setup(ConfigModeMatches(mode), _));
}

void EnrollmentHelperMixin::ExpectEnrollmentTokenConfig(
    const std::string& enrollment_token) {
  EXPECT_CALL(
      mock_enrollment_launcher_,
      Setup(ConfigModeIsTokenEnrollmentAndTokenMatches(enrollment_token), _));
}

void EnrollmentHelperMixin::ExpectEnrollmentModeRepeated(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(mock_enrollment_launcher_, Setup(ConfigModeMatches(mode), _))
      .Times(AtLeast(1));
}

void EnrollmentHelperMixin::ExpectSuccessfulOAuthEnrollment() {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()->OnDeviceEnrolled();
      }));
}

void EnrollmentHelperMixin::ExpectOAuthEnrollmentError(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs([this, status]() {
        mock_enrollment_launcher_.status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentSuccess() {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()->OnDeviceEnrolled();
      }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentError(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
      .WillOnce(InvokeWithoutArgs([this, status]() {
        mock_enrollment_launcher_.status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentErrorRepeated(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs([this, status]() {
        mock_enrollment_launcher_.status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::ExpectTokenBasedEnrollmentSuccess() {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingEnrollmentToken())
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()->OnDeviceEnrolled();
      }));
}

void EnrollmentHelperMixin::ExpectTokenBasedEnrollmentError(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingEnrollmentToken())
      .WillOnce(InvokeWithoutArgs([this, status]() {
        mock_enrollment_launcher_.status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::SetupClearAuth() {
  ON_CALL(mock_enrollment_launcher_, ClearAuth(_, _))
      .WillByDefault(
          Invoke([](base::OnceClosure callback, bool revoke_oauth2_tokens) {
            std::move(callback).Run();
          }));
}

void EnrollmentHelperMixin::ExpectEnrollmentCredentials() {
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode));
}

void EnrollmentHelperMixin::DisableAttributePromptUpdate() {
  EXPECT_CALL(mock_enrollment_launcher_, GetDeviceAttributeUpdatePermission())
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()
            ->OnDeviceAttributeUpdatePermission(false);
      }));
}

void EnrollmentHelperMixin::ExpectAttributePromptUpdate(
    const std::string& asset_id,
    const std::string& location) {
  // Causes the attribute-prompt flow to activate.
  ON_CALL(mock_enrollment_launcher_, GetDeviceAttributeUpdatePermission())
      .WillByDefault(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()
            ->OnDeviceAttributeUpdatePermission(true);
      }));

  // Ensures we receive the updates attributes.
  EXPECT_CALL(mock_enrollment_launcher_,
              UpdateDeviceAttributes(asset_id, location))
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_enrollment_launcher_.status_consumer()
            ->OnDeviceAttributeUploadCompleted(true);
      }));
}

}  // namespace test
}  // namespace ash
