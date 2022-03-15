// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_

#include <memory>

#include "ash/ambient/model/ambient_animation_photo_provider.h"
#include "ash/ambient/ui/jitter_calculator.h"
#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/lottie/animation.h"
#include "ui/lottie/animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class AnimatedImageView;
class BoxLayoutView;
}  // namespace views

namespace ash {

class AmbientAnimationAttributionProvider;
class AmbientAnimationPlayer;
class AmbientAnimationStaticResources;
class AmbientAnimationShieldController;
class AmbientViewDelegate;
class AmbientViewEventHandler;

class ASH_EXPORT AmbientAnimationView : public views::View,
                                        public lottie::AnimationObserver,
                                        public views::ViewObserver {
 public:
  METADATA_HEADER(AmbientAnimationView);

  AmbientAnimationView(
      AmbientViewDelegate* view_delegate,
      std::unique_ptr<const AmbientAnimationStaticResources> static_resources);
  AmbientAnimationView(const AmbientAnimationView&) = delete;
  AmbientAnimationView& operator=(AmbientAnimationView&) = delete;
  ~AmbientAnimationView() override;

 private:
  void Init(AmbientViewDelegate* view_delegate);

  void AnimationCycleEnded(const lottie::Animation* animation) override;

  void OnViewBoundsChanged(View* observed_view) override;

  void StartPlayingAnimation();
  void StartThroughputTracking();
  void RestartThroughputTracking();
  void ApplyJitter();

  AmbientViewEventHandler* const event_handler_;

  const std::unique_ptr<const AmbientAnimationStaticResources>
      static_resources_;
  AmbientAnimationPhotoProvider animation_photo_provider_;
  std::unique_ptr<AmbientAnimationAttributionProvider>
      animation_attribution_provider_;

  views::AnimatedImageView* animated_image_view_ = nullptr;
  views::BoxLayoutView* glanceable_info_container_ = nullptr;
  views::BoxLayoutView* media_string_container_ = nullptr;
  std::unique_ptr<AmbientAnimationShieldController> shield_view_controller_;
  std::unique_ptr<AmbientAnimationPlayer> animation_player_;
  base::ScopedObservation<View, ViewObserver> animated_image_view_observer_{
      this};
  base::ScopedObservation<lottie::Animation, lottie::AnimationObserver>
      animation_observer_{this};

  absl::optional<ui::ThroughputTracker> throughput_tracker_;
  base::RepeatingTimer throughput_tracker_restart_timer_;

  JitterCalculator animation_jitter_calculator_;
  base::TimeTicks last_jitter_timestamp_;

  base::WeakPtrFactory<AmbientAnimationView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
