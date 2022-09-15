// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "base/containers/contains.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace quick_pair {

QuickPairMetricsLogger::QuickPairMetricsLogger(
    ScannerBroker* scanner_broker,
    PairerBroker* pairer_broker,
    UIBroker* ui_broker,
    RetroactivePairingDetector* retroactive_pairing_detector)
    : feature_usage_metrics_logger_(
          std::make_unique<FastPairFeatureUsageMetricsLogger>()) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &QuickPairMetricsLogger::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));

  scanner_broker_observation_.Observe(scanner_broker);
  retroactive_pairing_detector_observation_.Observe(
      retroactive_pairing_detector);
  pairer_broker_observation_.Observe(pairer_broker);
  ui_broker_observation_.Observe(ui_broker);
}

QuickPairMetricsLogger::~QuickPairMetricsLogger() = default;

void QuickPairMetricsLogger::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_observation_.Observe(adapter_.get());
}

void QuickPairMetricsLogger::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  // This event fires whenever a device pairing has changed with the adapter.
  // If the |new_paired_status| is false, it means a device was unpaired with
  // the adapter, so we early return since it would not be a device that has
  // been paired alternatively. If the device that was paired to that fires this
  // event is a device we just paired to with Fast Pair, then we early return
  // since it also wouldn't be one that was alternatively pair to. We want to
  // only continue our check here if we have a newly paired device that was
  // paired with classic Bluetooth pairing.
  const std::string& classic_address = device->GetAddress();
  if (!new_paired_status ||
      base::Contains(fast_pair_addresses_, classic_address)) {
    return;
  }

  RecordPairingMethod(PairingMethod::kSystemPairingUi);
}

void QuickPairMetricsLogger::OnDevicePaired(scoped_refptr<Device> device) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kPairingSucceeded);
  RecordPairingResult(*device, /*success=*/true);
  feature_usage_metrics_logger_->RecordUsage(/*success=*/true);

  base::TimeDelta total_pair_time =
      base::TimeTicks::Now() - device_pairing_start_timestamps_[device];
  AttemptRecordingTotalUxPairTime(*device, total_pair_time);
  RecordPairingMethod(PairingMethod::kFastPair);

  // The classic address is assigned to the Device during the
  // initial Fast Pair pairing protocol during the key exchange, and if it
  // doesn't exist, then it wasn't properly paired during initial Fast Pair
  // pairing. We want to save the addresses here in the event that the
  // Bluetooth adapter pairing event fires, so we can detect when a device
  // was paired solely via classic bluetooth, instead of Fast Pair.
  if (device->classic_address())
    fast_pair_addresses_.insert(device->classic_address().value());
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
  RecordPairingFailureReason(*device, failure);
  RecordPairingResult(*device, /*success=*/false);
}

void QuickPairMetricsLogger::OnDiscoveryAction(scoped_refptr<Device> device,
                                               DiscoveryAction action) {
  switch (action) {
    case DiscoveryAction::kPairToDevice:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiConnectPressedAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed);
      device_pairing_start_timestamps_[device] = base::TimeTicks::Now();
      break;
    case DiscoveryAction::kLearnMore:
      // We need to record whether or not the Discovery UI for this
      // device has had the Learn More button pressed because since the
      // Learn More button is not a terminal state, we need to record
      // if the subsequent terminal states were reached after the user
      // has learned more about saving their accounts. So we will check
      // this map when the user dismisses or saves their account in order
      // to capture whether or not the user elected to learn more beforehand.
      discovery_learn_more_devices_.insert(device);

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed);
      break;
    case DiscoveryAction::kDismissedByUser:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiDismissedByUserAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser);
      break;
    case DiscoveryAction::kDismissed:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiDismissedAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissed);
      break;
    case DiscoveryAction::kAlreadyDisplayed:
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
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kErrorUiDismissedByUser);
      break;
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
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountSavePressedAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
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
      associate_account_learn_more_devices_.insert(device);

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountLearnMorePressed);
      break;
    case AssociateAccountAction::kDismissedByUser:
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedByUserAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountUiDismissedByUser);
      break;
    case AssociateAccountAction::kDismissed:
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
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
    absl::optional<AccountKeyFailure> error) {
  if (device->protocol == Protocol::kFastPairRetroactive)
    RecordRetroactivePairingResult(/*success=*/!error.has_value());

  if (error.has_value()) {
    RecordAccountKeyResult(*device, /*success=*/false);
    RecordAccountKeyFailureReason(*device, error.value());
    feature_usage_metrics_logger_->RecordUsage(/*success=*/false);
    return;
  }

  RecordAccountKeyResult(*device, /*success=*/true);
  feature_usage_metrics_logger_->RecordUsage(/*success=*/true);
}

void QuickPairMetricsLogger::OnCompanionAppAction(scoped_refptr<Device> device,
                                                  CompanionAppAction action) {}

void QuickPairMetricsLogger::OnDeviceLost(scoped_refptr<Device> device) {}

}  // namespace quick_pair
}  // namespace ash
