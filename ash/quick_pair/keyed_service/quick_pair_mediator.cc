// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"

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
      std::make_unique<ScannerBrokerImpl>());
}

// static
void Mediator::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory = factory;
}

Mediator::Mediator(std::unique_ptr<FeatureStatusTracker> feature_status_tracker,
                   std::unique_ptr<ScannerBroker> scanner_broker)
    : feature_status_tracker_(std::move(feature_status_tracker)),
      scanner_broker_(std::move(scanner_broker)) {
  feature_status_tracker_observation_.Observe(feature_status_tracker_.get());
  scanner_broker_observation_.Observe(scanner_broker_.get());

  SetFastPairState(feature_status_tracker_->IsFastPairEnabled());
}

Mediator::~Mediator() = default;

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  SetFastPairState(is_enabled);
}

void Mediator::OnDeviceFound(const Device& device) {
  QP_LOG(INFO) << __func__ << ": " << device;
}

void Mediator::OnDeviceLost(const Device& device) {
  QP_LOG(INFO) << __func__ << ": " << device;
}

void Mediator::SetFastPairState(bool is_enabled) {
  QP_LOG(INFO) << __func__ << ": " << is_enabled;

  if (is_enabled)
    scanner_broker_->StartScanning(Protocol::kFastPair);
  else
    scanner_broker_->StopScanning(Protocol::kFastPair);
}

}  // namespace quick_pair
}  // namespace ash
