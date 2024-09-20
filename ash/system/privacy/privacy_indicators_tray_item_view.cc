// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <memory>
#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr auto kPrivacyIndicatorsViewPadding = gfx::Insets::VH(4, 8);
const int kPrivacyIndicatorsViewSpacing = 2;
const int kPrivacyIndicatorsIconSize = 16;
const int kPrivacyIndicatorsViewExpandedShorterSideSize = 24;
const int kPrivacyIndicatorsViewExpandedLongerSideSize = 50;
const int kPrivacyIndicatorsViewExpandedWithScreenShareSize = 68;
const int kPrivacyIndicatorsViewSize = 8;

constexpr auto kDwellInExpandDuration = base::Milliseconds(3000);
constexpr auto kShorterSizeShrinkAnimationDelay =
    kDwellInExpandDuration + base::Milliseconds(133);
constexpr auto kSizeChangeAnimationDuration = base::Milliseconds(333);
constexpr auto kExpandAnimationDuration = base::Milliseconds(400);
constexpr auto kIconFadeInDelayDuration = base::Milliseconds(83);
constexpr auto kCameraIconFadeInDuration = base::Milliseconds(233);
constexpr auto kMicAndScreenshareFadeInDuration = base::Milliseconds(116);

void StartAnimation(gfx::LinearAnimation* animation) {
  if (!animation)
    return;

  // Stop any ongoing animation.
  animation->End();

  animation->Start();
}

void StartRecordAnimationSmoothness(
    views::Widget* widget,
    std::optional<ui::ThroughputTracker>& tracker) {
  // `widget` may not exist in tests.
  if (!widget)
    return;

  tracker.emplace(widget->GetCompositor()->RequestNewThroughputTracker());
  tracker->Start(ash::metrics_util::ForSmoothnessV3(
      base::BindRepeating([](int smoothness) {
        base::UmaHistogramPercentage(
            "Ash.PrivacyIndicators.AnimationSmoothness", smoothness);
      })));
}

void StartReportLayerAnimationSmoothness(
    const std::string& animation_histogram_name,
    int smoothness) {
  // Only record animation smoothness if `animation_histogram_name` is given.
  if (animation_histogram_name.empty())
    return;
  base::UmaHistogramPercentage(animation_histogram_name, smoothness);
}

void FadeInView(views::View* view,
                base::TimeDelta duration,
                const std::string& animation_histogram_name) {
  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  // Stop any ongoing animation.
  if (view->layer()->GetAnimator()->is_animating())
    view->layer()->GetAnimator()->StopAnimating();

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          &StartReportLayerAnimationSmoothness, animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(view, 0.0f)
      .At(kIconFadeInDelayDuration)
      .SetDuration(duration)
      .SetOpacity(view, 1.0f);
}

// Returns true if the widget is in the primary display.
bool IsInPrimaryDisplay(views::Widget* widget) {
  if (!widget) {
    return false;
  }

  auto* screen = display::Screen::GetScreen();
  return screen->GetDisplayNearestWindow(widget->GetNativeWindow()) ==
         screen->GetPrimaryDisplay();
}

}  // namespace

PrivacyIndicatorsTrayItemView::PrivacyIndicatorsTrayItemView(Shelf* shelf)
    : TrayItemView(shelf),
      expand_animation_(std::make_unique<gfx::LinearAnimation>(
          kExpandAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)),
      longer_side_shrink_animation_(std::make_unique<gfx::LinearAnimation>(
          kSizeChangeAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)),
      shorter_side_shrink_animation_(std::make_unique<gfx::LinearAnimation>(
          kSizeChangeAnimationDuration,
          gfx::LinearAnimation::kDefaultFrameRate,
          this)) {
  SetVisible(false);

  auto container_view = std::make_unique<views::View>();
  layout_manager_ =
      container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                                  views::BoxLayout::Orientation::kVertical),
          kPrivacyIndicatorsViewPadding, kPrivacyIndicatorsViewSpacing));
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Set up a solid color layer to paint the background color, then add a layer
  // to each child so that they are visible and can perform layer animation.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kPrivacyIndicatorsViewExpandedShorterSideSize / 2});
  layer()->SetIsFastRoundedCorner(true);

  auto add_icon_to_container = [&container_view]() {
    auto icon = std::make_unique<views::ImageView>();
    icon->SetPaintToLayer();
    icon->layer()->SetFillsBoundsOpaquely(false);
    icon->SetVisible(false);
    return container_view->AddChildView(std::move(icon));
  };

  camera_icon_ = add_icon_to_container();
  microphone_icon_ = add_icon_to_container();
  screen_share_icon_ = add_icon_to_container();

  AddChildView(std::move(container_view));

  UpdateIcons();
  TooltipTextChanged();

  UpdateVisibility();

  Shell::Get()->session_controller()->AddObserver(this);
}

