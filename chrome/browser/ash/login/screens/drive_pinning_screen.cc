// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/screens/drive_pinning_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "components/drive/drive_pref_names.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {
namespace {

using drivefs::pinning::PinningManager;
using drivefs::pinning::Progress;

bool ShouldShowChoobeReturnButton(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return false;
  }
  return controller->ShouldShowReturnButton(DrivePinningScreenView::kScreenId);
}

void ReportScreenCompletedToChoobe(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return;
  }
  controller->OnScreenCompleted(
      *ProfileManager::GetActiveUserProfile()->GetPrefs(),
      DrivePinningScreenView::kScreenId);
}

PinningManager* GetPinningManager() {
  drive::DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
  return service && service->IsMounted() ? service->GetPinningManager()
                                         : nullptr;
}

void RecordOOBEScreenSkippedMetric(drivefs::pinning::Stage stage) {
  base::UmaHistogramEnumeration(
      "FileBrowser.GoogleDrive.BulkPinning.CHOOBEScreenStage", stage);
}

void RecordCHOOBEScreenBulkPinningInitializations(int initializations) {
  base::UmaHistogramCounts100(
      "FileBrowser.GoogleDrive.BulkPinning.CHOOBEScreenInitializations",
      initializations);
}

void RecordSettingChanged(bool initial, bool current) {
  base::UmaHistogramBoolean("OOBE.CHOOBE.SettingChanged.Drive-pinning",
                            initial != current);
}

void RecordUserSelection(bool option) {
  base::UmaHistogramBoolean("OOBE.Drive-pinning.Enabled", option);
}

}  // namespace

// static
std::string DrivePinningScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

void DrivePinningScreen::ApplyDrivePinningPref(Profile* profile) {
  auto* prefs = profile->GetPrefs();
  if (!prefs->HasPrefPath(prefs::kOobeDrivePinningEnabledDeferred)) {
    return;
  }
  bool drive_pinning =
      profile->GetPrefs()->GetBoolean(prefs::kOobeDrivePinningEnabledDeferred);
  profile->GetPrefs()->SetBoolean(drive::prefs::kDriveFsBulkPinningEnabled,
                                  drive_pinning);
  drivefs::pinning::RecordBulkPinningEnabledSource(
      drivefs::pinning::BulkPinningEnabledSource::kChoobe);

  RecordUserSelection(drive_pinning);
  prefs->ClearPref(prefs::kOobeDrivePinningEnabledDeferred);
}

DrivePinningScreen::DrivePinningScreen(
    base::WeakPtr<DrivePinningScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DrivePinningScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DrivePinningScreen::~DrivePinningScreen() = default;

bool DrivePinningScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  RecordOOBEScreenSkippedMetric(drive_pinning_stage_);
  if (drive_pinning_stage_ != drivefs::pinning::Stage::kSuccess) {
    return true;
  }

  if (features::IsOobeChoobeEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (!ignore_choobe_controller_state_for_tests_ && !choobe_controller) {
      return true;
    }

    if (choobe_controller && choobe_controller->ShouldScreenBeSkipped(
                                 DrivePinningScreenView::kScreenId)) {
      return true;
    }
  }

  return false;
}

