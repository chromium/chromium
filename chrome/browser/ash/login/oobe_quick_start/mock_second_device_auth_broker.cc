// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/mock_second_device_auth_broker.h"

#include <memory>

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::quick_start {
namespace {

constexpr char kDeviceId[] = "fake-device-id";

}  // namespace

MockSecondDeviceAuthBroker::MockSecondDeviceAuthBroker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : SecondDeviceAuthBroker(
          kDeviceId,
          url_loader_factory,
          std::make_unique<attestation::MockAttestationFlow>()) {}

MockSecondDeviceAuthBroker::~MockSecondDeviceAuthBroker() = default;

}  // namespace ash::quick_start
