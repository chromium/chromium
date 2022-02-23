// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_view.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_resizer.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// How often to shift the animation slightly to prevent screen burn.
constexpr base::TimeDelta kAnimationJitterPeriod = base::Minutes(2);

constexpr JitterCalculator::Config kAnimationJitterConfig = {
    /*step_size=*/2,
    /*x_min_translation=*/-10,
    /*x_max_translation=*/10,
    /*y_min_translation=*/-10,
    /*y_max_translation=*/10};

constexpr base::TimeDelta kThroughputTrackerRestartPeriod = base::Seconds(30);

// TODO(esum): Record throughput metrics to track animation performance in the
// field. We can use ash::metrics_util::CalculateSmoothness().
void LogCompositorThroughput(
    base::TimeTicks logging_start_time,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  base::TimeDelta duration = base::TimeTicks::Now() - logging_start_time;
  float duration_sec = duration.InSecondsF();
  // Use VLOG instead of DVLOG since this log is performance-related and
  // developers will almost certainly only care about this log on non-debug
  // builds. The overhead of "--vmodule" regex matching is very minor so far to
  // performance/CPU.
  VLOG(1) << "Compositor throughput report: frames_expected="
          << data.frames_expected << " frames_produced=" << data.frames_produced
          << " jank_count=" << data.jank_count
          << " expected_fps=" << data.frames_expected / duration_sec
          << " actual_fps=" << data.frames_produced / duration_sec
          << " duration=" << duration;
}

// Returns the maximum possible displacement in either dimension from the
// original unshifted position when jitter is applied.
int GetPaddingForAnimationJitter() {
  return std::max({abs(kAnimationJitterConfig.x_min_translation),
                   abs(kAnimationJitterConfig.x_max_translation),
                   abs(kAnimationJitterConfig.y_min_translation),
                   abs(kAnimationJitterConfig.y_max_translation)});
}

}  // namespace

AmbientAnimationView::AmbientAnimationView(
    const AmbientBackendModel* model,
    AmbientViewEventHandler* event_handler,
    std::unique_ptr<const AmbientAnimationStaticResources> static_resources)
    : event_handler_(event_handler),
      static_resources_(std::move(static_resources)),
      animation_photo_provider_(static_resources_.get(), model),
      animation_jitter_calculator_(kAnimationJitterConfig) {
  SetID(AmbientViewID::kAmbientAnimationView);
  Init();
}

AmbientAnimationView::~AmbientAnimationView() = default;

void AmbientAnimationView::Init() {
  SetUseDefaultFillLayout(true);
  animated_image_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  // Purely for performance reasons. Gains 3-4 fps.
  animated_image_view_->SetPaintToLayer();
  base::span<const uint8_t> lottie_data_bytes =
      base::as_bytes(base::make_span(static_resources_->GetLottieData()));
  // Create a serializable SkottieWrapper since the SkottieWrapper may have to
  // be serialized and transmitted over IPC for out-of-process rasterization.
  auto animation = std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::CreateSerializable(std::vector<uint8_t>(
          lottie_data_bytes.begin(), lottie_data_bytes.end())),
      cc::SkottieColorMap(), &animation_photo_provider_);
  animation->SetAnimationObserver(this);
  animated_image_view_->SetAnimatedImage(std::move(animation));
  animated_image_view_observer_.Observe(animated_image_view_);
}

void AmbientAnimationView::AnimationWillStartPlaying(
    const lottie::Animation* animation) {
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiStartRendering);
  last_jitter_timestamp_ = base::TimeTicks::Now();
}

void AmbientAnimationView::AnimationCycleEnded(
    const lottie::Animation* animation) {
  event_handler_->OnMarkerHit(AmbientPhotoConfig::Marker::kUiCycleEnded);
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_jitter_timestamp_ >= kAnimationJitterPeriod) {
    gfx::Vector2d jitter = animation_jitter_calculator_.Calculate();
    DVLOG(4) << "Applying jitter to animation: " << jitter.ToString();
    animated_image_view_->SetAdditionalTranslation(std::move(jitter));
    last_jitter_timestamp_ = now;
  }
}

void AmbientAnimationView::OnViewBoundsChanged(View* observed_view) {
  DCHECK_EQ(observed_view, static_cast<View*>(animated_image_view_));
  DVLOG(4) << __func__ << " to "
           << animated_image_view_->GetContentsBounds().ToString();
  if (animated_image_view_->GetContentsBounds().IsEmpty())
    return;

  // By default, the |animated_image_view_| will render the animation with the
  // fixed dimensions specified in the Lottie file. To render the animation
  // at the view's full bounds, wait for the view's initial layout to happen
  // so that its proper bounds become available (they are 0x0 initially) before
  // starting the animation playback.
  gfx::Rect previous_animation_bounds = animated_image_view_->GetImageBounds();
  AmbientAnimationResizer::Resize(*animated_image_view_,
                                  GetPaddingForAnimationJitter());
  DVLOG(4)
      << "View bounds available. Resized animation with native size "
      << animated_image_view_->animated_image()->GetOriginalSize().ToString()
      << " from " << previous_animation_bounds.ToString() << " to "
      << animated_image_view_->GetImageBounds().ToString();
  animated_image_view_->Play();
  if (!throughput_tracker_restart_timer_.IsRunning()) {
    RestartThroughputTracking();
    throughput_tracker_restart_timer_.Start(
        FROM_HERE, kThroughputTrackerRestartPeriod, this,
        &AmbientAnimationView::RestartThroughputTracking);
  }
}

void AmbientAnimationView::RestartThroughputTracking() {
  // Stop() must be called to trigger throughput reporting.
  if (throughput_tracker_ && !throughput_tracker_->Stop()) {
    LOG(WARNING) << "Throughput will not be reported";
  }

  views::Widget* widget = GetWidget();
  DCHECK(widget);
  ui::Compositor* compositor = widget->GetCompositor();
  DCHECK(compositor);
  throughput_tracker_ = compositor->RequestNewThroughputTracker();
  throughput_tracker_->Start(base::BindOnce(
      &LogCompositorThroughput, /*logging_start_time=*/base::TimeTicks::Now()));
}

BEGIN_METADATA(AmbientAnimationView, views::View)
END_METADATA

}  // namespace ash
