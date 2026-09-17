// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/signals_aggregator_decorator.h"
#include "components/enterprise/device_trust/core/signals/decorators/common/signals_decorator.h"
#include "components/enterprise/device_trust/core/signals/signals_filterer.h"
#include "components/enterprise/device_trust/core/signals/signals_service.h"
#include "components/enterprise/device_trust/core/signals/signals_service_impl.h"

namespace enterprise_connectors {

std::unique_ptr<SignalsService> CreateSignalsService(Profile* profile) {
  CHECK(profile);

  // Android has no platform-specific signals decorator: both device and profile
  // signals are collected through the SignalsAggregator.
  auto* signals_aggregator =
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile);
  if (!signals_aggregator) {
    return nullptr;
  }

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;
  decorators.push_back(
      std::make_unique<SignalsAggregatorDecorator>(signals_aggregator));

  return std::make_unique<SignalsServiceImpl>(
      std::move(decorators), std::make_unique<SignalsFilterer>());
}

}  // namespace enterprise_connectors