PrivacyIndicatorsTrayItemView::~PrivacyIndicatorsTrayItemView() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void PrivacyIndicatorsTrayItemView::OnCameraAndMicrophoneAccessStateChanged(
    bool is_camera_used,
    bool is_microphone_used,
    bool is_new_app,
    bool was_camera_in_use,
    bool was_microphone_in_use) {
  UpdateVisibility();
  if (!GetVisible())
    return;

  auto* controller = PrivacyIndicatorsController::Get();

  // We only want to perform the animation and show the camera/microphone icons
  // in these cases:
  // * If this is a new app accessing camera/microphone, the icons will be shown
  //   according to the access state of that particular app.
  // * If this is an old app, but a new sensor is being accessed (was not in
  //   used before), we will show the icons of the sensors in which that
  //   particular app is accessing.
  if (!is_new_app && !(controller->IsCameraUsed() && !was_camera_in_use) &&
      !(controller->IsMicrophoneUsed() && !was_microphone_in_use)) {
    return;
  }

  // We show the icons based on the access state of this current app.
  camera_icon_->SetVisible(is_camera_used);
  microphone_icon_->SetVisible(is_microphone_used);

  TooltipTextChanged();
  RecordPrivacyIndicatorsType();

  // Perform animation if either one of the icon is visible.
  if (camera_icon_->GetVisible() || microphone_icon_->GetVisible()) {
    PerformAnimation();
  }
}

void PrivacyIndicatorsTrayItemView::UpdateScreenShareStatus(
    bool is_screen_sharing) {
  if (is_screen_sharing_ == is_screen_sharing)
    return;
  is_screen_sharing_ = is_screen_sharing;

  UpdateVisibility();
  if (!GetVisible())
    return;

  screen_share_icon_->SetVisible(is_screen_sharing_);
  TooltipTextChanged();
  RecordPrivacyIndicatorsType();

  // Perform animation whever screen is start sharing.
  if (is_screen_sharing_) {
    PerformAnimation();
  }
}

void PrivacyIndicatorsTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  layout_manager_->SetOrientation(
      shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                              views::BoxLayout::Orientation::kVertical));
  UpdateBoundsInset();
}

std::u16string PrivacyIndicatorsTrayItemView::GetTooltipText(
    const gfx::Point& point) const {
  auto* controller = PrivacyIndicatorsController::Get();
  auto cam_and_mic_status = std::u16string();
  if (controller->IsCameraUsed() && controller->IsMicrophoneUsed()) {
    cam_and_mic_status =
        l10n_util::GetStringUTF16(IDS_PRIVACY_INDICATORS_STATUS_CAMERA_AND_MIC);
  } else if (controller->IsCameraUsed()) {
    cam_and_mic_status =
        l10n_util::GetStringUTF16(IDS_PRIVACY_INDICATORS_STATUS_CAMERA);
  } else if (controller->IsMicrophoneUsed()) {
    cam_and_mic_status =
        l10n_util::GetStringUTF16(IDS_PRIVACY_INDICATORS_STATUS_MIC);
  }

  auto screen_share_status =
      is_screen_sharing_
          ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE)
          : std::u16string();

  if (cam_and_mic_status.empty())
    return screen_share_status;

  if (screen_share_status.empty())
    return cam_and_mic_status;

  return l10n_util::GetStringFUTF16(IDS_PRIVACY_INDICATORS_VIEW_TOOLTIP,
                                    {cam_and_mic_status, screen_share_status},
                                    /*offsets=*/nullptr);
}

