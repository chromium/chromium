// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_view.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/model/ambient_animation_attribution_provider.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_attribution_transformer.h"
#include "ash/ambient/ui/ambient_animation_background_color.h"
#include "ash/ambient/ui/ambient_animation_frame_rate_controller.h"
#include "ash/ambient/ui/ambient_animation_player.h"
#include "ash/ambient/ui/ambient_animation_resizer.h"
#include "ash/ambient/ui/ambient_animation_shield_controller.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/jitter_calculator.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/skottie_color_map.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
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

constexpr base::TimeDelta kThroughputTrackerRestartPeriod = base::Seconds(30);

// Amount of x and y padding there should be from the top-left of the
// AmbientAnimationView to the top-left of the weather/time content views.
constexpr int kWeatherTimeBorderPaddingDip = 28;

// Amount of padding from the top-right of the AmbientAnimationView's
// bounds to the top-right of the media string content views.
constexpr int kMediaStringPaddingDip = 28;

constexpr int kMediaStringTextElevation = 1;

constexpr int kTimeFontSizeDip = 32;

// Google Grey 500 with 10% opacity.
constexpr SkColor kDarkModeShieldColor =
    SkColorSetA(gfx::kGoogleGrey900, SK_AlphaOPAQUE / 10);

void LogCompositorThroughput(const AmbientUiSettings& ui_settings,
                             int smoothness) {
  // Use VLOG instead of DVLOG since this log is performance-related and
  // developers will almost certainly only care about this log on non-debug
  // builds.
  VLOG(1) << "Compositor throughput report: smoothness=" << smoothness;
  ambient::RecordAmbientModeAnimationSmoothness(smoothness, ui_settings);
}

void OnCompositorThroughputReported(
    base::TimeTicks logging_start_time,
    const AmbientUiSettings& ui_settings,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  base::TimeDelta duration = base::TimeTicks::Now() - logging_start_time;
  float duration_sec = duration.InSecondsF();
  VLOG(1) << "Compositor throughput report: frames_expected_v3="
          << data.frames_expected_v3
          << " frames_dropped_v3=" << data.frames_dropped_v3
          << " jank_count_v3=" << data.jank_count_v3
          << " expected_fps=" << data.frames_expected_v3 / duration_sec
          << " actual_fps="
          << (data.frames_expected_v3 - data.frames_dropped_v3) / duration_sec
          << " duration=" << duration;
  metrics_util::ForSmoothnessV3(
      base::BindRepeating(&LogCompositorThroughput, ui_settings))
      .Run(data);
}

// Returns the maximum possible displacement in either dimension from the
// original unshifted position when jitter is applied.
int GetPaddingForAnimationJitter(const AmbientJitterConfig& config) {
  return std::max({abs(config.x_min_translation), abs(config.x_max_translation),
                   abs(config.y_min_translation),
                   abs(config.y_max_translation)});
}

// When text with shadows requires X pixels of padding from the edges of its
// bounding view, it is not always sufficient to simply create a border within
// the view that is X pixels wide. In the event that the text's shadow extends
// past the text in a given direction, the text's shadow ends up with X pixels
// of padding from the edge rather than the text itself.
//
// This returns the amount to *subtract* from each side of a text view's border
// such that the text ultimately has X pixels of padding from the view's edge,
// and the shadow may extend into the padding.
gfx::Outsets GetTextShadowCorrection(const gfx::ShadowValues& text_shadows) {
  // A positive shadow outset means the shadow extends past the text in that
  // direction. A negative shadow outset means the shadow is "behind" the text
  // in that direction. In this case, subtracting the negative outset value
  // will result in padding that is too large (X + <shadow offset>). Hence,
  // impose a "floor" of 0 pixels here.
  static constexpr gfx::Outsets kZeroOutsetsFloor;
  gfx::Outsets shadow_outsets =
      gfx::ShadowValue::GetMargin(text_shadows).ToOutsets();
  shadow_outsets.SetToMax(kZeroOutsetsFloor);
  return shadow_outsets;
}

