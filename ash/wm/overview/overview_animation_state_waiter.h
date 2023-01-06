// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/functional/callback.h"

namespace ash {

// Wait until an overview animation completes. This self destruct
// after executing the callback. Used by testing APIs.
class ASH_EXPORT OverviewAnimationStateWaiter : public OverviewObserver {
 public:
  // Type of the callback. It receives true when the overview animation finishes
  // properly.
  using DoneCallback = base::OnceCallback<void(bool)>;

  OverviewAnimationStateWaiter(OverviewAnimationState expected_state,
                               DoneCallback callback);
  OverviewAnimationStateWaiter(const OverviewAnimationStateWaiter&) = delete;
  OverviewAnimationStateWaiter& operator=(const OverviewAnimationStateWaiter&) =
      delete;
  ~OverviewAnimationStateWaiter() override;

  // Cancels the ongoing observation of the overview animation and invokes
  // |callback_| with false. This also destructs itself.
  void Cancel();

 private:
  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  OverviewAnimationState expected_state_;
  DoneCallback callback_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_
