// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
#define CC_TEST_MOCK_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_

#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/latency/latency_info.h"

namespace cc {

class MockLatencyInfoSwapPromiseMonitor : public LatencyInfoSwapPromiseMonitor {
 public:
  explicit MockLatencyInfoSwapPromiseMonitor(
      SwapPromiseManager* swap_promise_manager);
  explicit MockLatencyInfoSwapPromiseMonitor(LayerTreeHostImpl* host_impl);
  ~MockLatencyInfoSwapPromiseMonitor() override;

  MOCK_METHOD(void, OnSetNeedsCommitOnMain, (), (override));
  MOCK_METHOD(void, OnSetNeedsRedrawOnImpl, (), (override));

 private:
  ui::LatencyInfo latency_info_;
};

}  // namespace cc

#endif  // CC_TEST_MOCK_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
