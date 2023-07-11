// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

namespace ash::quick_start {

// static
std::unique_ptr<TargetDeviceConnectionBroker>
TargetDeviceConnectionBrokerFactory::Create(
    SessionContext session_context,
    base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder) {
  if (test_factory_) {
    return test_factory_->CreateInstance(nearby_connections_manager,
                                         std::move(quick_start_decoder));
  }

  auto connection_factory = std::make_unique<Connection::Factory>();
  return std::make_unique<TargetDeviceConnectionBrokerImpl>(
      session_context, nearby_connections_manager,
      std::move(connection_factory), std::move(quick_start_decoder));
}

// static
void TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(
    TargetDeviceConnectionBrokerFactory* test_factory) {
  test_factory_ = test_factory;
}

// static
TargetDeviceConnectionBrokerFactory*
    TargetDeviceConnectionBrokerFactory::test_factory_ = nullptr;

TargetDeviceConnectionBrokerFactory::TargetDeviceConnectionBrokerFactory() =
    default;

TargetDeviceConnectionBrokerFactory::~TargetDeviceConnectionBrokerFactory() =
    default;

}  // namespace ash::quick_start
