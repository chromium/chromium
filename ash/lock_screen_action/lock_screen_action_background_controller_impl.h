// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace ash {

class LockScreenActionBackgroundView;

// The LockScreenActionBackgroundController implementation that shows lock
// screen action background as a non activable window that is shown/hidden with
// an ink drop animation that covers the whole window contents in black from the
// top right window corner.
class ASH_EXPORT LockScreenActionBackgroundControllerImpl
    : public LockScreenActionBackgroundController,
      public views::WidgetObserver {
 public:
  LockScreenActionBackgroundControllerImpl();
  ~LockScreenActionBackgroundControllerImpl() override;

  // LockScreenActionBackgroundController:
  bool IsBackgroundWindow(aura::Window* window) const override;
  bool ShowBackground() override;
  bool HideBackgroundImmediately() override;
  bool HideBackground() override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  friend class LockScreenActionBackgroundControllerImplTestApi;

  // Creates the widget to be used as the background widget.
  views::Widget* CreateWidget();

  // Called when the background widget view is fully shown, i.e. when the ink
  // drop animation associated with showing the widget ends.
  void OnBackgroundShown();

  // Called when the background widget view is fully hidden, i.e. when the ink
  // drop animation associated with hiding the widget ends.
  void OnBackgroundHidden();

  // The background widget used (and created) by the controller to display the
  // lock screen action background.
  // The widget is owned by its native (aura) window - it will be deleted when
  // the window gets closed/destroyed.
  views::Widget* background_widget_ = nullptr;
  LockScreenActionBackgroundView* contents_view_ = nullptr;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};

  base::WeakPtrFactory<LockScreenActionBackgroundControllerImpl>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LockScreenActionBackgroundControllerImpl);
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_IMPL_H_
