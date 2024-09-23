// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CHROMEVOX_TOUCH_EXPLORATION_MANAGER_H_
#define ASH_ACCESSIBILITY_CHROMEVOX_TOUCH_EXPLORATION_MANAGER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/chromevox/touch_accessibility_enabler.h"
#include "ash/accessibility/chromevox/touch_exploration_controller.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class CrasAudioHandler;
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

  TouchExplorationManager(const TouchExplorationManager&) = delete;
  TouchExplorationManager& operator=(const TouchExplorationManager&) = delete;

  ~TouchExplorationManager() override;

  // AccessibilityObserver overrides:
  void OnAccessibilityStatusChanged() override;
  void OnAccessibilityControllerShutdown() override;

  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // TouchExplorationControllerDelegate overrides:
  void SetOutputLevel(int volume) override;
  void SilenceSpokenFeedback() override;
  void PlayVolumeAdjustEarcon() override;
  void PlayPassthroughEarcon() override;
  void PlayLongPressRightClickEarcon() override;
  void PlayEnterScreenEarcon() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location) override;

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // TouchAccessibilityEnablerDelegate overrides:
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
  raw_ptr<RootWindowController> root_window_controller_;
  raw_ptr<CrasAudioHandler> audio_handler_;
  raw_ptr<aura::Window> observing_window_;
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CHROMEVOX_TOUCH_EXPLORATION_MANAGER_H_
