// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr const char kAcceptedSetupUserAction[] = "setup-accepted";
constexpr const char kDeclinedSetupUserAction[] = "setup-declined";

}  // namespace

// static
std::string MultiDeviceSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

MultiDeviceSetupScreen::MultiDeviceSetupScreen(
    base::WeakPtr<MultiDeviceSetupScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(MultiDeviceSetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

MultiDeviceSetupScreen::~MultiDeviceSetupScreen() {
  if (skipped_ && !skipped_reason_determined_) {
    RecordOobeMultideviceScreenSkippedReasonHistogram(
        OobeMultideviceScreenSkippedReason::
            kDestroyedBeforeReasonCouldBeDetermined);
  }
}

void MultiDeviceSetupScreen::TryInitSetupClient() {
  if (!setup_client_) {
    setup_client_ =
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
  }
}

bool MultiDeviceSetupScreen::MaybeSkip(WizardContext& context) {
  // Skip multidevice setup screen during oobe.SmokeEndToEnd test.
  if (switches::ShouldMultideviceScreenBeSkippedForTesting()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Only attempt the setup flow for non-guest users.
  if (context.skip_post_login_screens_for_tests ||
      chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    RecordOobeMultideviceScreenSkippedReasonHistogram(
        OobeMultideviceScreenSkippedReason::kPublicSessionOrEphemeralLogin);
    skipped_ = true;
    return true;
  }

  TryInitSetupClient();

  // Skip if the setup client wasn't successfully initialized.
  if (!setup_client_) {
    RecordOobeMultideviceScreenSkippedReasonHistogram(
        OobeMultideviceScreenSkippedReason::kSetupClientNotInitialized);
    exit_callback_.Run(Result::NOT_APPLICABLE);
    skipped_ = true;
    return true;
  }

  // Use WizardContext here to check if user already connected phone during
  // Quick Start. If so, the multidevice setup screen will display UI
  // enhancements.
  const std::string& phone_instance_id = context.quick_start_phone_instance_id;
  if (!phone_instance_id.empty()) {
    setup_client_->SetQuickStartPhoneInstanceID(phone_instance_id);
    quick_start_metrics_ = std::make_unique<quick_start::QuickStartMetrics>();
  }

  // Do not skip if potential host exists but none is set yet.
  if (setup_client_->GetHostStatus().first ==
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    skipped_ = false;
    return false;
  }

  skipped_ = true;
  VLOG(1) << "Skipping MultiDevice setup screen; host status: "
          << setup_client_->GetHostStatus().first;
  exit_callback_.Run(Result::NOT_APPLICABLE);

  // Determine underlying reason why the screen is being skipped.
  GetBetterTogetherMetadataStatus();

  return true;
}

void MultiDeviceSetupScreen::ShowImpl() {
  if (view_) {
    view_->Show();
  }

  if (quick_start_metrics_ != nullptr) {
    quick_start_metrics_->RecordScreenOpened(
        quick_start::QuickStartMetrics::ScreenName::kUnifiedSetup);
  }

  // Record that user was presented with setup flow to prevent spam
  // notifications from suggesting setup in the future.
  multidevice_setup::OobeCompletionTracker* oobe_completion_tracker =
      multidevice_setup::OobeCompletionTrackerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  DCHECK(oobe_completion_tracker);
  oobe_completion_tracker->MarkOobeShown();
}

void MultiDeviceSetupScreen::HideImpl() {}

void MultiDeviceSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kAcceptedSetupUserAction) {
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kAccepted);
    MaybeRecordQuickStartScreenClosed();
    exit_callback_.Run(Result::NEXT);
  } else if (action_id == kDeclinedSetupUserAction) {
    MaybeRecordQuickStartScreenClosed();
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kDeclined);
    exit_callback_.Run(Result::NEXT);
  } else {
    BaseScreen::OnUserAction(args);
    NOTREACHED_IN_MIGRATION();
  }
}

void MultiDeviceSetupScreen::GetBetterTogetherMetadataStatus() {
  if (!device_sync_client_) {
    device_sync_client_ = device_sync::DeviceSyncClientFactory::GetForProfile(
        ProfileManager::GetActiveUserProfile());
  }

  device_sync_client_->GetBetterTogetherMetadataStatus(
      base::BindOnce(&MultiDeviceSetupScreen::OnGetBetterTogetherMetadataStatus,
                     weak_factory_.GetWeakPtr()));
}

