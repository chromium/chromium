// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace quick_pair {

namespace {

void AttemptToRecordEngagementFunnelFlowWithMetadata(
    scoped_refptr<Device> device,
    FastPairEngagementFlowEvent event,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  // TODO(b/262452942): Add logic to retry fetching metadata to record funnel
  // flow. Currently we are missing logging if fetching metadata fails, and we
  // do not retry. |has_retryable_error| is currently not used, but will be if
  // we decide to retry.
  if (!device_metadata) {
    return;
  }

  RecordFastPairDeviceAndNotificationSpecificEngagementFlow(
      *device, device_metadata->GetDetails(), event);
}

void GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
    scoped_refptr<Device> device,
    FastPairEngagementFlowEvent event) {
  FastPairRepository::Get()->GetDeviceMetadata(
      device->metadata_id(),
      base::BindOnce(&AttemptToRecordEngagementFunnelFlowWithMetadata, device,
                     event));
}

void AttemptToRecordRetroactiveEngagementFunnelFlowWithMetadata(
    scoped_refptr<Device> device,
    FastPairRetroactiveEngagementFlowEvent event,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  // TODO(b/262452942): Add logic to retry fetching metadata to record funnel
  // flow. Currently we are missing logging if fetching metadata fails, and we
  // do not retry.
  if (!device_metadata) {
    return;
  }

  RecordFastPairDeviceAndNotificationSpecificRetroactiveEngagementFlow(
      *device, device_metadata->GetDetails(), event);
}

void GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
    scoped_refptr<Device> device,
    FastPairRetroactiveEngagementFlowEvent event) {
  FastPairRepository::Get()->GetDeviceMetadata(
      device->metadata_id(),
      base::BindOnce(
          &AttemptToRecordRetroactiveEngagementFunnelFlowWithMetadata, device,
          event));
}

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

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

const device::BluetoothDevice* QuickPairMetricsLogger::GetBluetoothDevice(
    scoped_refptr<Device> device) const {
  if (!adapter_) {
    return nullptr;
  }

  device::BluetoothDevice* bt_device = nullptr;
  // First, try to get the Bluetooth Device via the classic address since it's
  // more stable than the BLE address.
  if (device->classic_address()) {
    bt_device = adapter_->GetDevice(device->classic_address().value());
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Structured Classic device found: " << (bt_device ? "yes" : "no");
  }

  if (!bt_device) {
    bt_device = adapter_->GetDevice(device->ble_address());
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Structured LE device found: " << (bt_device ? "yes" : "no");
  }

  return bt_device;
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
  GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
      device, FastPairEngagementFlowEvent::kPairingSucceeded);
  RecordPairingResult(*device, /*success=*/true);
  feature_usage_metrics_logger_->RecordUsage(/*success=*/true);

  base::TimeDelta total_pair_time =
      base::TimeTicks::Now() - device_pairing_start_timestamps_[device];
  AttemptRecordingTotalUxPairTime(*device, total_pair_time);
  RecordPairingMethod(PairingMethod::kFastPair);

  if (device->protocol() == Protocol::kFastPairInitial) {
    RecordInitialSuccessFunnelFlow(
        FastPairInitialSuccessFunnelEvent::kPairingComplete);
  }

  // The classic address is assigned to the Device during the
  // initial Fast Pair pairing protocol during the key exchange, and if it
  // doesn't exist, then it wasn't properly paired during initial Fast Pair
  // pairing. We want to save the addresses here in the event that the
  // Bluetooth adapter pairing event fires, so we can detect when a device
  // was paired solely via classic bluetooth, instead of Fast Pair.
  if (device->classic_address()) {
    fast_pair_addresses_.insert(device->classic_address().value());
  }
}

void QuickPairMetricsLogger::OnPairFailure(scoped_refptr<Device> device,
                                           PairFailure failure) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kPairingFailed);
  GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
      device, FastPairEngagementFlowEvent::kPairingFailed);
  base::TimeDelta total_pair_time =
      base::TimeTicks::Now() - device_pairing_start_timestamps_[device];
  device_pairing_start_timestamps_.erase(device);
  AttemptRecordingTotalUxPairTime(*device, total_pair_time);

  feature_usage_metrics_logger_->RecordUsage(/*success=*/false);
  RecordPairingFailureReason(*device, failure);
  RecordPairingResult(*device, /*success=*/false);
  RecordStructuredPairFailure(*device, failure);
}

