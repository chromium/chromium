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
#include "ash/ambient/ui/glanceable_info_view.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/border.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"
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

// Amount of x and y padding there should be from the top-left of the
// AmbientAnimationView to the top-left of the weather/time content views.
constexpr int kWeatherTimeBorderPaddingDip = 28;

constexpr int kTimeFontSizeDip = 32;

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

// The border serves as padding between the GlanceableInfoView and its
// parent view's bounds.
std::unique_ptr<views::Border> CreateGlanceableInfoBorder(
    const gfx::Vector2d& jitter = gfx::Vector2d()) {
  int top_padding = kWeatherTimeBorderPaddingDip + jitter.y();
  int left_padding = kWeatherTimeBorderPaddingDip + jitter.x();
  DCHECK_GE(top_padding, 0);
  DCHECK_GE(left_padding, 0);
  return views::CreateEmptyBorder(top_padding, left_padding,
                                  /*bottom=*/0, /*right=*/0);
}

}  // namespace

AmbientAnimationView::AmbientAnimationView(
    AmbientViewDelegate* view_delegate,
    std::unique_ptr<const AmbientAnimationStaticResources> static_resources)
    : event_handler_(view_delegate->GetAmbientViewEventHandler()),
      static_resources_(std::move(static_resources)),
      animation_photo_provider_(static_resources_.get(),
                                view_delegate->GetAmbientBackendModel()),
      animation_jitter_calculator_(kAnimationJitterConfig) {
  SetID(AmbientViewID::kAmbientAnimationView);
  Init(view_delegate);
}

AmbientAnimationView::~AmbientAnimationView() = default;

void AmbientAnimationView::Init(AmbientViewDelegate* view_delegate) {
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

  // The set of weather/time views embedded within GlanceableInfoView should
  // appear in the top-left of the the AmbientAnimationView's boundaries with
  // |kWeatherTimeBorderPaddingDip| from the top-left corner. However, the
  // weather/time components must be bottom-aligned like so:
  // +-------------------------------------------------------------------------+
  // |                                                                         |
  // |  +----+     +--+                                                        |
  // |  |    |+---+|  |                                                        |
  // |  |    ||   ||  |                                                        |
  // |  +----++---++--+                                                        |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // +-------------------------------------------------------------------------+
  // As opposed to top-aligned :
  // +-------------------------------------------------------------------------+
  // |                                                                         |
  // |  +----++---++--+                                                        |
  // |  |    ||   ||  |                                                        |
  // |  |    |+---+|  |                                                        |
  // |  +----+     +--+                                                        |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // |                                                                         |
  // +-------------------------------------------------------------------------+
  //
  // To accomplish this, a "container" view is first created that is top-aligned
  // and has no actual content. GlanceableInfoView is then added as a child of
  // the container view and bottom-aligns its contents within the container.
  glanceable_info_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  glanceable_info_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  glanceable_info_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  glanceable_info_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  glanceable_info_container_->SetBorder(CreateGlanceableInfoBorder());
  glanceable_info_container_->AddChildView(
      std::make_unique<GlanceableInfoView>(view_delegate, kTimeFontSizeDip));
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
    // AnimationCycleEnded() may be called while a ui "paint" operation is still
    // in progress. Changing translation properties of the UI while a paint
    // operation is in progress results in a fatal error deep in the UI stack.
    // Thus, post a task to apply jitter rather than invoking it synchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AmbientAnimationView::ApplyJitter,
                                  weak_factory_.GetWeakPtr()));
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

void AmbientAnimationView::ApplyJitter() {
  gfx::Vector2d jitter = animation_jitter_calculator_.Calculate();
  DVLOG(4) << "Applying jitter to animation: " << jitter.ToString();
  // Sharing the same jitter between the animation and glanceable info keeps the
  // spacing between the weather/time and animation features consistent.
  animated_image_view_->SetAdditionalTranslation(jitter);
  glanceable_info_container_->SetBorder(CreateGlanceableInfoBorder(jitter));
}

BEGIN_METADATA(AmbientAnimationView, views::View)
END_METADATA

}  // namespace ash
