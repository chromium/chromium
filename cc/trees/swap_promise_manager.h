// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SWAP_PROMISE_MANAGER_H_
#define CC_TREES_SWAP_PROMISE_MANAGER_H_

#include <set>
#include <vector>

#include "cc/cc_export.h"
#include "cc/trees/swap_promise.h"

namespace cc {
class SwapPromise;
class SwapPromiseMonitor;

class CC_EXPORT SwapPromiseManager {
 public:
  SwapPromiseManager();
  SwapPromiseManager(const SwapPromiseManager&) = delete;
  ~SwapPromiseManager();

  SwapPromiseManager& operator=(const SwapPromiseManager&) = delete;

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // When a SwapPromiseMonitor is created on the main thread, it calls
  // InsertSwapPromiseMonitor() to register itself with LayerTreeHost.
  // When the monitor is destroyed, it calls RemoveSwapPromiseMonitor()
  // to unregister itself.
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor);
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor);

  // Called when a commit request is made on the LayerTreeHost.
  void NotifySwapPromiseMonitorsOfSetNeedsCommit();

  // Called before the commit of the main thread state will be started.
  void WillCommit();

  // The current swap promise list is moved to the caller.
  std::vector<std::unique_ptr<SwapPromise>> TakeSwapPromises();

  // Breaks the currently queued swap promises with the specified reason.
  void BreakSwapPromises(SwapPromise::DidNotSwapReason reason);

  size_t num_queued_swap_promises() const { return swap_promise_list_.size(); }

 private:
  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list_;
  std::set<SwapPromiseMonitor*> swap_promise_monitors_;
};

}  // namespace cc

#endif  // CC_TREES_SWAP_PROMISE_MANAGER_H_