void QuickPairMetricsLogger::OnDiscoveryAction(scoped_refptr<Device> device,
                                               DiscoveryAction action) {
  switch (action) {
    case DiscoveryAction::kPairToDevice: {
      switch (device->protocol()) {
        case Protocol::kFastPairSubsequent:
          RecordSubsequentSuccessFunnelFlow(
              FastPairSubsequentSuccessFunnelEvent::kNotificationsClicked);
          RecordStructuredPairingStarted(*device, GetBluetoothDevice(device));
          break;
        case Protocol::kFastPairInitial:
          RecordInitialSuccessFunnelFlow(
              FastPairInitialSuccessFunnelEvent::kNotificationsClicked);
          RecordStructuredPairingStarted(*device, GetBluetoothDevice(device));
          break;
        case Protocol::kFastPairRetroactive:
          break;
      }

      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiConnectPressedAfterLearnMorePressed);
        GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
            device, FastPairEngagementFlowEvent::
                        kDiscoveryUiConnectPressedAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      PrefService* pref = GetLastActiveUserPrefService();
      if (pref->FindPreference(ash::prefs::kUserPairedWithFastPair)) {
        pref->SetBoolean(ash::prefs::kUserPairedWithFastPair, true);
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed);
      device_pairing_start_timestamps_[device] = base::TimeTicks::Now();
      break;
    }
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
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed);
      break;
    case DiscoveryAction::kDismissedByUser:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiDismissedByUserAfterLearnMorePressed);
        GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
            device, FastPairEngagementFlowEvent::
                        kDiscoveryUiDismissedByUserAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser);
      break;
    case DiscoveryAction::kDismissedByOs:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiDismissedAfterLearnMorePressed);
        GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
            device, FastPairEngagementFlowEvent::
                        kDiscoveryUiDismissedAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissed);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kDiscoveryUiDismissed);
      break;
    case DiscoveryAction::kDismissedByTimeout:
      if (base::Contains(discovery_learn_more_devices_, device)) {
        AttemptRecordingFastPairEngagementFlow(
            *device, FastPairEngagementFlowEvent::
                         kDiscoveryUiDismissedByTimeoutAfterLearnMorePressed);
        GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
            device, FastPairEngagementFlowEvent::
                        kDiscoveryUiDismissedByTimeoutAfterLearnMorePressed);
        discovery_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout);
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
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kErrorUiSettingsPressed);
      break;
    case PairingFailedAction::kDismissedByUser:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kErrorUiDismissedByUser);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kErrorUiDismissedByUser);
      break;
    case PairingFailedAction::kDismissed:
      AttemptRecordingFastPairEngagementFlow(
          *device, FastPairEngagementFlowEvent::kErrorUiDismissed);
      GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
          device, FastPairEngagementFlowEvent::kErrorUiDismissed);
      break;
  }
}

void QuickPairMetricsLogger::OnDeviceFound(scoped_refptr<Device> device) {
  AttemptRecordingFastPairEngagementFlow(
      *device, FastPairEngagementFlowEvent::kDiscoveryUiShown);
  GetDeviceMetadataAndLogEngagementFunnelWithMetadata(
      device, FastPairEngagementFlowEvent::kDiscoveryUiShown);
  RecordStructuredDiscoveryNotificationShown(*device,
                                             GetBluetoothDevice(device));
}

void QuickPairMetricsLogger::OnPairingStart(scoped_refptr<Device> device) {
  RecordFastPairInitializePairingProcessEvent(
      *device, FastPairInitializePairingProcessEvent::kInitializationStarted);

  switch (device->protocol()) {
    case Protocol::kFastPairSubsequent:
      RecordSubsequentSuccessFunnelFlow(
          FastPairSubsequentSuccessFunnelEvent::kInitializationStarted);
      break;
    case Protocol::kFastPairInitial:
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kInitializationStarted);
      break;
    case Protocol::kFastPairRetroactive:
      RecordRetroactiveSuccessFunnelFlow(
          FastPairRetroactiveSuccessFunnelEvent::kInitializationStarted);
      break;
  }
}

void QuickPairMetricsLogger::OnHandshakeComplete(scoped_refptr<Device> device) {
  RecordFastPairInitializePairingProcessEvent(
      *device, FastPairInitializePairingProcessEvent::kInitializationComplete);

  switch (device->protocol()) {
    case Protocol::kFastPairSubsequent:
      RecordSubsequentSuccessFunnelFlow(
          FastPairSubsequentSuccessFunnelEvent::kPairingStarted);
      break;
    case Protocol::kFastPairInitial:
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kPairingStarted);
      break;
    case Protocol::kFastPairRetroactive:
      RecordRetroactiveSuccessFunnelFlow(
          FastPairRetroactiveSuccessFunnelEvent::kWritingAccountKey);
      break;
  }
}

