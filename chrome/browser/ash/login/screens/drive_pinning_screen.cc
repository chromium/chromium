// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/drive_pinning_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {
namespace {

constexpr const char kUserActionDecline[] = "driveDecline";
constexpr const char kUserActionAccept[] = "driveAccept";

using drivefs::pinning::Progress;

drivefs::pinning::PinManager* GetPinManager() {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  if (!service || !service->IsMounted() || !service->GetPinManager()) {
    return nullptr;
  }

  return service->GetPinManager();
}

}  // namespace

// static
std::string DrivePinningScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPT:
      return "Accept";
    case Result::DECLINE:
      return "Decline";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

DrivePinningScreen::DrivePinningScreen(
    base::WeakPtr<DrivePinningScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DrivePinningScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DrivePinningScreen::~DrivePinningScreen() = default;

bool DrivePinningScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  if (features::IsOobeChoobeEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller) {
      return choobe_controller->ShouldScreenBeSkipped(
          DrivePinningScreenView::kScreenId);
    }
  }

  return !drive_pinning_available_;
}

bool DrivePinningScreen::MaybeSkip(WizardContext& context) {
  if (ShouldBeSkipped(context)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void DrivePinningScreen::CalculateRequiredSpace() {
  auto* pin_manager = GetPinManager();

  if (pin_manager) {
    pin_manager->AddObserver(this);
    pin_manager->CalculateRequiredSpace();
  }
}

void DrivePinningScreen::OnProgressForTest(
    const drivefs::pinning::Progress& progress) {
  CHECK_IS_TEST();
  OnProgress(progress);
}

void DrivePinningScreen::OnProgress(const Progress& progress) {
  if (progress.stage == drivefs::pinning::Stage::kSuccess) {
    drive_pinning_available_ = true;
    std::u16string free_space = ui::FormatBytes(progress.free_space);
    std::u16string required_space = ui::FormatBytes(progress.required_space);
    view_->SetRequiredSpaceInfo(required_space, free_space);
  }
}

void DrivePinningScreen::OnDecline() {
  exit_callback_.Run(Result::DECLINE);
}

void DrivePinningScreen::OnAccept() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kOobeDrivePinningEnabledDeferred,
                                  true);
  exit_callback_.Run(Result::ACCEPT);
}

void DrivePinningScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void DrivePinningScreen::HideImpl() {}

void DrivePinningScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDecline) {
    OnDecline();
    return;
  }

  if (action_id == kUserActionAccept) {
    OnAccept();
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
