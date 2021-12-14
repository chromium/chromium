// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "base/containers/contains.h"

namespace ash {
namespace quick_pair {

QuickPairMetricsLogger::QuickPairMetricsLogger(
    ScannerBroker* scanner_broker,
    PairerBroker* pairer_broker,
    UIBroker* ui_broker,
    RetroactivePairingDetector* retroactive_pairing_detector)
    : feature_usage_metrics_logger_(
          std::make_unique<FastPairFeatureUsageMetricsLogger>()) {
  scanner_broker_observation_.Observe(scanner_broker);
  retroactive_pairing_detector_observation_.Observe(
      retroactive_pairing_detector);
  pairer_broker_observation_.Observe(pairer_broker);
  ui_broker_observation_.Observe(ui_broker);
}

QuickPairMetricsLogger::~QuickPairMetricsLogger() = default;

void QuickPairMetricsLogger::OnDevicePaired(scoped_refptr<Device> device) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kPairingSucceeded);
  feature_usage_metrics_logger_->RecordUsage(/*success=*/true);
  base::TimeDelta total_pair_time =
      base::TimeTicks::Now() - device_pairing_start_timestamps_[device];
  AttemptRecordingTotalUxPairTime(*device, total_pair_time);
}

void QuickPairMetricsLogger::OnPairFailure(scoped_refptr<Device> device,
                                           PairFailure failure) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kPairingFailed);
  base::TimeDelta total_pair_time =
      base::TimeTicks::Now() - device_pairing_start_timestamps_[device];
  device_pairing_start_timestamps_.erase(device);
  AttemptRecordingTotalUxPairTime(*device, total_pair_time);

  feature_usage_metrics_logger_->RecordUsage(/*success=*/false);
}

void QuickPairMetricsLogger::OnDiscoveryAction(scoped_refptr<Device> device,
                                               DiscoveryAction action) {
  switch (action) {
    case DiscoveryAction::kPairToDevice:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed);
      device_pairing_start_timestamps_[device] = base::TimeTicks::Now();
      break;
    case DiscoveryAction::kDismissedByUser:
    case DiscoveryAction::kDismissed:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissed);
      feature_usage_metrics_logger_->RecordUsage(/*success=*/true);
      break;
  }
}

void QuickPairMetricsLogger::OnPairingFailureAction(
    scoped_refptr<Device> device,
    PairingFailedAction action) {
  switch (action) {
    case PairingFailedAction::kNavigateToSettings:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kErrorUiSettingsPressed);
      break;
    case PairingFailedAction::kDismissedByUser:
    case PairingFailedAction::kDismissed:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kErrorUiDismissed);
      break;
  }
}

void QuickPairMetricsLogger::OnDeviceFound(scoped_refptr<Device> device) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kDiscoveryUiShown);
}

void QuickPairMetricsLogger::OnRetroactivePairFound(
    scoped_refptr<Device> device) {
  AttemptRecordingFastPairRetroactiveEngagementFlow(
      *device,
      FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown);
}

void QuickPairMetricsLogger::OnAssociateAccountAction(
    scoped_refptr<Device> device,
    AssociateAccountAction action) {
  switch (action) {
    case AssociateAccountAction::kAssoicateAccount:
      if (base::Contains(learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountSavePressedAfterLearnMorePressed);
        learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed);
      break;
    case AssociateAccountAction::kLearnMore:
      // We need to record whether or not the Associate Account UI for this
      // device has had the Learn More button pressed because since the
      // Learn More button is not a terminal state, we need to record
      // if the subsequent terminal states were reached after the user
      // has learned more about saving their accounts. So we will check
      // this map when the user dismisses or saves their account in order
      // to capture whether or not the user elected to learn more beforehand.
      learn_more_devices_.insert(device);

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountLearnMorePressed);
      break;
    case AssociateAccountAction::kDismissedByUser:
      if (base::Contains(learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedByUserAfterLearnMorePressed);
        learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountUiDismissedByUser);
      break;
    case AssociateAccountAction::kDismissed:
      if (base::Contains(learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedAfterLearnMorePressed);
        learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed);
      break;
  }
}

void QuickPairMetricsLogger::OnAccountKeyWrite(
    scoped_refptr<Device> device,
    absl::optional<AccountKeyFailure> error) {}

void QuickPairMetricsLogger::OnCompanionAppAction(scoped_refptr<Device> device,
                                                  CompanionAppAction action) {}

void QuickPairMetricsLogger::OnDeviceLost(scoped_refptr<Device> device) {}

}  // namespace quick_pair
}  // namespace ash
