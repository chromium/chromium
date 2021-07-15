// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/pairer_broker_impl.h"

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"

namespace ash {
namespace quick_pair {

PairerBrokerImpl::PairerBrokerImpl() = default;

PairerBrokerImpl::~PairerBrokerImpl() = default;

void PairerBrokerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PairerBrokerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PairerBrokerImpl::PairDevice(const Device& device) {
  switch (device.protocol) {
    case Protocol::kFastPair:
      PairFastPairDevice(device);
      break;
  }
}

void PairerBrokerImpl::PairFastPairDevice(const Device& device) {
  if (base::Contains(fast_pair_pairers_, device.address)) {
    QP_LOG(WARNING) << __func__ << ": Already pairing device" << device;
    return;
  }

  QP_LOG(INFO) << __func__ << ": " << device;

  fast_pair_pairers_[device.address] = std::make_unique<FastPairPairer>(
      device,
      base::BindOnce(&PairerBrokerImpl::OnFastPairDevicePaired,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnFastPairPairingFailure,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnAccountKeyFailure,
                     weak_pointer_factory_.GetWeakPtr()),
      base::BindOnce(&PairerBrokerImpl::OnFastPairProcedureComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void PairerBrokerImpl::OnFastPairDevicePaired(const Device& device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
}

void PairerBrokerImpl::OnFastPairPairingFailure(const Device& device,
                                                PairFailure failure) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Failure=" << failure;
}

void PairerBrokerImpl::OnAccountKeyFailure(const Device& device,
                                           AccountKeyFailure failure) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Failure=" << failure;
}

void PairerBrokerImpl::OnFastPairProcedureComplete(const Device& device) {
  QP_LOG(INFO) << __func__ << ": Device=" << device;
  fast_pair_pairers_.erase(device.address);
}

}  // namespace quick_pair
}  // namespace ash
