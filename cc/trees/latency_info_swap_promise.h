// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LATENCY_INFO_SWAP_PROMISE_H_
#define CC_TREES_LATENCY_INFO_SWAP_PROMISE_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "cc/trees/swap_promise.h"
#include "ui/latency/latency_info.h"

namespace cc {

// Associates a |LatencyInfo| with a compositor frame's metadata before that
// frame is sent to the downstream display compositor. The |LatencyInfo|
// instances used here track latencies of pieces of the input handling
// pipeline. Compositor frame consumers can combine their internal latency
// measurements with these measurements to generate end-to-end reports of input
// latency.
class CC_EXPORT LatencyInfoSwapPromise : public SwapPromise {
 public:
  explicit LatencyInfoSwapPromise(const ui::LatencyInfo& latency_info);
  ~LatencyInfoSwapPromise() override;

  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override;
  void DidSwap() override;
  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override;
  void OnCommit() override;

  int64_t TraceId() const override;

 private:
  ui::LatencyInfo latency_;
};

}  // namespace cc

#endif  // CC_TREES_LATENCY_INFO_SWAP_PROMISE_H_