void PrivacyIndicatorsTrayItemView::UpdateVisibility() {
  // We only hide the view when nothing is in use.
  const bool visible = PrivacyIndicatorsController::Get()->IsCameraUsed() ||
                       PrivacyIndicatorsController::Get()->IsMicrophoneUsed() ||
                       is_screen_sharing_;

  if (GetVisible() == visible) {
    return;
  }

  SetVisible(visible);

  if (!visible) {
    if (IsInPrimaryDisplay(GetWidget())) {
      base::UmaHistogramLongTimes(
          "Ash.PrivacyIndicators.IndicatorShowsDuration",
          base::Time::Now() - start_showing_time_);
    }
    return;
  }

  // Only record this metric on primary screen.
  if (IsInPrimaryDisplay(GetWidget())) {
    start_showing_time_ = base::Time::Now();
  }

  ++count_visible_per_session_;
}

void PrivacyIndicatorsTrayItemView::PerformVisibilityAnimation(bool visible) {
  // This view will not perform `TrayItemView`'s visibility animation since it
  // has its own animation. We need to create our own function to trigger the
  // animation rather than overriding this to avoid triggering overlapping
  // animations when visibility changes.
}

void PrivacyIndicatorsTrayItemView::HandleLocaleChange() {
  TooltipTextChanged();
}

gfx::Size PrivacyIndicatorsTrayItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int shorter_side;
  int longer_side;

  switch (animation_state_) {
    case AnimationState::kIdle:
      return gfx::Size(kPrivacyIndicatorsViewSize, kPrivacyIndicatorsViewSize);
    case AnimationState::kExpand:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side =
          GetLongerSideLengthInExpandedMode() *
          gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100,
                                     expand_animation_->GetCurrentValue());
      break;
    case AnimationState::kDwellInExpand:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side = GetLongerSideLengthInExpandedMode();
      break;
    case AnimationState::kOnlyLongerSideShrink:
      shorter_side = kPrivacyIndicatorsViewExpandedShorterSideSize;
      longer_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/true);
      break;
    case AnimationState::kBothSideShrink:
      shorter_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/false);
      longer_side =
          CalculateSizeDuringShrinkAnimation(/*for_longer_side=*/true);
      break;
  }
  // `GetWidget()` might be null in unit tests.
  auto* shelf = GetWidget() ? Shelf::ForWindow(GetWidget()->GetNativeWindow())
                            : Shell::GetPrimaryRootWindowController()->shelf();
  // The view is rotated 90 degree in side shelf.
  return shelf->PrimaryAxisValue(gfx::Size(longer_side, shorter_side),
                                 gfx::Size(shorter_side, longer_side));
}

void PrivacyIndicatorsTrayItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIcons();

  layer()->SetColor(
      GetColorProvider()->GetColor(ui::kColorAshPrivacyIndicatorsBackground));
}

void PrivacyIndicatorsTrayItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateBoundsInset();
}

views::View* PrivacyIndicatorsTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

void PrivacyIndicatorsTrayItemView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == expand_animation_.get()) {
    DCHECK_EQ(animation_state_, AnimationState::kExpand);
  } else if (animation == longer_side_shrink_animation_.get() &&
             !shorter_side_shrink_animation_->is_animating()) {
    animation_state_ = AnimationState::kOnlyLongerSideShrink;
  } else {
    animation_state_ = AnimationState::kBothSideShrink;
  }

  PreferredSizeChanged();
}