// The border serves as padding between the GlanceableInfoView and its
// parent view's bounds.
std::unique_ptr<views::Border> CreateGlanceableInfoBorder(
    bool include_text_shadow,
    const gfx::Vector2d& jitter = gfx::Vector2d()) {
  gfx::Outsets shadow_text_correction;
  if (include_text_shadow) {
    shadow_text_correction =
        GetTextShadowCorrection(ambient::util::GetTextShadowValues(nullptr));
  }
  int top_padding =
      kWeatherTimeBorderPaddingDip - shadow_text_correction.top() + jitter.y();
  int left_padding =
      kWeatherTimeBorderPaddingDip - shadow_text_correction.left() + jitter.x();
  DCHECK_GE(top_padding, 0);
  DCHECK_GE(left_padding, 0);
  return views::CreateEmptyBorder(
      gfx::Insets::TLBR(top_padding, left_padding, 0, 0));
}

// The border serves as padding between the MediaStringView and its
// parent view's bounds.
std::unique_ptr<views::Border> CreateMediaStringBorder(
    const gfx::Vector2d& jitter = gfx::Vector2d()) {
  gfx::Outsets shadow_text_correction = GetTextShadowCorrection(
      ambient::util::GetTextShadowValues(nullptr, kMediaStringTextElevation));
  int top_padding =
      kMediaStringPaddingDip - shadow_text_correction.top() + jitter.y();
  int right_padding =
      kMediaStringPaddingDip - shadow_text_correction.right() + jitter.x();
  DCHECK_GE(top_padding, 0);
  DCHECK_GE(right_padding, 0);
  return views::CreateEmptyBorder(
      gfx::Insets::TLBR(top_padding, 0, 0, right_padding));
}

}  // namespace

AmbientAnimationView::AmbientAnimationView(
    AmbientViewDelegateImpl* view_delegate,
    AmbientAnimationProgressTracker* progress_tracker,
    std::unique_ptr<const AmbientAnimationStaticResources> static_resources,
    AmbientAnimationFrameRateController* frame_rate_controller)
    : view_delegate_(view_delegate),
      progress_tracker_(progress_tracker),
      static_resources_(std::move(static_resources)),
      frame_rate_controller_(frame_rate_controller),
      add_glanceable_info_text_shadow_(
          static_resources_->GetUiSettings().theme() !=
          personalization_app::mojom::AmbientTheme::kFeelTheBreeze),
      animation_photo_provider_(static_resources_.get(),
                                view_delegate->GetAmbientBackendModel()),
      animation_jitter_calculator_(
          AmbientUiModel::Get()->GetAnimationJitterConfig()) {
  DCHECK(view_delegate_);
  DCHECK(frame_rate_controller_);
  SetID(AmbientViewID::kAmbientAnimationView);
  Init();
}

AmbientAnimationView::~AmbientAnimationView() = default;

