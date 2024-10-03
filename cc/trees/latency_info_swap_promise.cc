// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/latency_info_swap_promise.h"

#include <stdint.h>

#include "base/check.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "ui/latency/latency_info.h"

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
    DidNotSwapReason reason,
    base::TimeTicks ts) {
  latency_.Terminate();
  // TODO(miletus): Turn this back on once per-event LatencyInfo tracking
  // is enabled in GPU side.
  // DCHECK(latency_.terminated);
  return DidNotSwapAction::BREAK_PROMISE;
}

int64_t LatencyInfoSwapPromise::GetTraceId() const {
  return latency_.trace_id();
}

// Trace the original LatencyInfo of a LatencyInfoSwapPromise
void LatencyInfoSwapPromise::OnCommit() {
  using perfetto::protos::pbzero::TrackEvent;

  int64_t trace_id = GetTraceId();
  TRACE_EVENT("input,benchmark,latencyInfo", "LatencyInfo.Flow",
              [&](perfetto::EventContext ctx) {
                ui::LatencyInfo::FillTraceEvent(
                    ctx, trace_id,
                    perfetto::protos::pbzero::ChromeLatencyInfo2::Step::
                        STEP_HANDLE_INPUT_EVENT_MAIN_COMMIT);
              });
}

}  // namespace cc