void PrivacyIndicatorsTrayItemView::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation_state_ == AnimationState::kExpand) {
    // Start kDwellInExpand when `expand_animation_` just finished.
    animation_state_ = kDwellInExpand;
    PreferredSizeChanged();

    longer_side_shrink_delay_timer_.Start(
        FROM_HERE, kDwellInExpandDuration,
        base::BindOnce(&StartAnimation, longer_side_shrink_animation_.get()));

    shorter_side_shrink_delay_timer_.Start(
        FROM_HERE, kShorterSizeShrinkAnimationDelay,
        base::BindOnce(&StartAnimation, shorter_side_shrink_animation_.get()));
  }

  // `shorter_side_shrink_animation_` should be the last one that is running, so
  // switch the state back to kIdle when it ends.
  if (animation == shorter_side_shrink_animation_.get()) {
    animation_state_ = AnimationState::kIdle;

    // Hide all the icons at the end since we only want to show a green dot.
    camera_icon_->SetVisible(false);
    microphone_icon_->SetVisible(false);
    screen_share_icon_->SetVisible(false);

    if (throughput_tracker_) {
      // Reset `throughput_tracker_` to reset animation metrics recording.
      throughput_tracker_->Stop();
      throughput_tracker_.reset();
    }
  }

  UpdateBoundsInset();
}

void PrivacyIndicatorsTrayItemView::AnimationCanceled(
    const gfx::Animation* animation) {
  // Finish all animations if one is canceled.
  EndAllAnimations();

  UpdateBoundsInset();
}

void PrivacyIndicatorsTrayItemView::ImmediatelyUpdateVisibility() {
  // Normally there is work to do here, but this view implements custom
  // visibility animations that do not adhere to the `TrayItemView` animations
  // contract. See b/283493232 for details.
}

void PrivacyIndicatorsTrayItemView::PerformAnimation() {
  // End all previous animations before starting a new sequence of animations.
  EndAllAnimations();

  // Start a multi-part animation:
  // 1. kExpand: Expands to the fully expanded state, showing all icons.
  // 2. kDwellInExpand: Then dwells at this size for `kDwellInExpandDuration`.
  // 3. kOnlyLongerSideShrink: After that, collapses the long side first.
  // 4. kBothSideShrink: Before the long side shrinks completely, collapses the
  //    short side to the final size (a green dot).
  animation_state_ = AnimationState::kExpand;
  expand_animation_->Start();
  StartRecordAnimationSmoothness(GetWidget(), throughput_tracker_);

  // At the same time, fade in icons.
  if (camera_icon_->GetVisible()) {
    FadeInView(camera_icon_, kCameraIconFadeInDuration,
               "Ash.PrivacyIndicators.CameraIcon.AnimationSmoothness");
  }
  if (microphone_icon_->GetVisible()) {
    FadeInView(microphone_icon_, kMicAndScreenshareFadeInDuration,
               "Ash.PrivacyIndicators.MicrophoneIcon.AnimationSmoothness");
  }
  if (screen_share_icon_->GetVisible()) {
    FadeInView(screen_share_icon_, kMicAndScreenshareFadeInDuration,
               "Ash.PrivacyIndicators.ScreenshareIcon.AnimationSmoothness");
  }
}

void PrivacyIndicatorsTrayItemView::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (count_visible_per_session_ == 0)
    return;

  // Only record this metric on primary screen.
  if (!IsInPrimaryDisplay(GetWidget())) {
    return;
  }

  base::UmaHistogramCounts100("Ash.PrivacyIndicators.NumberOfShowsPerSession",
                              count_visible_per_session_);
  count_visible_per_session_ = 0;
}

void PrivacyIndicatorsTrayItemView::UpdateIcons() {
  const ui::ColorId icon_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? cros_tokens::kCrosSysInverseOnSurface
          : static_cast<ui::ColorId>(kColorAshButtonIconColorPrimary);

  camera_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kPrivacyIndicatorsCameraIcon, icon_color_id, kPrivacyIndicatorsIconSize));
  microphone_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kPrivacyIndicatorsMicrophoneIcon, icon_color_id,
      kPrivacyIndicatorsIconSize));
  screen_share_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kPrivacyIndicatorsScreenShareIcon, icon_color_id,
      kPrivacyIndicatorsIconSize));
}