void AmbientAnimationView::Init() {
  SetUseDefaultFillLayout(true);

  views::View* animation_container_view =
      AddChildView(std::make_unique<views::View>());
  animation_container_view->SetUseDefaultFillLayout(true);
  // Purely for performance reasons. Gains 3-4 fps.
  animation_container_view->SetPaintToLayer();
  // In portrait mode, the landscape animation file is currently being used. Its
  // width is scaled down to match the width of the portrait screen, and it's
  // center-aligned leaving empty space on the top and bottom of the screen. To
  // make this look less obvious to the user, make the empty space exactly match
  // the background color of the animation itself. This may be removed in the
  // future if portrait versions of the animations are made.
  animation_container_view->SetBackground(views::CreateSolidBackground(
      GetAnimationBackgroundColor(*static_resources_->GetSkottieWrapper())));

  animated_image_view_ = animation_container_view->AddChildView(
      std::make_unique<views::AnimatedImageView>());
  auto animation = std::make_unique<lottie::Animation>(
      static_resources_->GetSkottieWrapper(), cc::SkottieColorMap(),
      &animation_photo_provider_);
  animation_observer_.Observe(animation.get());
  animated_image_view_->SetAnimatedImage(std::move(animation));
  animated_image_view_observer_.Observe(animated_image_view_.get());
  animation_attribution_provider_ =
      std::make_unique<AmbientAnimationAttributionProvider>(
          &animation_photo_provider_, animated_image_view_->animated_image());

  // SetPaintToLayer() causes a view to be painted above its non-layer-backed
  // siblings, irrespective of the order they were added in. Using an
  // intermediate layer-backed |animation_container_view| ensures the shield is
  // painted on top of the animation, while still getting performance benefits.
  auto shield_view = std::make_unique<views::View>();
  shield_view->SetID(kAmbientShieldView);
  shield_view->SetBackground(
      views::CreateSolidBackground(kDarkModeShieldColor));
  shield_view_controller_ = std::make_unique<AmbientAnimationShieldController>(
      std::move(shield_view), /*parent_view=*/animation_container_view);

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
  glanceable_info_container_->SetBorder(
      CreateGlanceableInfoBorder(add_glanceable_info_text_shadow_));
  glanceable_info_container_->AddChildView(std::make_unique<GlanceableInfoView>(
      view_delegate_.get(), this, kTimeFontSizeDip,
      add_glanceable_info_text_shadow_));

  // Media string should appear in the top-right corner of the
  // AmbientAnimationView's bounds.
  media_string_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  media_string_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  media_string_container_->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  media_string_container_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  media_string_container_->SetBorder(CreateMediaStringBorder());
  MediaStringView* media_string_view = media_string_container_->AddChildView(
      std::make_unique<MediaStringView>(this));
  media_string_view->SetVisible(false);
}

void AmbientAnimationView::AnimationCycleEnded(
    const lottie::Animation* animation) {
  view_delegate_->NotifyObserversMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_jitter_timestamp_ >= kAnimationJitterPeriod) {
    // AnimationCycleEnded() may be called while a ui "paint" operation is still
    // in progress. Changing translation properties of the UI while a paint
    // operation is in progress results in a fatal error deep in the UI stack.
    // Thus, post a task to apply jitter rather than invoking it synchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AmbientAnimationView::ApplyJitter,
                                  weak_factory_.GetWeakPtr()));
    last_jitter_timestamp_ = now;
  }
}

void AmbientAnimationView::OnViewBoundsChanged(View* observed_view) {
  DCHECK_EQ(observed_view, static_cast<View*>(animated_image_view_));
  gfx::Rect content_bounds = animated_image_view_->GetContentsBounds();
  DVLOG(4) << __func__ << " to " << content_bounds.ToString();
  if (content_bounds.IsEmpty())
    return;

  // By default, the |animated_image_view_| will render the animation with the
  // fixed dimensions specified in the Lottie file. To render the animation
  // at the view's full bounds, wait for the view's initial layout to happen
  // so that its proper bounds become available (they are 0x0 initially) before
  // starting the animation playback.
  gfx::Rect previous_animation_bounds = animated_image_view_->GetImageBounds();
  AmbientAnimationResizer::Resize(
      *animated_image_view_,
      GetPaddingForAnimationJitter(animation_jitter_calculator_.config()));
  AmbientAnimationAttributionTransformer::TransformTextBox(
      *animated_image_view_);
  // When the device is in portrait mode, the landscape version of the
  // animation is currently being used. The tree shadow in "feel the breeze"
  // gets cut off at the top when doing this, making it look strange. UX
  // decision is to just omit the tree shadow in portrait mode. If/when
  // portrait versions of the animation are made, this logic can be removed.
  if (static_resources_->GetUiSettings().theme() ==
      personalization_app::mojom::AmbientTheme::kFeelTheBreeze) {
    bool tree_shadow_toggled = animation_photo_provider_.ToggleStaticImageAsset(
        cc::HashSkottieResourceId(ambient::resources::kTreeShadowAssetId),
        /*enabled=*/content_bounds.width() >= content_bounds.height());
    DCHECK(tree_shadow_toggled);
  }
  DVLOG(4)
      << "View bounds available. Resized animation with native size "
      << animated_image_view_->animated_image()->GetOriginalSize().ToString()
      << " from " << previous_animation_bounds.ToString() << " to "
      << animated_image_view_->GetImageBounds().ToString();
  StartPlayingAnimation();
  if (!throughput_tracker_restart_timer_.IsRunning()) {
    RestartThroughputTracking();
    throughput_tracker_restart_timer_.Start(
        FROM_HERE, kThroughputTrackerRestartPeriod, this,
        &AmbientAnimationView::RestartThroughputTracking);
  }
}

