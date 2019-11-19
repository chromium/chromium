// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace test {

namespace ui {

const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepWorking[] = "working";
const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepLicenses[] = "license";
const char kEnrollmentStepDeviceAttributes[] = "attribute-prompt";
const char kEnrollmentStepADJoin[] = "ad-join";
const char kEnrollmentStepError[] = "error";
const char kEnrollmentStepDeviceAttributesError[] = "attribute-prompt-error";
const char kEnrollmentStepADJoinError[] = "active-directory-join-error";

}  // namespace ui

namespace values {

const char kLicenseTypePerpetual[] = "perpetual";
const char kLicenseTypeAnnual[] = "annual";
const char kLicenseTypeKiosk[] = "kiosk";

const char kAssetId[] = "asset_id";
const char kLocation[] = "location";

}  // namespace values

namespace {

const char kEnrollmentUI[] = "enterprise-enrollment";

const char* const kAllSteps[] = {
    ui::kEnrollmentStepSignin,   ui::kEnrollmentStepWorking,
    ui::kEnrollmentStepLicenses, ui::kEnrollmentStepDeviceAttributes,
    ui::kEnrollmentStepSuccess,  ui::kEnrollmentStepADJoin,
    ui::kEnrollmentStepError};

std::string StepElementID(const std::string& step) {
  return "step-" + step;
}

const std::initializer_list<base::StringPiece> kEnrollmentErrorButtonPath = {
    kEnrollmentUI, "oauth-enroll-error-card", "submitButton"};

const std::initializer_list<base::StringPiece> kEnrollmentSuccessButtonPath = {
    kEnrollmentUI, "success-done-button"};

}  // namespace

EnrollmentUIMixin::EnrollmentUIMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

EnrollmentUIMixin::~EnrollmentUIMixin() = default;

// Waits until specific enrollment step is displayed.
void EnrollmentUIMixin::WaitForStep(const std::string& step) {
  OobeJS()
      .CreateVisibilityWaiter(true, {kEnrollmentUI, StepElementID(step)})
      ->Wait();
  for (const char* other : kAllSteps) {
    if (other != step)
      OobeJS().ExpectHiddenPath({kEnrollmentUI, StepElementID(other)});
  }
}

// Returns true if there are any DOM elements with the given class.
void EnrollmentUIMixin::ExpectStepVisibility(bool visibility,
                                             const std::string& step) {
  if (visibility) {
    OobeJS().ExpectVisiblePath({kEnrollmentUI, StepElementID(step)});
  } else {
    OobeJS().ExpectHiddenPath({kEnrollmentUI, StepElementID(step)});
  }
}

void EnrollmentUIMixin::SelectEnrollmentLicense(
    const std::string& license_type) {
  OobeJS().SelectRadioPath({kEnrollmentUI, "oauth-enroll-license-ui",
                            "license-option-" + license_type});
}

void EnrollmentUIMixin::UseSelectedLicense() {
  OobeJS().TapOnPath({kEnrollmentUI, "oauth-enroll-license-ui", "next"});
}

void EnrollmentUIMixin::ExpectErrorMessage(int error_message_id,
                                           bool can_retry) {
  const std::string element_path =
      GetOobeElementPath({kEnrollmentUI, "oauth-enroll-error-card"});
  const std::string message = OobeJS().GetString(element_path + ".textContent");
  ASSERT_TRUE(std::string::npos !=
              message.find(l10n_util::GetStringUTF8(error_message_id)));
  if (can_retry) {
    OobeJS().ExpectVisiblePath(kEnrollmentErrorButtonPath);
  } else {
    OobeJS().ExpectHiddenPath(kEnrollmentErrorButtonPath);
  }
}

void EnrollmentUIMixin::RetryAfterError() {
  OobeJS().ClickOnPath(kEnrollmentErrorButtonPath);
  WaitForStep(ui::kEnrollmentStepSignin);
}

void EnrollmentUIMixin::LeaveDeviceAttributeErrorScreen() {
  OobeJS().ClickOnPath(kEnrollmentErrorButtonPath);
}

void EnrollmentUIMixin::LeaveSuccessScreen() {
  OobeJS().ClickOnPath(kEnrollmentSuccessButtonPath);
}

void EnrollmentUIMixin::SubmitDeviceAttributes(const std::string& asset_id,
                                               const std::string& location) {
  OobeJS().TypeIntoPath(asset_id, {kEnrollmentUI, "oauth-enroll-asset-id"});
  OobeJS().TypeIntoPath(location, {kEnrollmentUI, "oauth-enroll-location"});
  OobeJS().TapOnPath({kEnrollmentUI, "attributes-submit"});
}

void EnrollmentUIMixin::SetExitHandler() {
  ASSERT_NE(WizardController::default_controller(), nullptr);
  EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
      WizardController::default_controller()->screen_manager());
  ASSERT_NE(enrollment_screen, nullptr);
  enrollment_screen->set_exit_callback_for_testing(base::BindRepeating(
      &EnrollmentUIMixin::HandleScreenExit, base::Unretained(this)));
}

EnrollmentScreen::Result EnrollmentUIMixin::WaitForScreenExit() {
  if (screen_result_.has_value())
    return screen_result_.value();

  DCHECK(!screen_exit_waiter_.has_value());
  screen_exit_waiter_.emplace();
  screen_exit_waiter_->Run();
  DCHECK(screen_result_.has_value());
  EnrollmentScreen::Result result = screen_result_.value();
  screen_result_.reset();
  screen_exit_waiter_.reset();
  return result;
}

void EnrollmentUIMixin::HandleScreenExit(EnrollmentScreen::Result result) {
  EXPECT_FALSE(screen_result_.has_value());
  screen_result_ = result;
  if (screen_exit_waiter_)
    screen_exit_waiter_->Quit();
}

}  // namespace test
}  // namespace chromeos
