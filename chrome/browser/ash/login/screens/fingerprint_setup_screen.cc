// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"

#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/device_service.h"
#include "ui/base/l10n/l10n_util.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
namespace {

constexpr char kUserActionSetupDone[] = "setup-done";
constexpr char kUserActionSetupSkippedOnStart[] = "setup-skipped-on-start";
constexpr char kUserActionSetupSkippedInFlow[] = "setup-skipped-in-flow";
constexpr char kUserActionAddAnotherFinger[] = "add-another-finger";

struct FingerprintSetupUserAction {
  const char* name_;
  FingerprintSetupScreen::UserAction uma_name_;
};

const FingerprintSetupUserAction actions[] = {
    {kUserActionSetupDone, FingerprintSetupScreen::UserAction::kSetupDone},
    {kUserActionAddAnotherFinger,
     FingerprintSetupScreen::UserAction::kAddAnotherFinger},
    {kUserActionSetupSkippedOnStart,
     FingerprintSetupScreen::UserAction::kSkipButtonClickedOnStart},
    {kUserActionSetupSkippedInFlow,
     FingerprintSetupScreen::UserAction::kSkipButtonClickedInFlow},
};

void RecordFingerprintSetupUserAction(
    FingerprintSetupScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.FingerprintSetupScreen.UserActions",
                                value);
}

bool IsFingerprintUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_)
      return true;
  }
  return false;
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordFingerprintSetupUserAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected action id: " << action_id;
}

// The max number of fingerprints that can be stored.
constexpr int kMaxAllowedFingerprints = 3;

// Determines what the newly added fingerprint's name should be.
std::string GetDefaultFingerprintName(int enrolled_finger_count) {
  DCHECK(enrolled_finger_count < kMaxAllowedFingerprints);
  switch (enrolled_finger_count) {
    case 0:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_1);
    case 1:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_2);
    case 2:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_3);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();
}

}  // namespace

// static
std::string FingerprintSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return "Skipped";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

FingerprintSetupScreen::FingerprintSetupScreen(
    base::WeakPtr<FingerprintSetupScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FingerprintSetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      auth_performer_(UserDataAuthClient::Get()),
      fp_engine_(&auth_performer_) {
  content::GetDeviceService().BindFingerprint(
      fp_service_.BindNewPipeAndPassReceiver());
  fp_service_->AddFingerprintObserver(receiver_.BindNewPipeAndPassRemote());
  DCHECK(view_);
}

FingerprintSetupScreen::~FingerprintSetupScreen() = default;

bool FingerprintSetupScreen::ShouldBeSkipped(
    const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests ||
      !fp_engine_.IsFingerprintEnabled(
          *ProfileManager::GetActiveUserProfile()->GetPrefs(),
          LegacyFingerprintEngine::Purpose::kAny) ||
      chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    return true;
  }
  return false;
}

bool FingerprintSetupScreen::MaybeSkip(WizardContext& context) {
  if (ShouldBeSkipped(context)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void FingerprintSetupScreen::ShowImpl() {
  StartAddingFinger();
  if (view_)
    view_->Show();
}

void FingerprintSetupScreen::HideImpl() {
  // Clean up existing fingerprint enroll session.
  if (enroll_session_started_) {
    fp_service_->CancelCurrentEnrollSession(
        base::BindOnce(&FingerprintSetupScreen::OnCancelCurrentEnrollSession,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void FingerprintSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (!IsFingerprintUserAction(action_id)) {
    BaseScreen::OnUserAction(args);
    return;
  }
  RecordUserAction(action_id);
  if (action_id == kUserActionSetupDone) {
    exit_callback_.Run(Result::DONE);
  } else if (action_id == kUserActionSetupSkippedOnStart ||
             action_id == kUserActionSetupSkippedInFlow) {
    exit_callback_.Run(Result::SKIPPED);
  } else if (action_id == kUserActionAddAnotherFinger) {
    StartAddingFinger();
  }
}

void FingerprintSetupScreen::OnRestarted() {
  VLOG(1) << "Fingerprint session restarted.";
}

void FingerprintSetupScreen::OnStatusChanged(
    device::mojom::BiometricsManagerStatus status) {
  VLOG(1) << "Fingerprint status changed. New state="
          << static_cast<int>(status);
}

void FingerprintSetupScreen::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  VLOG(1) << "Receive fingerprint enroll scan result. scan_result="
          << scan_result
          << ", enroll_session_complete=" << enroll_session_complete
          << ", percent_complete=" << percent_complete;
  if (view_) {
    view_->OnEnrollScanDone(scan_result, enroll_session_complete,
                            percent_complete);
  }
  if (!enroll_session_complete)
    return;

  enroll_session_started_ = false;
  ++enrolled_finger_count_;
  if (view_) {
    view_->EnableAddAnotherFinger(
        /* enable = */ enrolled_finger_count_ < kMaxAllowedFingerprints);
  }

  // Update the number of registered fingers, it's fine to override because
  // this is the first time user log in and have no finger registered.
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
      prefs::kQuickUnlockFingerprintRecord, enrolled_finger_count_);
}

void FingerprintSetupScreen::OnAuthScanDone(
    const device::mojom::FingerprintMessagePtr ptr,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {}

void FingerprintSetupScreen::OnSessionFailed() {
  // TODO(xiaoyinh): Add more user visible information when available.
  LOG(ERROR) << "Fingerprint session failed.";
}

void FingerprintSetupScreen::StartAddingFinger() {
  DCHECK(enrolled_finger_count_ < kMaxAllowedFingerprints);

  enroll_session_started_ = true;
  fp_service_->StartEnrollSession(
      ProfileHelper::Get()->GetUserIdHashFromProfile(
          ProfileManager::GetActiveUserProfile()),
      GetDefaultFingerprintName(enrolled_finger_count_));
}

void FingerprintSetupScreen::OnCancelCurrentEnrollSession(bool success) {
  if (!success)
    LOG(ERROR) << "Failed to cancel current fingerprint enroll session.";
}

}  // namespace ash
