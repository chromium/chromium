// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SCOPED_ABORT_REMAINING_SWAP_PROMISES_H_
#define CC_TREES_SCOPED_ABORT_REMAINING_SWAP_PROMISES_H_

#include "base/memory/raw_ptr.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/swap_promise_manager.h"

namespace cc {

class ScopedAbortRemainingSwapPromises {
 public:
  explicit ScopedAbortRemainingSwapPromises(
      SwapPromiseManager* swap_promise_manager)
      : swap_promise_manager_(swap_promise_manager) {}
  ScopedAbortRemainingSwapPromises(const ScopedAbortRemainingSwapPromises&) =
      delete;

  ~ScopedAbortRemainingSwapPromises() {
    swap_promise_manager_->BreakSwapPromises(SwapPromise::COMMIT_FAILS);
  }

  ScopedAbortRemainingSwapPromises& operator=(
      const ScopedAbortRemainingSwapPromises&) = delete;

 private:
  raw_ptr<SwapPromiseManager> swap_promise_manager_;
};

}  // namespace cc

#endif  // CC_TREES_SCOPED_ABORT_REMAINING_SWAP_PROMISES_H_
