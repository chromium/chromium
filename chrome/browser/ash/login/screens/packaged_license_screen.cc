// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/packaged_license_screen.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"

namespace {

constexpr const char kUserActionEnrollButtonClicked[] = "enroll";
constexpr const char kUserActionDontEnrollButtonClicked[] = "dont-enroll";

}  // namespace

namespace chromeos {

// static
std::string PackagedLicenseScreen::GetResultString(Result result) {
  switch (result) {
    case Result::DONT_ENROLL:
      return "DontEnroll";
    case Result::ENROLL:
      return "Enroll";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

PackagedLicenseScreen::PackagedLicenseScreen(
    PackagedLicenseView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PackagedLicenseView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

PackagedLicenseScreen::~PackagedLicenseScreen() {
  if (view_)
    view_->Unbind();
}

bool PackagedLicenseScreen::MaybeSkip(WizardContext* context) {
  policy::EnrollmentConfig enrollment_config =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPrescribedEnrollmentConfig();
  // License screen should be shown when device packed with license and other
  // enrollment flows are not triggered by the device state.
  if (enrollment_config.is_license_packaged_with_device &&
      !enrollment_config.should_enroll()) {
    return false;
  }
  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void PackagedLicenseScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void PackagedLicenseScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void PackagedLicenseScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionEnrollButtonClicked)
    exit_callback_.Run(Result::ENROLL);
  else if (action_id == kUserActionDontEnrollButtonClicked)
    exit_callback_.Run(Result::DONT_ENROLL);
  else
    BaseScreen::OnUserAction(action_id);
}

bool PackagedLicenseScreen::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kStartEnrollment) {
    exit_callback_.Run(Result::ENROLL);
    return true;
  }
  return false;
}

}  // namespace chromeos