void QuickPairMetricsLogger::OnPairingComplete(scoped_refptr<Device> device) {
  switch (device->protocol()) {
    case Protocol::kFastPairSubsequent:
      RecordSubsequentSuccessFunnelFlow(
          FastPairSubsequentSuccessFunnelEvent::kProcessComplete);
      RecordStructuredPairingComplete(*device, GetBluetoothDevice(device));
      break;
    case Protocol::kFastPairInitial:
      RecordInitialSuccessFunnelFlow(
          FastPairInitialSuccessFunnelEvent::kProcessComplete);
      RecordStructuredPairingComplete(*device, GetBluetoothDevice(device));
      break;
    case Protocol::kFastPairRetroactive:
      break;
  }
}

void QuickPairMetricsLogger::OnRetroactivePairFound(
    scoped_refptr<Device> device) {
  // When a device for the retroactive pairing scenario is detected, the
  // corresponding "Associate Account" retroactive pairing notification is
  // presenting to the user.
  AttemptRecordingFastPairRetroactiveEngagementFlow(
      *device,
      FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown);
  GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
      device, FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown);
  RecordRetroactiveSuccessFunnelFlow(
      FastPairRetroactiveSuccessFunnelEvent::kDeviceDetected);
  RecordStructuredPairingStarted(*device, GetBluetoothDevice(device));
}

void QuickPairMetricsLogger::OnAssociateAccountAction(
    scoped_refptr<Device> device,
    AssociateAccountAction action) {
  switch (action) {
    case AssociateAccountAction::kAssociateAccount: {
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountSavePressedAfterLearnMorePressed);
        GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
            device, FastPairRetroactiveEngagementFlowEvent::
                        kAssociateAccountSavePressedAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
        break;
      }

      PrefService* pref = GetLastActiveUserPrefService();
      if (pref->FindPreference(ash::prefs::kUserPairedWithFastPair)) {
        pref->SetBoolean(ash::prefs::kUserPairedWithFastPair, true);
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed);
      GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
          device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed);
      RecordRetroactiveSuccessFunnelFlow(
          FastPairRetroactiveSuccessFunnelEvent::kSaveRequested);
      break;
    }
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
      GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
          device, FastPairRetroactiveEngagementFlowEvent::
                      kAssociateAccountLearnMorePressed);
      break;
    case AssociateAccountAction::kDismissedByUser:
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedByUserAfterLearnMorePressed);
        GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
            device, FastPairRetroactiveEngagementFlowEvent::
                        kAssociateAccountDismissedByUserAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountUiDismissedByUser);
      GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
          device, FastPairRetroactiveEngagementFlowEvent::
                      kAssociateAccountUiDismissedByUser);
      break;
    case AssociateAccountAction::kDismissedByTimeout:
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device,
            FastPairRetroactiveEngagementFlowEvent::
                kAssociateAccountDismissedByTimeoutAfterLearnMorePressed);
        GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
            device,
            FastPairRetroactiveEngagementFlowEvent::
                kAssociateAccountDismissedByTimeoutAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device, FastPairRetroactiveEngagementFlowEvent::
                       kAssociateAccountUiDismissedByTimeout);
      GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
          device, FastPairRetroactiveEngagementFlowEvent::
                      kAssociateAccountUiDismissedByTimeout);
      break;
    case AssociateAccountAction::kDismissedByOs:
      if (base::Contains(associate_account_learn_more_devices_, device)) {
        AttemptRecordingFastPairRetroactiveEngagementFlow(
            *device, FastPairRetroactiveEngagementFlowEvent::
                         kAssociateAccountDismissedAfterLearnMorePressed);
        GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
            device, FastPairRetroactiveEngagementFlowEvent::
                        kAssociateAccountDismissedAfterLearnMorePressed);
        associate_account_learn_more_devices_.erase(device);
        break;
      }

      AttemptRecordingFastPairRetroactiveEngagementFlow(
          *device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed);
      GetDeviceMetadataAndLogRetroactiveEngagementFunnelWithMetadata(
          device,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed);
      break;
  }
}

void QuickPairMetricsLogger::OnAccountKeyWrite(
    scoped_refptr<Device> device,
    std::optional<AccountKeyFailure> error) {
  switch (device->protocol()) {
    case Protocol::kFastPairSubsequent:
      // TODO(b/259443372): Record this case once we implement account key
      // writing in all scenarios,
      NOTREACHED();
    case Protocol::kFastPairInitial:
      break;
    case Protocol::kFastPairRetroactive:
      RecordRetroactivePairingResult(/*success=*/!error.has_value());

      if (!error.has_value()) {
        RecordRetroactiveSuccessFunnelFlow(
            FastPairRetroactiveSuccessFunnelEvent::kAccountKeyWrittenToDevice);
        RecordStructuredPairingComplete(*device, GetBluetoothDevice(device));
      }
      // TODO(jackshira): Log new PairFailure case here.
      break;
  }

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
