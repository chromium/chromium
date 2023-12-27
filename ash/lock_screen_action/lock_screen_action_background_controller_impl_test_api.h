// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_TEST_API_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller_impl.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Widget;
}

namespace ash {

class LockScreenActionBackgroundControllerImpl;
class LockScreenActionBackgroundView;

// Class that provides access to LockScreenActionBackgroundControllerImpl
// implementation details in tests.
class ASH_EXPORT LockScreenActionBackgroundControllerImplTestApi {
 public:
  explicit LockScreenActionBackgroundControllerImplTestApi(
      LockScreenActionBackgroundControllerImpl* controller)
      : controller_(controller) {}

  LockScreenActionBackgroundControllerImplTestApi(
      const LockScreenActionBackgroundControllerImplTestApi&) = delete;
  LockScreenActionBackgroundControllerImplTestApi& operator=(
      const LockScreenActionBackgroundControllerImplTestApi&) = delete;

  ~LockScreenActionBackgroundControllerImplTestApi() = default;

  views::Widget* GetWidget() { return controller_->background_widget_; }

  LockScreenActionBackgroundView* GetContentsView() {
    return controller_->contents_view_;
  }

 private:
  raw_ptr<LockScreenActionBackgroundControllerImpl> controller_;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_TEST_API_H_
