// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/callback.h"
#include "base/macros.h"

namespace ash {

// Wait until an overview animation completes. This self destruct
// after executing the callback. Used by testing APIs.
class ASH_EXPORT OverviewAnimationStateWaiter : public OverviewObserver {
 public:
  // Type of the callback. It receives true when the overview animation finishes
  // properly.
  typedef base::OnceCallback<void(bool)> DoneCallback;

  OverviewAnimationStateWaiter(OverviewAnimationState expected_state,
                               DoneCallback callback);
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

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationStateWaiter);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_STATE_WAITER_H_