void AmbientAnimationView::OnViewAddedToWidget(View* observed_view) {
  DCHECK_EQ(observed_view, static_cast<View*>(animated_image_view_));
  DCHECK(observed_view->GetWidget());
  // Frame throttling requires a window with a valid FrameSinkId. Keep searching
  // up the window tree until one is found.
  auto* window_to_throttle = animated_image_view_->GetWidget()->GetNativeView();
  while (!window_to_throttle->GetFrameSinkId().is_valid()) {
    window_to_throttle = window_to_throttle->parent();
    DCHECK(window_to_throttle) << "Search for window to throttle failed";
  }
  frame_rate_controller_->AddWindowToThrottle(
      window_to_throttle, animated_image_view_->animated_image());
}

SkColor AmbientAnimationView::GetTimeTemperatureFontColor() {
  return gfx::kGoogleGrey900;
}

MediaStringView::Settings AmbientAnimationView::GetSettings() {
  return MediaStringView::Settings(
      {/*icon_light_mode_color=*/gfx::kGoogleGrey600,
       /*icon_dark_mode_color=*/gfx::kGoogleGrey500,
       /*text_light_mode_color=*/gfx::kGoogleGrey600,
       /*text_dark_mode_color=*/gfx::kGoogleGrey500,
       kMediaStringTextElevation});
}

void AmbientAnimationView::StartPlayingAnimation() {
  // There should only be one active AmbientAnimationPlayer at any given time,
  // otherwise multiple active players can lead to confusing simultaneous state
  // changes. So destroy the existing player first before creating a new one.
  animation_player_.reset();
  // |animated_image_view_| is owned by the base |View| class and outlives the
  // |animation_player_|, so it's safe to pass a raw ptr here.
  animation_player_ = std::make_unique<AmbientAnimationPlayer>(
      animated_image_view_, progress_tracker_.get());
  view_delegate_->NotifyObserversMarkerHit(
      AmbientPhotoConfig::Marker::kUiStartRendering);
  last_jitter_timestamp_ = base::TimeTicks::Now();
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
  throughput_tracker_->Start(
      base::BindOnce(&OnCompositorThroughputReported,
                     /*logging_start_time=*/base::TimeTicks::Now(),
                     static_resources_->GetUiSettings()));
}

void AmbientAnimationView::ApplyJitter() {
  gfx::Vector2d jitter = animation_jitter_calculator_.Calculate();
  DVLOG(4) << "Applying jitter to animation: " << jitter.ToString();
  // Sharing the same jitter between the animation and other peripheral content
  // keeps the spacing between features consistent.
  animated_image_view_->SetAdditionalTranslation(jitter);
  glanceable_info_container_->SetBorder(
      CreateGlanceableInfoBorder(add_glanceable_info_text_shadow_, jitter));
  media_string_container_->SetBorder(CreateMediaStringBorder(jitter));
}

JitterCalculator* AmbientAnimationView::GetJitterCalculatorForTesting() {
  return &animation_jitter_calculator_;
}

BEGIN_METADATA(AmbientAnimationView)
END_METADATA

}  // namespace ash
