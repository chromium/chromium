// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/swap_promise_manager.h"

#include <utility>

#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/swap_promise.h"

namespace cc {

SwapPromiseManager::SwapPromiseManager() = default;

SwapPromiseManager::~SwapPromiseManager() {
  DCHECK(latency_info_swap_promise_monitors_.empty());
  BreakSwapPromises(SwapPromise::COMMIT_FAILS);
}

void SwapPromiseManager::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void SwapPromiseManager::InsertLatencyInfoSwapPromiseMonitor(
    LatencyInfoSwapPromiseMonitor* monitor) {
  latency_info_swap_promise_monitors_.insert(monitor);
}

void SwapPromiseManager::RemoveLatencyInfoSwapPromiseMonitor(
    LatencyInfoSwapPromiseMonitor* monitor) {
  latency_info_swap_promise_monitors_.erase(monitor);
}

void SwapPromiseManager::NotifyLatencyInfoSwapPromiseMonitors() {
  for (LatencyInfoSwapPromiseMonitor* monitor :
       latency_info_swap_promise_monitors_) {
    monitor->OnSetNeedsCommitOnMain();
  }
}

void SwapPromiseManager::WillCommit() {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->OnCommit();
}

std::vector<std::unique_ptr<SwapPromise>>
SwapPromiseManager::TakeSwapPromises() {
  std::vector<std::unique_ptr<SwapPromise>> result;
  result.swap(swap_promise_list_);
  return result;
}

void SwapPromiseManager::BreakSwapPromises(
    SwapPromise::DidNotSwapReason reason) {
  std::vector<std::unique_ptr<SwapPromise>> keep_active_swap_promises;
  keep_active_swap_promises.reserve(swap_promise_list_.size());

  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (auto& swap_promise : swap_promise_list_) {
    if (swap_promise->DidNotSwap(reason, timestamp) ==
        SwapPromise::DidNotSwapAction::KEEP_ACTIVE) {
      keep_active_swap_promises.push_back(std::move(swap_promise));
    }
  }
  swap_promise_list_.swap(keep_active_swap_promises);
}

}  // namespace cc
