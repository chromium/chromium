// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/pairer_broker_impl.h"

#include <memory>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_unpair_handler.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

constexpr int kMaxFailureRetryCount = 3;

// 1s delay after cancelling pairing was chosen to align with Android's Fast
// Pair implementation.
constexpr base::TimeDelta kCancelPairingRetryDelay = base::Seconds(1);

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
  fast_pair_unpair_handler_ = std::make_unique<FastPairUnpairHandler>(adapter_);
}

PairerBrokerImpl::~PairerBrokerImpl() = default;

void PairerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PairerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PairerBrokerImpl::PairDevice(scoped_refptr<Device> device) {
  switch (device->protocol) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairRetroactive:
    case Protocol::kFastPairSubsequent:
      PairFastPairDevice(std::move(device));
      break;
  }
}

void PairerBrokerImpl::EraseHandshakeAndFromPairers(
    scoped_refptr<Device> device) {
  // |fast_pair_pairers_| and its children objects depend on the handshake
  // instance. Shut them down before destroying the handshake.
  pair_failure_counts_.erase(device->ble_address);
  fast_pair_pairers_.erase(device->ble_address);
  FastPairHandshakeLookup::GetInstance()->Erase(device);
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
  if (base::Contains(fast_pair_pairers_, device->ble_address)) {
    QP_LOG(WARNING) << __func__ << ": Already pairing device" << device;
    RecordFastPairInitializePairingProcessEvent(
        *device, FastPairInitializePairingProcessEvent::kAlreadyPairingFailure);
    return;
  }

  if (!base::Contains(pair_failure_counts_, device->ble_address))
    pair_failure_counts_[device->ble_address] = 0;

  QP_LOG(INFO) << __func__ << ": " << device;

  for (auto& observer : observers_) {
    observer.OnPairingStart(device);
  }

  DCHECK(adapter_);
  fast_pair_pairers_[device->ble_address] = FastPairPairerImpl::Factory::Create(
      adapter_, device,
      base::BindOnce(&PairerBrokerImpl::OnFastPairHandshakeComplete,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnFastPairDevicePaired,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnFastPairPairingFailure,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnAccountKeyFailure,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnFastPairProcedureComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void PairerBrokerImpl::OnFastPairHandshakeComplete(
    scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
  for (auto& observer : observers_) {
    observer.OnHandshakeComplete(device);
  }
}

void PairerBrokerImpl::OnFastPairDevicePaired(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;

  for (auto& observer : observers_) {
    observer.OnDevicePaired(device);
  }

  RecordPairFailureRetry(
      /*num_retries=*/pair_failure_counts_[device->ble_address]);
  pair_failure_counts_.erase(device->ble_address);
}

void PairerBrokerImpl::OnFastPairPairingFailure(scoped_refptr<Device> device,
                                                PairFailure failure) {
  ++pair_failure_counts_[device->ble_address];
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Failure=" << failure
               << ", Failure Count = "
               << pair_failure_counts_[device->ble_address];

  device::BluetoothDevice* bt_device = nullptr;
  if (device->classic_address()) {
    bt_device = adapter_->GetDevice(device->classic_address().value());
  }

  if (pair_failure_counts_[device->ble_address] == kMaxFailureRetryCount) {
    QP_LOG(INFO) << __func__
                 << ": Reached max failure count. Notifying observers.";
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

  fast_pair_pairers_.erase(device->ble_address);

  if (bt_device && !bt_device->IsPaired()) {
    QP_LOG(INFO)
        << __func__
        << ": Cancelling pairing and scheduling retry for failed pair attempt.";
    bt_device->CancelPairing();

    // Create a timer to wait |kCancelPairingRetryDelay| after cancelling
    // pairing to retry the pairing attempt.
    cancel_pairing_timer_.Start(
        FROM_HERE, kCancelPairingRetryDelay,
        base::BindOnce(&PairerBrokerImpl::PairFastPairDevice,
                       base::Unretained(this), device));

    return;
  }

  PairFastPairDevice(device);
}

void PairerBrokerImpl::OnAccountKeyFailure(scoped_refptr<Device> device,
                                           AccountKeyFailure failure) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Failure=" << failure;

  for (auto& observer : observers_) {
    observer.OnAccountKeyWrite(device, failure);
  }

  EraseHandshakeAndFromPairers(device);
}

void PairerBrokerImpl::OnFastPairProcedureComplete(
    scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;

  for (auto& observer : observers_) {
    observer.OnPairingComplete(device);
  }

  // If we get to this point in the flow for the initial and retroactive pairing
  // scenarios, this means that the account key has successfully been written
  // to these devices.
  if (device->protocol == Protocol::kFastPairInitial ||
      device->protocol == Protocol::kFastPairRetroactive) {
    for (auto& observer : observers_) {
      observer.OnAccountKeyWrite(device, /*error=*/absl::nullopt);
    }
  }

  EraseHandshakeAndFromPairers(device);
}

}  // namespace quick_pair
}  // namespace ash
