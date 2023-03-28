// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/notifier_catalogs.h"
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

  // Records Nudge "TimeToAction" metric, which tracks the time from when a
  // nudge was shown to when the nudge's suggested action was performed.
  // The metric is not recorded if the nudge hasn't been shown before.
  static void RecordNudgeAction(NudgeCatalogName catalog_name);

  // Shows the nudge widget.
  void ShowNudge();

  // Ensure the destruction of a nudge that is animating.
  void ForceCloseAnimatingNudge();

  // Test method for triggering the nudge timer to hide.
  void FireHideNudgeTimerForTesting();

  // Get the system nudge for testing purpose.
  SystemNudge* GetSystemNudgeForTesting() { return nudge_.get(); }

  // Resets the `nudge_registry` object that records the time a nudge was last
  // shown.
  void ResetNudgeRegistryForTesting();

  // Hides the nudge widget.
  void HideNudge();

 protected:
  // Concrete subclasses must implement this method to return a
  // SystemNudge that creates a label and specifies an icon specific
  // to the nudge.
  virtual std::unique_ptr<SystemNudge> CreateSystemNudge() = 0;

 private:
  // Returns the registry which keeps track of when a nudge was last shown.
  static std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>&
  GetNudgeRegistry();

  // Begins the animation for fading in or fading out the nudge.
  void StartFadeAnimation(bool show);

  // Records the time a nudge was last shown and stores it in the
  // `nudge_registry`.
  void RecordNudgeShown(NudgeCatalogName catalog_name);

  // Contextual nudge which shows a view.
  std::unique_ptr<SystemNudge> nudge_;

  // Timer to hide the nudge.
  std::unique_ptr<base::OneShotTimer> hide_nudge_timer_;

  std::unique_ptr<ui::ImplicitAnimationObserver> hide_nudge_animation_observer_;

  base::WeakPtrFactory<SystemNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_NUDGE_CONTROLLER_H_
