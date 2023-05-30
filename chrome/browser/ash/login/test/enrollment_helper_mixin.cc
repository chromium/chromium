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
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"

namespace ash {
namespace test {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;

MATCHER_P(ConfigModeMatches, mode, "") {
  return arg.mode == mode;
}

}  // namespace

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
  EnrollmentLauncher::SetEnrollmentHelperMock(nullptr);
  // Enrollment screen might have reference to enrollment_launcher_.
  if (WizardController::default_controller()) {
    auto* screen_manager =
        WizardController::default_controller()->screen_manager();
    if (screen_manager->HasScreen(EnrollmentScreenView::kScreenId)) {
      EnrollmentScreen::Get(screen_manager)->enrollment_launcher_.reset();
    }
  }
}

void EnrollmentHelperMixin::ResetMock() {
  std::unique_ptr<MockEnrollmentLauncher> mock =
      std::make_unique<MockEnrollmentLauncher>();
  mock_ = mock.get();
  EnrollmentLauncher::SetEnrollmentHelperMock(std::move(mock));
}

void EnrollmentHelperMixin::ExpectNoEnrollment() {
  EXPECT_CALL(*mock_, Setup(_, _, _)).Times(0);
}

void EnrollmentHelperMixin::ExpectEnrollmentMode(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(*mock_, Setup(ConfigModeMatches(mode), _, _));
}

void EnrollmentHelperMixin::ExpectEnrollmentModeRepeated(
    policy::EnrollmentConfig::Mode mode) {
  EXPECT_CALL(*mock_, Setup(ConfigModeMatches(mode), _, _)).Times(AtLeast(1));
}

void EnrollmentHelperMixin::ExpectSuccessfulOAuthEnrollment() {
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs(
          [this]() { mock_->status_consumer()->OnDeviceEnrolled(); }));
}

void EnrollmentHelperMixin::ExpectOAuthEnrollmentError(
    policy::EnrollmentStatus status) {
  EXPECT_CALL(*mock_, EnrollUsingAuthCode(kTestAuthCode))
      .WillOnce(InvokeWithoutArgs([this, status]() {
        mock_->status_consumer()->OnEnrollmentError(status);
      }));
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

}  // namespace test
}  // namespace ash
