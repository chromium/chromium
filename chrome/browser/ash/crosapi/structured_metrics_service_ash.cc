// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/structured_metrics_service_ash.h"

#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

StructuredMetricsServiceAsh::StructuredMetricsServiceAsh() = default;
StructuredMetricsServiceAsh::~StructuredMetricsServiceAsh() = default;

void StructuredMetricsServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::StructuredMetricsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void StructuredMetricsServiceAsh::Record(
    const std::vector<::metrics::structured::Event>& events) {
  for (const auto& event : events) {
    auto event_base = metrics::structured::EventBase::FromEvent(event);
    // Only record the event if it is valid.
    if (event_base.has_value())
      event_base.value().Record();
  }
}

}  // namespace crosapi
