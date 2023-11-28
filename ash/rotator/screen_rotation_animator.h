// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/display/display_configuration_controller.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LayerTreeOwner;
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace viz {
class CopyOutputRequest;
class CopyOutputResult;
}  // namespace viz

namespace ash {
class ScreenRotationAnimatorObserver;

// Utility to perform a screen rotation with an animation.
class ASH_EXPORT ScreenRotationAnimator {
 public:
  static ScreenRotationAnimator* GetForRootWindow(aura::Window* root_window);

  explicit ScreenRotationAnimator(aura::Window* root_window);

  ScreenRotationAnimator(const ScreenRotationAnimator&) = delete;
  ScreenRotationAnimator& operator=(const ScreenRotationAnimator&) = delete;

  virtual ~ScreenRotationAnimator();

  // Rotates the display::Display specified by |display_id| of the |root_window|
  // to the |new_rotation| orientation, for the given |source|. The rotation
  // will also become active. |screen_rotation_animator_observer_| will be
  // notified when rotation is finished and there is no more pending rotation
  // request. Otherwise, any ongoing animation will be stopped and progressed to
  // the target position, followed by a new |Rotate()| call with the pending
  // rotation request.
  void Rotate(display::Display::Rotation new_rotation,
              display::Display::RotationSource source,
              DisplayConfigurationController::RotationAnimation mode);

  void AddObserver(ScreenRotationAnimatorObserver* observer);
  void RemoveObserver(ScreenRotationAnimatorObserver* observer);

  // When screen rotation animation is ended or aborted, calls |Rotate()| with
  // the pending rotation request if the request queue is not empty. Otherwise
  // notifies |screen_rotation_animator_observer_|.
  void ProcessAnimationQueue();

  // True if the screen is in rotating state (not IDLE).
  bool IsRotating() const;

  // Returns the target (new) rotation. This will return the last requested
  // orientation if |IsRotating()| is false.
  display::Display::Rotation GetTargetRotation() const;

 protected:
  using CopyCallback =
      base::OnceCallback<void(std::unique_ptr<viz::CopyOutputResult> result)>;
  struct ScreenRotationRequest {
    ScreenRotationRequest(
        int64_t id,
        int64_t display_id,
        display::Display::Rotation to_rotation,
        display::Display::RotationSource from_source,
        DisplayConfigurationController::RotationAnimation mode)
        : id(id),
          display_id(display_id),
          new_rotation(to_rotation),
          source(from_source),
          mode(mode) {}
    int64_t id;
    int64_t display_id;
    display::Display::Rotation old_rotation;
    display::Display::Rotation new_rotation;
    display::Display::RotationSource source;
    DisplayConfigurationController::RotationAnimation mode;
  };

  // This function can be overridden in unit test to test removing external
  // display.
  virtual CopyCallback CreateAfterCopyCallbackBeforeRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

  // This function can be overridden in unit test to test removing external
  // display.
  virtual CopyCallback CreateAfterCopyCallbackAfterRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

 private:
  friend class ScreenRotationAnimatorTestApi;

  void StartRotationAnimation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

  // The code path to start "slow animation". The difference between the "slow"
  // and "smooth" animation, is that "slow animation" will recreate all the
  // layers before rotation and use the recreated layers and rotated layers for
  // cross-fading animation. This is slow by adding multiple layer animation
  // elements. The "smooth animation" copies the layer output before and after
  // rotation, and use them for cross-fading animation. The output copy layer
  // flatten the layer hierarchy and makes the animation smooth.
  void StartSlowAnimation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

  // A wrapper to call |display_manager| to set screen rotation and rotate the
  // |old_layer_tree| to the |old_rotation|.
  void SetRotation(int64_t display_id,
                   display::Display::Rotation old_rotation,
                   display::Display::Rotation new_rotation,
                   display::Display::RotationSource source);

  // This is an asynchronous call to request copy output of root layer.
  void RequestCopyScreenRotationContainerLayer(
      std::unique_ptr<viz::CopyOutputRequest> copy_output_request);

  // The callback in |RequestCopyScreenRotationContainerLayer()| before screen
  // rotation.
  void OnScreenRotationContainerLayerCopiedBeforeRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request,
      std::unique_ptr<viz::CopyOutputResult> result);

  // The callback in |RequestCopyScreenRotationContainerLayer()| after screen
  // rotation.
  void OnScreenRotationContainerLayerCopiedAfterRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request,
      std::unique_ptr<viz::CopyOutputResult> result);

  // Recreates all |root_window| layers and their layer tree owner.
  void CreateOldLayerTreeForSlowAnimation();

  // Creates a new layer and its layer tree owner from |CopyOutputResult|.
  std::unique_ptr<ui::LayerTreeOwner> CopyLayerTree(
      std::unique_ptr<viz::CopyOutputResult> result);

  // Note: Only call this function when the |old_layer_tree_owner_| is set up
  // properly.
  // Sets the screen orientation to |new_rotation| and animate the change. The
  // animation will rotate the initial orientation's layer towards the new
  // orientation through |rotation_degrees| while fading out, and the new
  // orientation's layer will be rotated in to the |new_orientation| through
  // |rotation_degrees| arc.
  void AnimateRotation(std::unique_ptr<ScreenRotationRequest> rotation_request);

  void NotifyAnimationFinished(bool canceled);

  void set_disable_animation_timers_for_test(bool disable_timers) {
    disable_animation_timers_for_test_ = disable_timers;
  }

  void StopAnimating();

  raw_ptr<aura::Window> root_window_;

  // For current slow rotation animation, there are two states |ROTATING| and
  // |IDLE|. For the smooth rotation animation, we need to send copy request
  // and get copy result before animating.
  enum ScreenRotationState {
    COPY_REQUESTED,
    ROTATING,
    IDLE,
  };
  ScreenRotationState screen_rotation_state_;

  // Rotation request id, used to ignore copy request callback if we decide to
  // cancel the previous rotation request.
  int64_t rotation_request_id_;

  // Only set in unittest to disable animation timers.
  bool disable_animation_timers_for_test_;
  base::ObserverList<ScreenRotationAnimatorObserver>::Unchecked
      screen_rotation_animator_observers_;
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner_;
  std::unique_ptr<ui::LayerTreeOwner> new_layer_tree_owner_;
  std::unique_ptr<ui::LayerTreeOwner> mask_layer_tree_owner_;
  std::unique_ptr<ScreenRotationRequest> last_pending_request_;
  std::optional<ScreenRotationRequest> current_async_rotation_request_;
  display::Display::Rotation target_rotation_ = display::Display::ROTATE_0;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_scale_mode_;
  base::WeakPtrFactory<ScreenRotationAnimator> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
