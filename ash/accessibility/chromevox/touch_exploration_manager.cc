// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/touch_exploration_manager.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/chromevox/touch_exploration_controller.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

AccessibilityController* GetA11yController() {
  return Shell::Get()->accessibility_controller();
}

}  // namespace

TouchExplorationManager::TouchExplorationManager(
    RootWindowController* root_window_controller)
    : root_window_controller_(root_window_controller),
      audio_handler_(CrasAudioHandler::Get()),
      observing_window_(nullptr) {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  UpdateTouchExplorationState();
}

TouchExplorationManager::~TouchExplorationManager() {
  // TODO(jamescook): Clean up shutdown order so this check isn't needed. See
  // also the TODO in |OnAccessibilityControllerShutdown|.
  if (Shell::Get()->accessibility_controller())
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  if (observing_window_)
    observing_window_->RemoveObserver(this);
}

void TouchExplorationManager::OnAccessibilityStatusChanged() {
  UpdateTouchExplorationState();
}

void TouchExplorationManager::OnAccessibilityControllerShutdown() {
  // This code helps with shutdown, but it does not obviate the need for similar
  // code in |TouchExplorationManager::~TouchExplorationManager|. That is
  // because there is a |TouchExplorationManager| per display, but only one
  // |AccessibilityController|. If you disconnect an external display, then the
  // corresponding |TouchExplorationManager| will be destroyed, but
  // |OnAccessibilityControllerShutdown| will not be called thereon.
  // TODO(jamescook): Clean up shutdown order so this code is not reached (and
  // then remove it). See also the TODO in |~TouchExplorationManager|.
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void TouchExplorationManager::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  if (key != aura::client::kAccessibilityTouchExplorationPassThrough)
    return;

  UpdateTouchExplorationState();
}

void TouchExplorationManager::OnWindowDestroying(aura::Window* window) {
  DCHECK(observing_window_ == window);
  observing_window_->RemoveObserver(this);
  observing_window_ = nullptr;
}

void TouchExplorationManager::SetOutputLevel(int volume) {
  if (volume > 0) {
    if (audio_handler_->IsOutputMuted()) {
      audio_handler_->SetOutputMute(false);
    }
  }
  audio_handler_->SetOutputVolumePercent(volume);
  // Avoid negative volume.
  if (audio_handler_->IsOutputVolumeBelowDefaultMuteLevel())
    audio_handler_->SetOutputMute(true);
}

void TouchExplorationManager::SilenceSpokenFeedback() {
  if (GetA11yController()->spoken_feedback().enabled())
    GetA11yController()->SilenceSpokenFeedback();
}

void TouchExplorationManager::PlayVolumeAdjustEarcon() {
  if (!VolumeAdjustSoundEnabled())
    return;
  if (!audio_handler_->IsOutputMuted() &&
      audio_handler_->GetOutputVolumePercent() != 100) {
    GetA11yController()->PlayEarcon(Sound::kVolumeAdjust);
  }
}

void TouchExplorationManager::PlayPassthroughEarcon() {
  GetA11yController()->PlayEarcon(Sound::kPassthrough);
}

void TouchExplorationManager::PlayLongPressRightClickEarcon() {
  // TODO: Rename this sound to SOUND_LONG_PRESS_RIGHT_CLICK.
  GetA11yController()->PlayEarcon(Sound::kExitScreen);
}

void TouchExplorationManager::PlayEnterScreenEarcon() {
  GetA11yController()->PlayEarcon(Sound::kEnterScreen);
}

void TouchExplorationManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  base::UmaHistogramEnumeration("Accessibility.ChromeVox.PerformGestureType",
                                gesture);
  GetA11yController()->HandleAccessibilityGesture(gesture, location);
}

void TouchExplorationManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  const display::Display this_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          root_window_controller_->GetRootWindow());
  if (this_display.id() == display.id())
    UpdateTouchExplorationState();
}

void TouchExplorationManager::PlayTouchTypeEarcon() {
  GetA11yController()->PlayEarcon(Sound::kTouchType);
}

void TouchExplorationManager::ToggleSpokenFeedback() {
  if (GetA11yController()->ShouldToggleSpokenFeedbackViaTouch()) {
    GetA11yController()->SetSpokenFeedbackEnabled(
        !GetA11yController()->spoken_feedback().enabled(),
        A11Y_NOTIFICATION_SHOW);
  }
}

void TouchExplorationManager::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (lost_active && lost_active->HasObserver(this)) {
    lost_active->RemoveObserver(this);
    observing_window_ = nullptr;
  }

  if (gained_active && !gained_active->HasObserver(this)) {
    gained_active->AddObserver(this);
    observing_window_ = gained_active;
  }

  UpdateTouchExplorationState();
}

void TouchExplorationManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  if (touch_exploration_controller_) {
    touch_exploration_controller_->SetTouchAccessibilityAnchorPoint(
        anchor_point);
  }
}

void TouchExplorationManager::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& new_bounds) {
  UpdateTouchExplorationState();
}

void TouchExplorationManager::OnKeyboardEnabledChanged(bool is_enabled) {
  UpdateTouchExplorationState();
}

void TouchExplorationManager::UpdateTouchExplorationState() {
  // See crbug.com/603745 for more details.
  const bool pass_through_surface =
      window_util::GetActiveWindow() &&
      window_util::GetActiveWindow()->GetProperty(
          aura::client::kAccessibilityTouchExplorationPassThrough);

  const bool spoken_feedback_enabled =
      GetA11yController()->spoken_feedback().enabled();

  if (!touch_accessibility_enabler_) {
    // Always enable gesture to toggle spoken feedback.
    touch_accessibility_enabler_ = std::make_unique<TouchAccessibilityEnabler>(
        root_window_controller_->GetRootWindow(), this);
  }

  if (spoken_feedback_enabled) {
    if (!touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          std::make_unique<TouchExplorationController>(
              root_window_controller_->GetRootWindow(), this,
              touch_accessibility_enabler_->GetWeakPtr());
    }
    if (pass_through_surface) {
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              root_window_controller_->GetRootWindow());
      const gfx::Rect work_area = display.work_area();
      touch_exploration_controller_->SetExcludeBounds(work_area);
      SilenceSpokenFeedback();
      // Clear the focus highlight.
      Shell::Get()->accessibility_focus_ring_controller()->SetFocusRing(
          extension_misc::kChromeVoxExtensionId,
          std::make_unique<AccessibilityFocusRingInfo>());
    } else {
      touch_exploration_controller_->SetExcludeBounds(gfx::Rect());
    }

    // Virtual keyboard.
    auto* keyboard_controller = keyboard::KeyboardUIController::Get();
    if (keyboard_controller->IsEnabled()) {
      touch_exploration_controller_->SetLiftActivationBounds(
          keyboard_controller->GetVisualBoundsInScreen());
    }
  } else {
    touch_exploration_controller_.reset();
  }
}

bool TouchExplorationManager::VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableVolumeAdjustSound);
}

}  // namespace ash