void MultiDeviceSetupScreen::OnGetBetterTogetherMetadataStatus(
    device_sync::BetterTogetherMetadataStatus status) {
  switch (status) {
    case device_sync::BetterTogetherMetadataStatus::kMetadataDecrypted:
      // If the better together metadata status is in its expected final state,
      // then we know that device sync successfully finished. Investigate the
      // host status for more granular information.
      setup_client_->GetHostStatus().first ==
              multidevice_setup::mojom::HostStatus::kNoEligibleHosts
          ? RecordOobeMultideviceScreenSkippedReasonHistogram(
                OobeMultideviceScreenSkippedReason::
                    kDeviceSyncFinishedAndNoEligibleHostPhone)
          : RecordOobeMultideviceScreenSkippedReasonHistogram(
                OobeMultideviceScreenSkippedReason::kHostPhoneAlreadySet);
      return;
    case device_sync::BetterTogetherMetadataStatus::
        kWaitingToProcessDeviceMetadata:
      [[fallthrough]];
    case device_sync::BetterTogetherMetadataStatus::kGroupPrivateKeyMissing:
      // If the better together metadata status is
      // kWaitingToProcessDeviceMetadata or kGroupPrivateKeyMissing, we must
      // inspect the group private key status to get a more granular
      // understanding.
      GetGroupPrivateKeyStatus();
      return;
    case device_sync::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kDeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetch);
      return;
    case device_sync::BetterTogetherMetadataStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kNoDeviceSyncerSetDuringBetterTogetherMetadataStatusFetch);
      return;
    case device_sync::BetterTogetherMetadataStatus::kEncryptedMetadataEmpty:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::kEncryptedMetadataEmpty);
      return;
  }
}

void MultiDeviceSetupScreen::GetGroupPrivateKeyStatus() {
  if (!device_sync_client_) {
    device_sync_client_ = device_sync::DeviceSyncClientFactory::GetForProfile(
        ProfileManager::GetActiveUserProfile());
  }

  device_sync_client_->GetGroupPrivateKeyStatus(
      base::BindOnce(&MultiDeviceSetupScreen::OnGetGroupPrivateKeyStatus,
                     weak_factory_.GetWeakPtr()));
}

void MultiDeviceSetupScreen::OnGetGroupPrivateKeyStatus(
    device_sync::GroupPrivateKeyStatus status) {
  switch (status) {
    case device_sync::GroupPrivateKeyStatus::kWaitingForGroupPrivateKey:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::kWaitingForGroupPrivateKey);
      return;
    case device_sync::GroupPrivateKeyStatus::
        kNoEncryptedGroupPrivateKeyReceived:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kNoEncryptedGroupPrivateKeyReceived);
      return;
    case device_sync::GroupPrivateKeyStatus::kEncryptedGroupPrivateKeyEmpty:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::kEncryptedGroupPrivateKeyEmpty);
      return;
    case device_sync::GroupPrivateKeyStatus::
        kLocalDeviceSyncBetterTogetherKeyMissing:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kLocalDeviceSyncBetterTogetherKeyMissing);
      return;
    case device_sync::GroupPrivateKeyStatus::kGroupPrivateKeyDecryptionFailed:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::kGroupPrivateKeyDecryptionFailed);
      return;
    case device_sync::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseDeviceSyncIsNotInitialized:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kDeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetch);
      return;
    case device_sync::GroupPrivateKeyStatus::
        kStatusUnavailableBecauseNoDeviceSyncerSet:
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::
              kNoDeviceSyncerSetDuringGroupPrivateKeyStatusFetch);
      return;
    case device_sync::GroupPrivateKeyStatus::
        kGroupPrivateKeySuccessfullyDecrypted:
      // This is the expected finished status of the GroupPrivateKey. If this
      // point is reached, there's no known reason why the setup client wouldn't
      // be initialized.
      RecordOobeMultideviceScreenSkippedReasonHistogram(
          OobeMultideviceScreenSkippedReason::kUnknown);
      return;
  }
}

void MultiDeviceSetupScreen::MaybeRecordQuickStartScreenClosed() {
  if (quick_start_metrics_ != nullptr) {
    quick_start_metrics_->RecordScreenClosed(
        quick_start::QuickStartMetrics::ScreenName::kUnifiedSetup,
        quick_start::QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow);
  }
}
void MultiDeviceSetupScreen::RecordOobeMultideviceScreenSkippedReasonHistogram(
    OobeMultideviceScreenSkippedReason reason) {
  skipped_reason_determined_ = true;
  UMA_HISTOGRAM_ENUMERATION(
      "OOBE.StepShownStatus.Multidevice-setup-screen.Skipped", reason);
}

void MultiDeviceSetupScreen::RecordMultiDeviceSetupOOBEUserChoiceHistogram(
    MultiDeviceSetupOOBEUserChoice value) {
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup.OOBE.UserChoice", value);
}

}  // namespace ash
