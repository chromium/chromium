// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_TEST_API_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_view.h"
#include "base/memory/raw_ptr.h"

namespace views {
class View;
}

namespace ash {

class LockScreenActionBackgroundView;

// Class that provides access to LockScreenActionBackgroundView implementation
// details in tests.
class ASH_EXPORT LockScreenActionBackgroundViewTestApi {
 public:
  explicit LockScreenActionBackgroundViewTestApi(
      LockScreenActionBackgroundView* action_background_view)
      : action_background_view_(action_background_view) {}

  LockScreenActionBackgroundViewTestApi(
      const LockScreenActionBackgroundViewTestApi&) = delete;
  LockScreenActionBackgroundViewTestApi& operator=(
      const LockScreenActionBackgroundViewTestApi&) = delete;

  ~LockScreenActionBackgroundViewTestApi() = default;

  views::View* GetBackground() {
    return action_background_view_->GetBackgroundView();
  }

 private:
  raw_ptr<LockScreenActionBackgroundView> action_background_view_;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_TEST_API_H_
