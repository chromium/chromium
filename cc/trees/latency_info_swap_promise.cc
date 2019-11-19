// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/latency_info_swap_promise.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"

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
  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(TraceId()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventMainCommit");
}

}  // namespace cc
