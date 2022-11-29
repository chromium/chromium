// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/win/win_signals_decorator.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {
constexpr char kLatencyHistogramVariant[] = "Win";
}  // namespace

WinSignalsDecorator::WinSignalsDecorator() = default;

WinSignalsDecorator::~WinSignalsDecorator() = default;

void WinSignalsDecorator::Decorate(base::Value::Dict& signals,
                                   base::OnceClosure done_closure) {
  auto start_time = base::TimeTicks::Now();

  auto crowdstrike_signals = device_signals::GetCrowdStrikeSignals();
  if (crowdstrike_signals.has_value()) {
    auto serialized_crowdstrike_signals = crowdstrike_signals->ToValue();
    if (serialized_crowdstrike_signals) {
      signals.Set(device_signals::names::kCrowdStrike,
                  std::move(serialized_crowdstrike_signals.value()));
    }
  }

  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
