// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/quick_pair/ui/ui_broker_impl.h"

namespace ash {
namespace quick_pair {

namespace {

Mediator::Factory* g_test_factory = nullptr;

}

// static
std::unique_ptr<Mediator> Mediator::Factory::Create() {
  if (g_test_factory)
    return g_test_factory->BuildInstance();

  return std::make_unique<Mediator>(
      std::make_unique<FeatureStatusTrackerImpl>(),
      std::make_unique<ScannerBrokerImpl>(), std::make_unique<UIBrokerImpl>(),
      std::make_unique<FastPairRepository>());
}

// static
void Mediator::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

Mediator::Mediator(std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
                   std::unique_ptr<ScannerBroker> scanner_broker,
                   std::unique_ptr<UIBroker> ui_broker,
                   std::unique_ptr<FastPairRepository> fast_pair_repository)
    : feature_status_tracker_(std::move(feature_status_tracker)),
      scanner_broker_(std::move(scanner_broker)),
      ui_broker_(std::move(ui_broker)),
      fast_pair_repository_(std::move(fast_pair_repository)) {
  feature_status_tracker_observation_.Observe(feature_status_tracker_.get());
  scanner_broker_observation_.Observe(scanner_broker_.get());
  ui_broker_observation_.Observe(ui_broker_.get());

  SetFastPairState(feature_status_tracker_->IsFastPairEnabled());
}

Mediator::~Mediator() = default;

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  SetFastPairState(is_enabled);
}

void Mediator::OnDeviceFound(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
  ui_broker_->ShowDiscovery(device);
}

void Mediator::OnDeviceLost(scoped_refptr<Device> device) {
  QP_LOG(INFO) << __func__ << ": " << device;
}

void Mediator::SetFastPairState(bool is_enabled) {
  QP_LOG(VERBOSE) << __func__ << ": " << is_enabled;

  if (is_enabled)
    scanner_broker_->StartScanning(Protocol::kFastPair);
  else
    scanner_broker_->StopScanning(Protocol::kFastPair);
}

void Mediator::OnDiscoveryAction(scoped_refptr<Device> device,
                                 DiscoveryAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;

  switch (action) {
    case DiscoveryAction::kPairToDevice:
      ui_broker_->ShowPairing(std::move(device));
      break;
    case DiscoveryAction::kDismissedByUser:
    case DiscoveryAction::kDismissed:
      break;
  }
}

void Mediator::OnPairingFailureAction(scoped_refptr<Device> device,
                                      PairingFailedAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;
}

void Mediator::OnCompanionAppAction(scoped_refptr<Device> device,
                                    CompanionAppAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;
}

void Mediator::OnAssociateAccountAction(scoped_refptr<Device> device,
                                        AssociateAccountAction action) {
  QP_LOG(INFO) << __func__ << ": Device=" << device << ", Action=" << action;
}

}  // namespace quick_pair
}  // namespace ash
