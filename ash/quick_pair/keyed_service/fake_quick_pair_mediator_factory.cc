// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/fake_quick_pair_mediator_factory.h"

#include "ash/quick_pair/companion_app/companion_app_broker_impl.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"
#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"
#include "ash/quick_pair/pairing/pairer_broker_impl.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/scanning/scanner_broker_impl.h"
#include "ash/quick_pair/ui/ui_broker_impl.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"

namespace ash::quick_pair {

std::unique_ptr<Mediator> FakeQuickPairMediatorFactory::BuildInstance() {
  auto process_manager = std::make_unique<QuickPairProcessManagerImpl>();
  auto pairer_broker = std::make_unique<PairerBrokerImpl>();
  auto message_stream_lookup = std::make_unique<MessageStreamLookupImpl>();

  return std::make_unique<Mediator>(
      std::make_unique<FeatureStatusTrackerImpl>(),
      std::make_unique<ScannerBrokerImpl>(process_manager.get()),
      std::make_unique<RetroactivePairingDetectorImpl>(
          pairer_broker.get(), message_stream_lookup.get()),
      std::move(message_stream_lookup), std::move(pairer_broker),
      std::make_unique<UIBrokerImpl>(),
      std::make_unique<CompanionAppBrokerImpl>(),
      std::make_unique<FakeFastPairRepository>(), std::move(process_manager));
}

}  // namespace ash::quick_pair
