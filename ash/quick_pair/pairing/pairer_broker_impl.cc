// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/pairer_broker_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

constexpr int kMaxFailureRetryCount = 3;
constexpr int kMaxNumHandshakeAttempts = 3;

// 1s delay after cancelling pairing was chosen to align with Android's Fast
// Pair implementation.
constexpr base::TimeDelta kCancelPairingRetryDelay = base::Seconds(1);

// 1s delay after handshake failure to allow failed handshake to tear down.
// TODO(b/265311455): implement handshake factory to handle retry logic.
constexpr base::TimeDelta kRetryHandshakeDelay = base::Seconds(1);

}  // namespace

namespace ash {
namespace quick_pair {

PairerBrokerImpl::PairerBrokerImpl() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &PairerBrokerImpl::OnGetAdapter, weak_pointer_factory_.GetWeakPtr()));
}

void PairerBrokerImpl::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
}

PairerBrokerImpl::~PairerBrokerImpl() = default;

void PairerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PairerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PairerBrokerImpl::PairDevice(scoped_refptr<Device> device) {
  if (ash::features::IsFastPairBleRotationEnabled() &&
      device->protocol() == Protocol::kFastPairRetroactive &&
      model_id_to_current_ble_address_map_.contains(device->metadata_id()) &&
      model_id_to_current_ble_address_map_[device->metadata_id()] !=
          device->ble_address()) {
    // There is already an entry in the map for the same model id that we have,
    // see if a handshake has already been created for it as well.
    auto* handshake = FastPairHandshakeLookup::GetInstance()->Get(
        model_id_to_current_ble_address_map_[device->metadata_id()]);
    if (handshake) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__
          << ": A handshake already occurred for this device using a "
             "different BLE Address, setting the callback and returning.";

      // If there is already a handshake created for the device. Set the
      // callback so the flow associated with that device knows it should not
      // try to write the account and instead restart the pairing process.
      handshake->BleAddressRotated(
          base::BindOnce(&PairerBrokerImpl::OnBleAddressRotation,
                         weak_pointer_factory_.GetWeakPtr(), device));
      return;
    }
  }

  model_id_to_current_ble_address_map_.insert_or_assign(device->metadata_id(),
                                                        device->ble_address());
  did_handshake_previously_complete_successfully_map_.insert_or_assign(
      device->metadata_id(), false);
  PairFastPairDevice(std::move(device));
}

void PairerBrokerImpl::OnBleAddressRotation(scoped_refptr<Device> device) {
  // The BLE Address rotated, so we need to start the Retroactive Pairing
  // process over again after clearing the state.
  EraseHandshakeAndFromPairers(device);
  did_handshake_previously_complete_successfully_map_.insert_or_assign(
      device->metadata_id(), false);
  PairFastPairDevice(std::move(device));
}

void PairerBrokerImpl::EraseHandshakeAndFromPairers(
    scoped_refptr<Device> device) {
  // |fast_pair_pairers_| and its children objects depend on the handshake
  // instance. Shut them down before destroying the handshake. Also remove
  // the GATT connection.
  pair_failure_counts_.erase(device->metadata_id());
  fast_pair_pairers_.erase(device->metadata_id());
  FastPairHandshakeLookup::GetInstance()->Erase(device);
  did_handshake_previously_complete_successfully_map_.insert_or_assign(
      device->metadata_id(), false);
  FastPairGattServiceClientLookup::GetInstance()->Erase(
      adapter_->GetDevice(device->ble_address()));
}

bool PairerBrokerImpl::IsPairing() {
  // We are guaranteed to not be pairing when the following two maps are
  // empty.
  return !fast_pair_pairers_.empty() || !pair_failure_counts_.empty();
}

void PairerBrokerImpl::StopPairing() {
  fast_pair_pairers_.clear();
  pair_failure_counts_.clear();
}

void PairerBrokerImpl::PairFastPairDevice(scoped_refptr<Device> device) {
  if (base::Contains(fast_pair_pairers_, device->metadata_id())) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Already pairing device" << device;
    RecordFastPairInitializePairingProcessEvent(
        *device, FastPairInitializePairingProcessEvent::kAlreadyPairingFailure);
    return;
  }

  // If this is a v1 pairing, we don't have to make a handshake before bonding
  // because we will pass off pairing to the classic Bluetooth pairing dialog in
  // 'FastPairPairer', so skip straight to 'StartBondingAttempt'.
  DCHECK(device->version().has_value());
  if (device->version().value() == DeviceFastPairVersion::kV1) {
    StartBondingAttempt(device);
    return;
  }

  // Otherwise, try to create a handshake.
  CreateHandshake(std::move(device));
}

