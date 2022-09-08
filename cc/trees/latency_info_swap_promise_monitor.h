// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
#define CC_TREES_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "cc/cc_export.h"

namespace ui {
class LatencyInfo;
}

namespace cc {

class LayerTreeHostImpl;
class SwapPromiseManager;

// A `LatencyInfoSwapPromiseMonitor` is used to monitor compositor state change
// that should be associated with a `LatencyInfoSwapPromise`, e.g.
// `SetNeedsCommit()` is called on the main thread or `SetNeedsRedraw()` is
// called on the compositor thread.
//
// Creating a `LatencyInfoSwapPromiseMonitor` will insert it into a
// `SwapPromiseManager` or `LayerTreeHostImpl`, depending on the constructor
// used.
//
// Notification of compositor state change will be sent through
// `OnSetNeedsCommitOnMain()` or `OnSetNeedsRedrawOnImpl()`. Note that multiple
// notifications of the same type to the same monitor will only queue one
// `LatencyInfoSwapPromise`.
//
// When `LatencyInfoSwapPromiseMonitor` is destroyed, it will unregister itself
// from `SwapPromiseManager` or `LayerTreeHostImpl`.
class CC_EXPORT LatencyInfoSwapPromiseMonitor {
 public:
  // Constructor for when the monitor lives on the main thread.
  LatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency,
                                SwapPromiseManager* swap_promise_manager);

  // Constructor for when the monitor lives on the compositor thread.
  LatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency,
                                LayerTreeHostImpl* host_impl);

  LatencyInfoSwapPromiseMonitor(const LatencyInfoSwapPromiseMonitor&) = delete;
  LatencyInfoSwapPromiseMonitor& operator=(
      const LatencyInfoSwapPromiseMonitor&) = delete;

  virtual ~LatencyInfoSwapPromiseMonitor();

  // Virtual so that tests can mock them.
  virtual void OnSetNeedsCommitOnMain();
  virtual void OnSetNeedsRedrawOnImpl();

 private:
  const raw_ptr<ui::LatencyInfo> latency_;
  const raw_ptr<SwapPromiseManager> swap_promise_manager_ = nullptr;
  const raw_ptr<LayerTreeHostImpl> host_impl_ = nullptr;

  SEQUENCE_CHECKER(main_sequence_checker_);
  SEQUENCE_CHECKER(impl_sequence_checker_);
};

}  // namespace cc

#endif  // CC_TREES_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
