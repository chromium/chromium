// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/active_directory_join_delegate.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace {

MATCHER_P(ConfigModeMatches, mode, "") {
  return arg.mode == mode;
}

}  // namespace

namespace chromeos {
namespace test {

// static
const char EnrollmentHelperMixin::kTestAuthCode[] = "test_auth_code";

EnrollmentHelperMixin::EnrollmentHelperMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

EnrollmentHelperMixin::~EnrollmentHelperMixin() = default;

void EnrollmentHelperMixin::SetUpInProcessBrowserTestFixture() {
  ResetMock();
}

void EnrollmentHelperMixin::TearDownInProcessBrowserTestFixture() {
  mock_ = nullptr;
  EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(nullptr);
  // Enrollment screen might have reference to enrollment_helper_.
  if (WizardController::default_controller()) {
    auto* screen_manager =
        WizardController::default_controller()->screen_manager();
    if (screen_manager->HasScreen(EnrollmentScreenView::kScreenId)) {
      EnrollmentScreen::Get(screen_manager)->enrollment_helper_.reset();
    }
  }
}

void EnrollmentHelperMixin::ResetMock() {
  std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
      std::make_unique<EnterpriseEnrollmentHelperMock>();
  mock_ = mock.get();
  EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
}

void EnrollmentHelperMixin::ExpectNoEnrollment() {
  EXPECT_CALL(*mock_, Setup(_, _, _)).Times(0);
}

void EnrollmentHelperMixin::ExpectEnrollmentMode(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(*mock_, Setup(_, ConfigModeMatches(mode), _));
}

void EnrollmentHelperMixin::ExpectEnrollmentModeRepeated(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(*mock_, Setup(_, ConfigModeMatches(mode), _)).Times(AtLeast(1));
}

void EnrollmentHelperMixin::ExpectSuccessfulOAuthEnrollment() {
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

void EnrollmentHelperMixin::ExpectAvailableLicenseCount(int perpetual,
                                                        int annual,
                                                        int kiosk) {
  std::map<policy::LicenseType, int> license_map;
  if (perpetual >= 0)
    license_map[policy::LicenseType::PERPETUAL] = perpetual;
  if (annual >= 0)
    license_map[policy::LicenseType::ANNUAL] = annual;
  if (kiosk >= 0)
    license_map[policy::LicenseType::KIOSK] = kiosk;
  CHECK(license_map.size() > 1);
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs([this, license_map]() {
        mock_->status_consumer()->OnMultipleLicensesAvailable(license_map);
      }));
}

void EnrollmentHelperMixin::ExpectSuccessfulEnrollmentWithLicense(
    policy::LicenseType license_type) {
  EXPECT_CALL(*mock_, UseLicenseType(license_type))
      .WillOnce(InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentSuccess() {
  EXPECT_CALL(*mock_, EnrollUsingAttestation())
      .WillOnce(InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentError(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(*mock_, EnrollUsingAttestation())
      .WillOnce(InvokeWithoutArgs([this, status]() {
        mock_->status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::ExpectAttestationEnrollmentErrorRepeated(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(*mock_, EnrollUsingAttestation())
      .Times(AtLeast(1))
      .WillRepeatedly(InvokeWithoutArgs([this, status]() {
        mock_->status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::ExpectOfflineEnrollmentSuccess() {
  ExpectEnrollmentMode(policy::EnrollmentConfig::MODE_OFFLINE_DEMO);

  EXPECT_CALL(*mock_, EnrollForOfflineDemo())
      .WillOnce(testing::InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

void EnrollmentHelperMixin::ExpectOfflineEnrollmentError(
    policy::EnrollmentStatus status) {
  ExpectEnrollmentMode(policy::EnrollmentConfig::MODE_OFFLINE_DEMO);
  EXPECT_CALL(*mock_, EnrollForOfflineDemo())
      .WillOnce(testing::InvokeWithoutArgs([this, status]() {
        mock_->status_consumer()->OnEnrollmentError(status);
      }));
}

void EnrollmentHelperMixin::SetupClearAuth() {
  ON_CALL(*mock_, ClearAuth(_))
      .WillByDefault(Invoke(
          [](base::OnceClosure callback) { std::move(callback).Run(); }));
}

void EnrollmentHelperMixin::ExpectEnrollmentCredentials() {
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode));
}

void EnrollmentHelperMixin::DisableAttributePromptUpdate() {
  EXPECT_CALL(*mock_, GetDeviceAttributeUpdatePermission())
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_->status_consumer()->OnDeviceAttributeUpdatePermission(false);
      }));
}

void EnrollmentHelperMixin::ExpectAttributePromptUpdate(
    const std::string& asset_id,
    const std::string& location) {
  // Causes the attribute-prompt flow to activate.
  ON_CALL(*mock_, GetDeviceAttributeUpdatePermission())
      .WillByDefault(InvokeWithoutArgs([this]() {
        mock_->status_consumer()->OnDeviceAttributeUpdatePermission(true);
      }));

  // Ensures we receive the updates attributes.
  EXPECT_CALL(*mock_, UpdateDeviceAttributes(asset_id, location))
      .WillOnce(InvokeWithoutArgs([this]() {
        mock_->status_consumer()->OnDeviceAttributeUploadCompleted(true);
      }));
}

void EnrollmentHelperMixin::SetupActiveDirectoryJoin(
    ActiveDirectoryJoinDelegate* delegate,
    const std::string& expected_domain,
    const std::string& domain_join_config,
    const std::string& dm_token) {
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs(
          [delegate, expected_domain, domain_join_config, dm_token]() {
            delegate->JoinDomain(dm_token, domain_join_config,
                                 base::BindOnce(
                                     [](const std::string& expected_domain,
                                        const std::string& domain) {
                                       ASSERT_EQ(expected_domain, domain);
                                     },
                                     expected_domain));
          }));
}

void EnrollmentHelperMixin::ExpectTokenEnrollmentSuccess(
    const std::string& token) {
  ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ENROLLMENT_TOKEN);
  EXPECT_CALL(*mock_, EnrollUsingEnrollmentToken(token))
      .WillOnce(InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

}  // namespace test
}  // namespace chromeos