void PairerBrokerImpl::CreateHandshake(scoped_refptr<Device> device) {
  if (ash::features::IsFastPairBleRotationEnabled() &&
      device->ble_address() !=
          model_id_to_current_ble_address_map_[device->metadata_id()]) {
    // If the current |device| has a different BLE Address than the address in
    // the map, abort creating the handshake and return early;
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": The device's BLE did not match the expected value, returning.";
    return;
  }

  auto* fast_pair_handshake =
      FastPairHandshakeLookup::GetInstance()->Get(device);

  if (fast_pair_handshake) {
    if (fast_pair_handshake->completed_successfully()) {
      CD_LOG(VERBOSE, Feature::FP)
          << __func__ << ": Reusing existing handshake for pair attempt.";
      RecordFastPairInitializePairingProcessEvent(
          *device, FastPairInitializePairingProcessEvent::kHandshakeReused);
      StartBondingAttempt(device);
      return;
    } else {
      // If the previous handshake did not complete successfully, erase it
      // before attempting to create a new handshake for the device.
      FastPairHandshakeLookup::GetInstance()->Erase(device);
    }
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Creating new handshake for pair attempt.";
  num_handshake_attempts_[device->metadata_id()]++;
  FastPairHandshakeLookup::GetInstance()->Create(
      adapter_, device,
      base::BindOnce(&PairerBrokerImpl::OnHandshakeComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void PairerBrokerImpl::OnHandshakeComplete(scoped_refptr<Device> device,
                                           std::optional<PairFailure> failure) {
  if (failure.has_value()) {
    CD_LOG(WARNING, Feature::FP) << __func__ << ": Handshake failed with "
                                 << device << " because: " << failure.value();
    OnHandshakeFailure(device, failure.value());
    return;
  }

  // During handshake, the device address can be set to null.
  if (!device->classic_address()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Device lost during handshake.";
    OnHandshakeFailure(device, PairFailure::kPairingDeviceLost);
    return;
  }

  if (!did_handshake_previously_complete_successfully_map_
          [device->metadata_id()]) {
    // Even if an observer does not implement this function in particular, it
    // will use the default implementation in the PairerBroker. The number
    // of observers is based on the number that call `AddObserver`, not by
    // the number that implement and override this function in their
    // derived class.
    for (auto& observer : observers_) {
      observer.OnHandshakeComplete(device);
    }

    did_handshake_previously_complete_successfully_map_.insert_or_assign(
        device->metadata_id(), true);
  }

  RecordEffectiveHandshakeSuccess(/*success=*/true);
  RecordHandshakeAttemptCount(num_handshake_attempts_[device->metadata_id()]);

  // Reset |num_handshake_attempts_| so if the handshake is lost during pairing,
  // we will attempt to create it 3 more times. This should be an extremely rare
  // situation, such as handshake happening directly before the device rotates
  // ble addresses.
  num_handshake_attempts_[device->metadata_id()] = 0;
  StartBondingAttempt(device);
}

void PairerBrokerImpl::OnHandshakeFailure(scoped_refptr<Device> device,
                                          PairFailure failure) {
  if (num_handshake_attempts_[device->metadata_id()] <
          kMaxNumHandshakeAttempts &&
      !ash::features::IsFastPairHandshakeLongTermRefactorEnabled()) {
    // Directly calling CreateHandshake() from here will cause the new
    // handshake to be nested inside the failed handshake. Use a timer to give
    // the failed handshake time to cleanup and avoid nesting.
    retry_handshake_timer_.Start(
        FROM_HERE, kRetryHandshakeDelay,
        base::BindOnce(&PairerBrokerImpl::CreateHandshake,
                       weak_pointer_factory_.GetWeakPtr(), device));
    return;
  }

  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Handshake failed to be created. Notifying observers.";
  RecordEffectiveHandshakeSuccess(/*success=*/false);
  RecordInitializationFailureReason(*device, failure);
  for (auto& observer : observers_) {
    observer.OnPairFailure(device, failure);
  }

  FastPairHandshakeLookup::GetInstance()->Erase(device);
  return;
}

void PairerBrokerImpl::StartBondingAttempt(scoped_refptr<Device> device) {
  if (!base::Contains(pair_failure_counts_, device->metadata_id())) {
    pair_failure_counts_[device->metadata_id()] = 0;

    // `OnPairingStart` is used in metrics to signal the beginning of the
    // initialization. We only want to signal this when pairing begins on the
    // first pairing attempt, otherwise observers will be notified on each retry
    // and incorrectly capture initialization in our metrics three times instead
    // of one when it begins.
    for (auto& observer : observers_) {
      observer.OnPairingStart(device);
    }
  }

  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << device;

  DCHECK(adapter_);
  fast_pair_pairers_[device->metadata_id()] =
      FastPairPairerImpl::Factory::Create(
          adapter_, device,
          base::BindOnce(&PairerBrokerImpl::OnFastPairDeviceBonded,
                         weak_pointer_factory_.GetWeakPtr()),
          base::BindOnce(&PairerBrokerImpl::OnFastPairBondingFailure,
                         weak_pointer_factory_.GetWeakPtr()),
          base::BindOnce(&PairerBrokerImpl::OnAccountKeyFailure,
                         weak_pointer_factory_.GetWeakPtr()),
          base::BindOnce(&PairerBrokerImpl::OnDisplayPasskey,
                         weak_pointer_factory_.GetWeakPtr()),
          base::BindOnce(&PairerBrokerImpl::OnFastPairProcedureComplete,
                         weak_pointer_factory_.GetWeakPtr()));
}

void PairerBrokerImpl::OnFastPairDeviceBonded(scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device;

  for (auto& observer : observers_) {
    observer.OnDevicePaired(device);
  }

  RecordPairFailureRetry(
      /*num_retries=*/pair_failure_counts_[device->metadata_id()]);
  pair_failure_counts_.erase(device->metadata_id());
}

void PairerBrokerImpl::OnFastPairBondingFailure(scoped_refptr<Device> device,
                                                PairFailure failure) {
  ++pair_failure_counts_[device->metadata_id()];
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Failure=" << failure
      << ", Failure Count = " << pair_failure_counts_[device->metadata_id()];

  device::BluetoothDevice* bt_device = nullptr;
  if (device->classic_address()) {
    bt_device = adapter_->GetDevice(device->classic_address().value());
  }

  if (pair_failure_counts_[device->metadata_id()] == kMaxFailureRetryCount) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": Reached max failure count. Notifying observers.";
    RecordProtocolPairingStep(FastPairProtocolPairingSteps::kExhaustedRetries,
                              *device);
    for (auto& observer : observers_) {
      observer.OnPairFailure(device, failure);
    }

    if (bt_device && !bt_device->IsPaired()) {
      bt_device->CancelPairing();
    }

    EraseHandshakeAndFromPairers(device);
    return;
  }

  fast_pair_pairers_.erase(device->metadata_id());

  if (bt_device && !bt_device->IsPaired()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__
        << ": Cancelling pairing and scheduling retry for failed pair attempt.";
    bt_device->CancelPairing();

    // Create a timer to wait |kCancelPairingRetryDelay| after cancelling
    // pairing to retry the pairing attempt.
    cancel_pairing_timer_.Start(
        FROM_HERE, kCancelPairingRetryDelay,
        base::BindOnce(&PairerBrokerImpl::PairFastPairDevice,
                       weak_pointer_factory_.GetWeakPtr(), device));

    return;
  }

  PairFastPairDevice(device);
}

void PairerBrokerImpl::OnAccountKeyFailure(scoped_refptr<Device> device,
                                           AccountKeyFailure failure) {
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Device=" << device << ", Failure=" << failure;

  for (auto& observer : observers_) {
    observer.OnAccountKeyWrite(device, failure);
  }

  EraseHandshakeAndFromPairers(device);
}

void PairerBrokerImpl::OnDisplayPasskey(std::u16string device_name,
                                        uint32_t passkey) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device_name;

  for (auto& observer : observers_) {
    observer.OnDisplayPasskey(device_name, passkey);
  }
}

void PairerBrokerImpl::OnFastPairProcedureComplete(
    scoped_refptr<Device> device) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Device=" << device;

  for (auto& observer : observers_) {
    observer.OnPairingComplete(device);
  }

  // If we get to this point in the flow for the initial and retroactive pairing
  // scenarios, this means that the account key has successfully been written
  // for devices with a version of V2 or higher.
  if (device->version().has_value() &&
      device->version().value() != DeviceFastPairVersion::kV1 &&
      (device->protocol() == Protocol::kFastPairInitial ||
       device->protocol() == Protocol::kFastPairRetroactive)) {
    for (auto& observer : observers_) {
      observer.OnAccountKeyWrite(device, /*error=*/std::nullopt);
    }
  }

  EraseHandshakeAndFromPairers(device);
}

}  // namespace quick_pair
}  // namespace ash
