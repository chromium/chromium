// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TOUCH_EXPLORATION_MANAGER_H_
#define ASH_ACCESSIBILITY_TOUCH_EXPLORATION_MANAGER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/touch_accessibility_enabler.h"
#include "ash/accessibility/touch_exploration_controller.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace chromeos {
class CrasAudioHandler;
}

namespace ash {
class RootWindowController;

// Responsible for initializing TouchExplorationController when spoken feedback
// is on. Implements TouchExplorationControllerDelegate which allows touch
// gestures to manipulate the system.
//
// TODO(jamescook): Move the TouchExplorationControllerDelegate methods into
// TouchExplorationController. I suspect the delegate was added to support ash
// on Windows, which we don't ship anymore.
class ASH_EXPORT TouchExplorationManager
    : public AccessibilityObserver,
      public aura::WindowObserver,
      public TouchExplorationControllerDelegate,
      public TouchAccessibilityEnablerDelegate,
      public display::DisplayObserver,
      public ::wm::ActivationChangeObserver,
      public KeyboardControllerObserver,
      public ShellObserver {
 public:
  explicit TouchExplorationManager(
      RootWindowController* root_window_controller);
  ~TouchExplorationManager() override;

  // AccessibilityObserver overrides:
  void OnAccessibilityStatusChanged() override;
  void OnAccessibilityControllerShutdown() override;

  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // TouchExplorationControllerDelegate overrides:
  void SetOutputLevel(int volume) override;
  void SilenceSpokenFeedback() override;
  void PlayVolumeAdjustEarcon() override;
  void PlayPassthroughEarcon() override;
  void PlayExitScreenEarcon() override;
  void PlayEnterScreenEarcon() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override;

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // TouchAccessibilityEnablerDelegate overrides:
  void OnTwoFingerTouchStart() override;
  void OnTwoFingerTouchStop() override;
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override;
  void PlayTouchTypeEarcon() override;
  void ToggleSpokenFeedback() override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Update the touch exploration controller so that synthesized touch
  // events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

 private:
  // KeyboardControllerObserver overrides:
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  void UpdateTouchExplorationState();
  bool VolumeAdjustSoundEnabled();

  std::unique_ptr<TouchExplorationController> touch_exploration_controller_;
  std::unique_ptr<TouchAccessibilityEnabler> touch_accessibility_enabler_;
  RootWindowController* root_window_controller_;
  chromeos::CrasAudioHandler* audio_handler_;
  aura::Window* observing_window_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationManager);
};

}  // namespace ash

#endif  // ASH_TOUCH_EXPLORATION_MANAGER_CHROMEOS_H_
