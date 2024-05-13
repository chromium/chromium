// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_
#define ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/lottie/animation_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ASH_EXPORT BootingAnimationController
    : public display::DisplayConfigurator::Observer,
      public lottie::AnimationObserver {
 public:
  BootingAnimationController();
  BootingAnimationController(const BootingAnimationController&) = delete;
  BootingAnimationController& operator=(const BootingAnimationController&) =
      delete;
  ~BootingAnimationController() override;

  // Sets the `animation_played_callback_` that is fired when the animation
  // finishes playing. Starts the animation if the GPU is ready, otherwise
  // waits for it.
  void ShowAnimationWithEndCallback(base::OnceClosure callback);

  // Cleans up the animation, resets the widget and the view.
  void Finish();

  base::WeakPtr<BootingAnimationController> GetWeakPtr();

 private:
  // display::DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;
  void OnDisplaySnapshotsInvalidated() override;

  // lottie::AnimationObserver:
  void AnimationCycleEnded(const lottie::Animation* animation) override;

  // Shows the widget and starts to play a booting animation.
  void Show();
  void OnAnimationDataFetched(std::string data);
  void StartAnimation();
  void IgnoreGpuReadiness();
  bool IsDeviceReady() const;

  std::string animation_data_;
  std::unique_ptr<views::Widget> widget_;
  std::optional<bool> data_fetch_failed_;
  bool was_shown_ = false;
  bool is_gpu_ready_ = false;
  base::OnceClosure animation_played_callback_;

  base::ScopedObservation<display::DisplayConfigurator,
                          display::DisplayConfigurator::Observer>
      scoped_display_configurator_observer_{this};

  base::ScopedObservation<lottie::Animation, lottie::AnimationObserver>
      scoped_animation_observer_{this};

  base::WeakPtrFactory<BootingAnimationController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BOOTING_BOOTING_ANIMATION_CONTROLLER_H_
