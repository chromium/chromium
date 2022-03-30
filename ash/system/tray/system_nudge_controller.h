// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {

class SystemNudge;

// An abstract class which displays a contextual system nudge and fades it in
// and out after a short period of time. While nudge behavior is covered by this
// abstract class, Subclasses must implement CreateSystemNudge() in order to
// create a custom label and icon for their nudge's UI.
// TODO(crbug.com/1232525): Duration and positioning should be configurable.
class ASH_EXPORT SystemNudgeController {
 public:
  SystemNudgeController();
  SystemNudgeController(const SystemNudgeController&) = delete;
  SystemNudgeController& operator=(const SystemNudgeController&) = delete;
  virtual ~SystemNudgeController();

  // Shows the nudge widget.
  void ShowNudge();

  // Ensure the destruction of a nudge that is animating.
  void ForceCloseAnimatingNudge();

  // Test method for triggering the nudge timer to hide.
  void FireHideNudgeTimerForTesting();

  // Get the system nudge for testing purpose.
  SystemNudge* GetSystemNudgeForTesting() { return nudge_.get(); }

 protected:
  // Concrete subclasses must implement this method to return a
  // SystemNudge that creates a label and specifies an icon specific
  // to the nudge.
  virtual std::unique_ptr<SystemNudge> CreateSystemNudge() = 0;

  // Hides the nudge widget.
  void HideNudge();

 private:
  // Begins the animation for fading in or fading out the nudge.
  void StartFadeAnimation(bool show);

  // Contextual nudge which shows a view.
  std::unique_ptr<SystemNudge> nudge_;

  // Timer to hide the nudge.
  base::OneShotTimer hide_nudge_timer_;

  std::unique_ptr<ui::ImplicitAnimationObserver> hide_nudge_animation_observer_;

  base::WeakPtrFactory<SystemNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_
