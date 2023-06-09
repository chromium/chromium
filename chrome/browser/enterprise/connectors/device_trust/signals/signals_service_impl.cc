// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_filterer.h"

namespace enterprise_connectors {

namespace {

constexpr char kLatencyHistogramVariant[] = "Full";

}  // namespace

SignalsServiceImpl::SignalsServiceImpl(
    std::vector<std::unique_ptr<SignalsDecorator>> signals_decorators,
    std::unique_ptr<SignalsFilterer> signals_filterer)
    : signals_decorators_(std::move(signals_decorators)),
      signals_filterer_(std::move(signals_filterer)) {
  CHECK(signals_filterer_);
}

SignalsServiceImpl::~SignalsServiceImpl() = default;

void SignalsServiceImpl::CollectSignals(CollectSignalsCallback callback) {
  auto start_time = base::TimeTicks::Now();
  auto signals = std::make_unique<base::Value::Dict>();
  auto* signals_ptr = signals.get();

  auto barrier_closure = base::BarrierClosure(
      signals_decorators_.size(),
      base::BindOnce(&SignalsServiceImpl::OnSignalsDecorated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     start_time, std::move(signals)));

  for (const auto& decorator : signals_decorators_) {
    decorator->Decorate(*signals_ptr, barrier_closure);
  }
}

void SignalsServiceImpl::OnSignalsDecorated(
    CollectSignalsCallback callback,
    base::TimeTicks start_time,
    std::unique_ptr<base::Value::Dict> signals) {
  LogSignalsCollectionLatency(kLatencyHistogramVariant, start_time);

  if (!signals) {
    base::Value::Dict empty_dictionary;
    std::move(callback).Run(std::move(empty_dictionary));
  } else {
    signals_filterer_->Filter(*signals);
    std::move(callback).Run(std::move(*signals));
  }
}

}  // namespace enterprise_connectors
