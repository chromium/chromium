// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/packaged_license_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_oobe.mojom.h"
#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"

namespace ash {

// static
std::string PackagedLicenseScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::DONT_ENROLL:
      return "DontEnroll";
    case Result::ENROLL:
      return "Enroll";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
    case Result::NOT_APPLICABLE_SKIP_TO_ENROLL:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

PackagedLicenseScreen::PackagedLicenseScreen(
    base::WeakPtr<PackagedLicenseView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PackagedLicenseView::kScreenId, OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

PackagedLicenseScreen::~PackagedLicenseScreen() = default;

bool PackagedLicenseScreen::MaybeSkip(WizardContext& context) {
  policy::EnrollmentConfig config =
      policy::EnrollmentConfig::GetPrescribedEnrollmentConfig();
  // License screen should be shown when device packed with license and other
  // enrollment flows are not triggered by the device state.
  if (config.is_license_packaged_with_device && !config.should_enroll()) {
    // Skip to enroll since GAIA form has welcoming text for enterprise license.
    if (config.license_type == policy::LicenseType::kEnterprise) {
      exit_callback_.Run(Result::NOT_APPLICABLE_SKIP_TO_ENROLL);
      return true;
    }
    // Skip to enroll since GAIA form has welcoming text for education license.
    if (config.license_type == policy::LicenseType::kEducation) {
      exit_callback_.Run(Result::NOT_APPLICABLE_SKIP_TO_ENROLL);
      return true;
    }
    return false;
  }
  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void PackagedLicenseScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void PackagedLicenseScreen::HideImpl() {}

void PackagedLicenseScreen::OnEnrollClicked() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run(Result::ENROLL);
}

void PackagedLicenseScreen::OnDontEnrollClicked() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run(Result::DONT_ENROLL);
}

bool PackagedLicenseScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENROLL);
    return true;
  }
  return false;
}

}  // namespace ash
