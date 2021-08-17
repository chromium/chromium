// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"

namespace enterprise_connectors {

SignalsServiceImpl::SignalsServiceImpl(
    std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators)
    : signals_decorators_(std::move(signals_decorators)) {}

SignalsServiceImpl::~SignalsServiceImpl() = default;

DeviceTrustSignals SignalsServiceImpl::CollectSignals() {
  DeviceTrustSignals signals;

  for (const auto& decorator : signals_decorators_) {
    decorator->Decorate(signals);
  }

  return signals;
}

}  // namespace enterprise_connectors
