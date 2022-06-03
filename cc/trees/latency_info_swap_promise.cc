// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/latency_info_swap_promise.h"

#include <stdint.h>

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "services/tracing/public/cpp/perfetto/macros.h"

namespace cc {

LatencyInfoSwapPromise::LatencyInfoSwapPromise(const ui::LatencyInfo& latency)
    : latency_(latency) {}

LatencyInfoSwapPromise::~LatencyInfoSwapPromise() = default;

void LatencyInfoSwapPromise::WillSwap(viz::CompositorFrameMetadata* metadata) {
  DCHECK(!latency_.terminated());
  metadata->latency_info.push_back(latency_);
}

void LatencyInfoSwapPromise::DidSwap() {}

SwapPromise::DidNotSwapAction LatencyInfoSwapPromise::DidNotSwap(
    DidNotSwapReason reason) {
  latency_.Terminate();
  // TODO(miletus): Turn this back on once per-event LatencyInfo tracking
  // is enabled in GPU side.
  // DCHECK(latency_.terminated);
  return DidNotSwapAction::BREAK_PROMISE;
}

int64_t LatencyInfoSwapPromise::TraceId() const {
  return latency_.trace_id();
}

// Trace the original LatencyInfo of a LatencyInfoSwapPromise
void LatencyInfoSwapPromise::OnCommit() {
  using perfetto::protos::pbzero::ChromeLatencyInfo;
  using perfetto::protos::pbzero::TrackEvent;

  TRACE_EVENT("input,benchmark", "LatencyInfo.Flow",
              [this](perfetto::EventContext ctx) {
                ChromeLatencyInfo* latency_info =
                    ctx.event()->set_chrome_latency_info();
                latency_info->set_trace_id(TraceId());
                latency_info->set_step(
                    ChromeLatencyInfo::STEP_HANDLE_INPUT_EVENT_MAIN_COMMIT);
                tracing::FillFlowEvent(ctx, TrackEvent::LegacyEvent::FLOW_INOUT,
                                       TraceId());
              });
}

}  // namespace cc
