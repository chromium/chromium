// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <memory>
#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace enterprise_connectors {

SignalsServiceImpl::SignalsServiceImpl(
    std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators)
    : signals_decorators_(std::move(signals_decorators)) {}

SignalsServiceImpl::~SignalsServiceImpl() = default;

std::unique_ptr<DeviceTrustSignals> SignalsServiceImpl::CollectSignals() {
  auto signals = std::make_unique<DeviceTrustSignals>();

  for (const auto& decorator : signals_decorators_) {
    decorator->Decorate(*signals);
  }

  return signals;
}

}  // namespace enterprise_connectors
