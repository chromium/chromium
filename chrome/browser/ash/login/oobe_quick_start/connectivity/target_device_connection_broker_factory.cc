// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

namespace ash::quick_start {

// static
std::unique_ptr<TargetDeviceConnectionBroker>
TargetDeviceConnectionBrokerFactory::Create() {
  return Create(RandomSessionId());
}

// static
std::unique_ptr<TargetDeviceConnectionBroker>
TargetDeviceConnectionBrokerFactory::Create(RandomSessionId session_id) {
  if (test_factory_) {
    return test_factory_->CreateInstance(session_id);
  }

  return std::make_unique<TargetDeviceConnectionBrokerImpl>(session_id);
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
