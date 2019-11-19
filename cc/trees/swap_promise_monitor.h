// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SWAP_PROMISE_MONITOR_H_
#define CC_TREES_SWAP_PROMISE_MONITOR_H_

#include "cc/cc_export.h"

namespace cc {

class SwapPromiseManager;
class LayerTreeHostImpl;

// A SwapPromiseMonitor is used to monitor compositor state change that
// should be associated with a SwapPromise, e.g. SetNeedsCommit() is
// called on main thread or SetNeedsRedraw() is called on impl thread.
// Creating a SwapPromiseMonitor will insert itself into a SwapPromiseManager
// or LayerTreeHostImpl. You must provide a pointer to the appropriate
// structure to the monitor (and only one of the two). Notification of
// compositor state change will be sent through OnSetNeedsCommitOnMain()
// or OnSetNeedsRedrawOnImpl(). When SwapPromiseMonitor is destroyed, it
// will unregister itself from SwapPromiseManager or LayerTreeHostImpl.
class CC_EXPORT SwapPromiseMonitor {
 public:
  // If the monitor lives on the main thread, pass in |swap_promise_manager|
  // tied to the LayerTreeHost and set |host_impl| to nullptr. If the monitor
  // lives on the impl thread, pass in |host_impl| and set |layer_tree_host| to
  // nullptr.
  SwapPromiseMonitor(SwapPromiseManager* swap_promise_managaer,
                     LayerTreeHostImpl* host_impl);
  virtual ~SwapPromiseMonitor();

  virtual void OnSetNeedsCommitOnMain() = 0;
  virtual void OnSetNeedsRedrawOnImpl() = 0;

 protected:
  SwapPromiseManager* swap_promise_manager_;
  LayerTreeHostImpl* host_impl_;
};

}  // namespace cc

#endif  // CC_TREES_SWAP_PROMISE_MONITOR_H_
