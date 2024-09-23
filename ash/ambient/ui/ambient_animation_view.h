// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ambient/model/ambient_animation_photo_provider.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/jitter_calculator.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
class AmbientAnimationFrameRateController;
class AmbientAnimationPlayer;
class AmbientAnimationProgressTracker;
class AmbientAnimationStaticResources;
class AmbientAnimationShieldController;
class AmbientViewDelegateImpl;

class ASH_EXPORT AmbientAnimationView : public views::View,
                                        public lottie::AnimationObserver,
                                        public views::ViewObserver,
                                        public GlanceableInfoView::Delegate,
                                        public MediaStringView::Delegate {
  METADATA_HEADER(AmbientAnimationView, views::View)

 public:
  AmbientAnimationView(
      AmbientViewDelegateImpl* view_delegate,
      AmbientAnimationProgressTracker* progress_tracker,
      std::unique_ptr<const AmbientAnimationStaticResources> static_resources,
      AmbientAnimationFrameRateController* frame_rate_controller);
  AmbientAnimationView(const AmbientAnimationView&) = delete;
  AmbientAnimationView& operator=(AmbientAnimationView&) = delete;
  ~AmbientAnimationView() override;

  JitterCalculator* GetJitterCalculatorForTesting();

 private:
  void Init();

  void AnimationCycleEnded(const lottie::Animation* animation) override;

  void OnViewBoundsChanged(View* observed_view) override;
  void OnViewAddedToWidget(View* observed_view) override;

  // GlanceableInfoView::Delegate:
  SkColor GetTimeTemperatureFontColor() override;

  // MediaStringView::Delegate:
  MediaStringView::Settings GetSettings() override;

  void StartPlayingAnimation();
  void StartThroughputTracking();
  void RestartThroughputTracking();
  void ApplyJitter();

  const raw_ptr<AmbientViewDelegateImpl> view_delegate_;
  const raw_ptr<AmbientAnimationProgressTracker> progress_tracker_;
  const std::unique_ptr<const AmbientAnimationStaticResources>
      static_resources_;
  const raw_ptr<AmbientAnimationFrameRateController> frame_rate_controller_;
  const bool add_glanceable_info_text_shadow_;
  AmbientAnimationPhotoProvider animation_photo_provider_;
  std::unique_ptr<AmbientAnimationAttributionProvider>
      animation_attribution_provider_;

  raw_ptr<views::AnimatedImageView> animated_image_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> glanceable_info_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> media_string_container_ = nullptr;
  std::unique_ptr<AmbientAnimationShieldController> shield_view_controller_;
  std::unique_ptr<AmbientAnimationPlayer> animation_player_;
  base::ScopedObservation<View, ViewObserver> animated_image_view_observer_{
      this};
  base::ScopedObservation<lottie::Animation, lottie::AnimationObserver>
      animation_observer_{this};

  std::optional<ui::ThroughputTracker> throughput_tracker_;
  base::RepeatingTimer throughput_tracker_restart_timer_;

  JitterCalculator animation_jitter_calculator_;
  base::TimeTicks last_jitter_timestamp_;

  base::WeakPtrFactory<AmbientAnimationView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