bool DrivePinningScreen::MaybeSkip(WizardContext& context) {
  if (ShouldBeSkipped(context)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void DrivePinningScreen::StartCalculatingRequiredSpace() {
  if (started_calculating_space_) {
    return;
  }
  started_calculating_space_ = true;
  CalculateRequiredSpace();
}

void DrivePinningScreen::CalculateRequiredSpace() {
  drive::DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          ProfileManager::GetActiveUserProfile());
  if (!service) {
    LOG(ERROR) << "No Drive integration service";
    return;
  }

  Observe(service);

  PinningManager* const pinning_manager = GetPinningManager();
  if (!pinning_manager) {
    VLOG(1) << "No bulk-pinning manager";
    return;
  }

  if (bulk_pinning_initializations_ == 0) {
    ++bulk_pinning_initializations_;
  }

  RecordCHOOBEScreenBulkPinningInitializations(bulk_pinning_initializations_);
  LOG_IF(ERROR, !pinning_manager->CalculateRequiredSpace())
      << "Cannot calculate required space";
}

void DrivePinningScreen::OnProgressForTest(
    const drivefs::pinning::Progress& progress) {
  CHECK_IS_TEST();
  OnBulkPinProgress(progress);
}

void DrivePinningScreen::OnBulkPinProgress(
    const drivefs::pinning::Progress& progress) {
  drive_pinning_stage_ = progress.stage;
  if (progress.stage == drivefs::pinning::Stage::kSuccess) {
    VLOG(1) << "Finished calculating required space";
    std::u16string free_space = ui::FormatBytes(progress.free_space);
    std::u16string required_space = ui::FormatBytes(progress.required_space);
    SetRequiredSpaceInfo(required_space, free_space);
  }
}

void DrivePinningScreen::SetRequiredSpaceInfo(std::u16string required_space,
                                              std::u16string free_space) {
  if (GetRemote()->is_bound()) {
    (*GetRemote())->SetRequiredSpaceInfo(required_space, free_space);
  }
}

void DrivePinningScreen::OnBulkPinInitialized() {
  VLOG_IF(1, !started_calculating_space_)
      << "Bulk pinning initialized, but have not started calculating space";
  if (started_calculating_space_) {
    CalculateRequiredSpace();
  } else {
    ++bulk_pinning_initializations_;
  }
}

void DrivePinningScreen::OnNext(bool drive_pinning) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  bool old_value =
      profile->GetPrefs()->GetBoolean(prefs::kOobeDrivePinningEnabledDeferred);
  RecordSettingChanged(old_value, drive_pinning);
  profile->GetPrefs()->SetBoolean(prefs::kOobeDrivePinningEnabledDeferred,
                                  drive_pinning);
  exit_callback_.Run(Result::NEXT);
}

void DrivePinningScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  base::Value::Dict data;
  data.Set(
      "shouldShowReturn",
      ShouldShowChoobeReturnButton(
          WizardController::default_controller()->choobe_flow_controller()));
  view_->Show(std::move(data));
}

void DrivePinningScreen::HideImpl() {}

void DrivePinningScreen::OnReturnClicked(bool enable_drive_pinning) {
  if (is_hidden()) {
    return;
  }
  ReportScreenCompletedToChoobe(
      WizardController::default_controller()->choobe_flow_controller());
  context()->return_to_choobe_screen = true;
  OnNext(enable_drive_pinning);
}

void DrivePinningScreen::OnNextClicked(bool enable_drive_pinning) {
  if (is_hidden()) {
    return;
  }
  ReportScreenCompletedToChoobe(
      WizardController::default_controller()->choobe_flow_controller());
  OnNext(enable_drive_pinning);
}

std::string DrivePinningScreen::RetrieveChoobeSubtitle() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  bool drive_pinning =
      profile->GetPrefs()->GetBoolean(prefs::kOobeDrivePinningEnabledDeferred);
  if (drive_pinning) {
    return "choobeDevicePinningSubtitleEnabled";
  }
  return "choobeDevicePinningSubtitleDisabled";
}

ScreenSummary DrivePinningScreen::GetScreenSummary() {
  ScreenSummary summary;
  summary.screen_id = DrivePinningScreenView::kScreenId;
  summary.icon_id = "oobe-40:drive-pinning-choobe";
  summary.title_id = "choobeDrivePinningTitle";
  summary.is_revisitable = true;
  summary.is_synced = false;
  if (WizardController::default_controller()
          ->choobe_flow_controller()
          ->IsScreenCompleted(DrivePinningScreenView::kScreenId)) {
    summary.subtitle_resource = RetrieveChoobeSubtitle();
  }

  return summary;
}

}  // namespace ash