void PrivacyIndicatorsTrayItemView::UpdateBoundsInset() {
  gfx::Rect bounds = GetLocalBounds();

  // `GetWidget()` might be null in unit tests.
  auto* shelf = GetWidget() ? Shelf::ForWindow(GetWidget()->GetNativeWindow())
                            : Shell::GetPrimaryRootWindowController()->shelf();

  // We set the bounds inset based on the shorter side of the view (the shorter
  // size changes based on shelf alignment).
  int shorter_side_inset = shelf->PrimaryAxisValue(height(), width()) -
                           shelf->PrimaryAxisValue(GetPreferredSize().height(),
                                                   GetPreferredSize().width());
  bounds.Inset(
      shelf->PrimaryAxisValue(gfx::Insets::VH(shorter_side_inset / 2, 0),
                              gfx::Insets::VH(0, shorter_side_inset / 2)));
  layer()->SetClipRect(bounds);
}

int PrivacyIndicatorsTrayItemView::CalculateSizeDuringShrinkAnimation(
    bool for_longer_side) const {
  auto* animation = for_longer_side ? longer_side_shrink_animation_.get()
                                    : shorter_side_shrink_animation_.get();

  double animation_value = gfx::Tween::CalculateValue(
      gfx::Tween::ACCEL_20_DECEL_100, animation->GetCurrentValue());
  int begin_size = for_longer_side
                       ? GetLongerSideLengthInExpandedMode()
                       : kPrivacyIndicatorsViewExpandedShorterSideSize;

  // The size shrink from `begin_size` to kPrivacyIndicatorsViewSize when
  // `animation_value` goes from 0 to 1, and here is the calculation for it.
  return begin_size -
         (begin_size - kPrivacyIndicatorsViewSize) * animation_value;
}

int PrivacyIndicatorsTrayItemView::GetLongerSideLengthInExpandedMode() const {
  // If all three icons are visible, the view should be longer.
  return PrivacyIndicatorsController::Get()->IsCameraUsed() &&
                 PrivacyIndicatorsController::Get()->IsMicrophoneUsed() &&
                 is_screen_sharing_
             ? kPrivacyIndicatorsViewExpandedWithScreenShareSize
             : kPrivacyIndicatorsViewExpandedLongerSideSize;
}

void PrivacyIndicatorsTrayItemView::EndAllAnimations() {
  shorter_side_shrink_animation_->End();
  longer_side_shrink_animation_->End();
  expand_animation_->End();
  animation_state_ = AnimationState::kIdle;

  if (throughput_tracker_) {
    // Reset `throughput_tracker_` to reset animation metrics recording.
    throughput_tracker_->Stop();
    throughput_tracker_.reset();
  }
}

void PrivacyIndicatorsTrayItemView::RecordPrivacyIndicatorsType() {
  auto* controller = PrivacyIndicatorsController::Get();
  const bool is_camera_used = controller->IsCameraUsed();
  const bool is_microphone_used = controller->IsMicrophoneUsed();

  int camera_used = is_camera_used ? static_cast<int>(Type::kCamera) : 0;
  int microphone_used =
      is_microphone_used ? static_cast<int>(Type::kMicrophone) : 0;
  int screen_sharing =
      is_screen_sharing_ ? static_cast<int>(Type::kScreenSharing) : 0;

  base::UmaHistogramEnumeration(
      "Ash.PrivacyIndicators.ShowType",
      static_cast<Type>(camera_used | microphone_used | screen_sharing));

  if (is_camera_used) {
    base::UmaHistogramCounts100(
        "Ash.PrivacyIndicators.NumberOfAppsAccessingCamera",
        controller->apps_using_camera().size());
  }

  if (is_microphone_used) {
    base::UmaHistogramCounts100(
        "Ash.PrivacyIndicators.NumberOfAppsAccessingMicrophone",
        controller->apps_using_microphone().size());
  }
}

BEGIN_METADATA(PrivacyIndicatorsTrayItemView)
END_METADATA

}  // namespace ash
