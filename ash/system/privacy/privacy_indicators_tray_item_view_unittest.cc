// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

const int kPrivacyIndicatorsViewExpandedShorterSideSize = 24;
const int kPrivacyIndicatorsViewExpandedLongerSideSize = 50;
const int kPrivacyIndicatorsViewSize = 8;

constexpr char kPrivacyIndicatorsShowTypeHistogramName[] =
    "Ash.PrivacyIndicators.ShowType";
constexpr char kPrivacyIndicatorsShowPerSessionHistogramName[] =
    "Ash.PrivacyIndicators.NumberOfShowsPerSession";
constexpr char kCountAppsAccessCameraHistogramName[] =
    "Ash.PrivacyIndicators.NumberOfAppsAccessingCamera";
constexpr char kCountAppsAccessMicrophoneHistogramName[] =
    "Ash.PrivacyIndicators.NumberOfAppsAccessingMicrophone";
constexpr char kVisibilityDurationHistogramName[] =
    "Ash.PrivacyIndicators.IndicatorShowsDuration";

// Update the state of accessing camera and microphone using the
// `PrivacyIndicatorsController`.
void UpdateCameraAndMicrophoneUsage(bool is_camera_used,
                                    bool is_microphone_used,
                                    const std::string& app_id = "app_id") {
  ash::PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, /*app_name=*/u"App Name", is_camera_used, is_microphone_used,
      base::MakeRefCounted<ash::PrivacyIndicatorsNotificationDelegate>(),
      ash::PrivacyIndicatorsSource::kApps);
}

// Get the expected size in expand animation, given the animation value.
int GetExpectedSizeInExpandAnimation(double progress) {
  return kPrivacyIndicatorsViewExpandedLongerSideSize *
         gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100, progress);
}

// Get the expected size in shrink animation, given the animation value.
int GetExpectedSizeInShrinkAnimation(bool for_longer_side, double progress) {
  double animation_value =
      gfx::Tween::CalculateValue(gfx::Tween::ACCEL_20_DECEL_100, progress);
  int begin_size = for_longer_side
                       ? kPrivacyIndicatorsViewExpandedLongerSideSize
                       : kPrivacyIndicatorsViewExpandedShorterSideSize;
  return begin_size -
         (begin_size - kPrivacyIndicatorsViewSize) * animation_value;
}

// Get the expected tooltip text, given the string for camera/mic access and
// screen share.
std::u16string GetExpectedTooltipText(std::u16string cam_mic_status,
                                      std::u16string screen_share_status) {
  if (cam_mic_status.empty()) {
    return screen_share_status;
  }

  if (screen_share_status.empty()) {
    return cam_mic_status;
  }

  return l10n_util::GetStringFUTF16(IDS_PRIVACY_INDICATORS_VIEW_TOOLTIP,
                                    {cam_mic_status, screen_share_status},
                                    /*offsets=*/nullptr);
}

}  // namespace

namespace ash {

class PrivacyIndicatorsTrayItemViewTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrivacyIndicatorsTrayItemViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  PrivacyIndicatorsTrayItemViewTest(const PrivacyIndicatorsTrayItemViewTest&) =
      delete;
  PrivacyIndicatorsTrayItemViewTest& operator=(
      const PrivacyIndicatorsTrayItemViewTest&) = delete;
  ~PrivacyIndicatorsTrayItemViewTest() override = default;

  std::u16string GetTooltipText() {
    return privacy_indicators_view()->GetTooltipText(gfx::Point());
  }

  views::BoxLayout* GetLayoutManager(
      PrivacyIndicatorsTrayItemView* privacy_indicators_view) {
    return privacy_indicators_view->layout_manager_;
  }

  void AnimateToValue(gfx::LinearAnimation* animation, double animation_value) {
    EXPECT_TRUE(animation->is_animating());
    animation->SetCurrentValue(animation_value);
    privacy_indicators_view()->AnimationProgressed(animation);
  }

  // Set `privacy_indicators_view()` to be visible and perform animation.
  void SetViewVisibleWithAnimation() {
    privacy_indicators_view()->SetVisible(true);
    privacy_indicators_view()->PerformAnimation();
  }

  // Simulates completing the animation.
  void SimulateAnimationEnded() {
    privacy_indicators_view()->AnimationEnded(
        privacy_indicators_view()->shorter_side_shrink_animation_.get());
  }

 protected:
  PrivacyIndicatorsTrayItemView* privacy_indicators_view() const {
    return Shell::GetPrimaryRootWindowController()
        ->GetStatusAreaWidget()
        ->notification_center_tray()
        ->privacy_indicators_view();
  }

  PrivacyIndicatorsTrayItemView* GetSecondaryDisplayPrivacyIndicatorsView()
      const {
    auto* status_area_widget =
        Shell::GetRootWindowControllerWithDisplayId(GetSecondaryDisplay().id())
            ->GetStatusAreaWidget();

    return status_area_widget->notification_center_tray()
        ->privacy_indicators_view();
  }

  views::ImageView* camera_icon() {
    return privacy_indicators_view()->camera_icon_;
  }
  views::ImageView* microphone_icon() {
    return privacy_indicators_view()->microphone_icon_;
  }
  views::ImageView* screen_share_icon() {
    return privacy_indicators_view()->screen_share_icon_;
  }

  gfx::LinearAnimation* expand_animation() {
    return privacy_indicators_view()->expand_animation_.get();
  }

  PrivacyIndicatorsTrayItemView::AnimationState animation_state() {
    return privacy_indicators_view()->animation_state_;
  }

  gfx::LinearAnimation* longer_side_shrink_animation() {
    return privacy_indicators_view()->longer_side_shrink_animation_.get();
  }

  gfx::LinearAnimation* shorter_side_shrink_animation() {
    return privacy_indicators_view()->shorter_side_shrink_animation_.get();
  }
};

TEST_F(PrivacyIndicatorsTrayItemViewTest, IconsVisibility) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, IconsVisibilityAfterAnimation) {
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  // No icons shown after the animation.
  SimulateAnimationEnded();
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  // Since there's no new sensor and new media stream added, no icons should be
  // visible and animation should not be triggered.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  // New sensor is accessed (microphone), so we show all icons accessing that
  // particular app. Animation should start.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  SimulateAnimationEnded();

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);

  // New app accessed, show the indicator according to that app.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true, /*app_id=*/"app_id2");
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  SimulateAnimationEnded();

  // Updates the previous app. However, since no new sensor is accessed
  // (microphone is already accessed by the second app), the indicator should
  // remain the same with no animation.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  ASSERT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, ScreenShareIconsVisibility) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(screen_share_icon()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  // Test screen share showing up with other icons.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true);
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());
  EXPECT_TRUE(screen_share_icon()->GetVisible());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());
  EXPECT_FALSE(screen_share_icon()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, TooltipText) {
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/std::u16string(),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                       IDS_PRIVACY_INDICATORS_STATUS_CAMERA),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                       IDS_PRIVACY_INDICATORS_STATUS_MIC),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  EXPECT_EQ(
      GetExpectedTooltipText(/*cam_mic_status=*/l10n_util::GetStringUTF16(
                                 IDS_PRIVACY_INDICATORS_STATUS_CAMERA_AND_MIC),
                             /*screen_share_status=*/std::u16string()),
      GetTooltipText());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  EXPECT_EQ(GetExpectedTooltipText(/*cam_mic_status=*/std::u16string(),
                                   /*screen_share_status=*/std::u16string()),
            GetTooltipText());

  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);
  EXPECT_EQ(GetExpectedTooltipText(
                /*cam_mic_status=*/std::u16string(),
                /*screen_share_status=*/l10n_util::GetStringUTF16(
                    IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE)),
            GetTooltipText());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, ShelfAlignmentChanged) {
  auto* view = privacy_indicators_view();
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(views::BoxLayout::Orientation::kVertical,
            GetLayoutManager(view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_EQ(views::BoxLayout::Orientation::kHorizontal,
            GetLayoutManager(view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(views::BoxLayout::Orientation::kVertical,
            GetLayoutManager(view)->GetOrientation());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottomLocked);
  EXPECT_EQ(views::BoxLayout::Orientation::kHorizontal,
            GetLayoutManager(view)->GetOrientation());
}

// Tests that the privacy indicators tray item is visible when its show
// animation finishes running after the notification center tray has been
// hidden. This test was added in response to b/283091001.
TEST_F(PrivacyIndicatorsTrayItemViewTest,
       ShowAnimationAfterNotificationCenterTrayHidden) {
  // Verify that the privacy indicators are hidden and not animating.
  ASSERT_FALSE(privacy_indicators_view()->GetVisible());
  ASSERT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  // Show the notification center tray.
  GetPrimaryNotificationCenterTray()->SetVisiblePreferred(true);
  ASSERT_TRUE(GetPrimaryNotificationCenterTray()->IsDrawn());
  ASSERT_EQ(GetPrimaryNotificationCenterTray()->layer()->opacity(), 1.0f);

  // Hide the notification center tray.
  GetPrimaryNotificationCenterTray()->SetVisiblePreferred(false);
  ASSERT_FALSE(GetPrimaryNotificationCenterTray()->IsDrawn());
  ASSERT_EQ(GetPrimaryNotificationCenterTray()->layer()->opacity(), 0.0f);

  // Show privacy indicators and let the animation end.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true);
  SimulateAnimationEnded();
  ASSERT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  // Verify that the privacy indicators tray item is visible.
  EXPECT_TRUE(privacy_indicators_view()->IsDrawn());
  EXPECT_EQ(privacy_indicators_view()->layer()->opacity(), 1.0f);
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, VisibilityAnimation) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  expand_animation()->Start();
  AnimateToValue(expand_animation(), progress);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(GetExpectedSizeInExpandAnimation(progress),
            privacy_indicators_view()->GetPreferredSize().width());

  expand_animation()->End();

  // When expand animation ends, the view will be in `kDwellInExpand` state.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kDwellInExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedLongerSideSize,
            privacy_indicators_view()->GetPreferredSize().width());

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  EXPECT_EQ(
      PrivacyIndicatorsTrayItemView::AnimationState::kOnlyLongerSideShrink,
      animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().width());

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kBothSideShrink,
            animation_state());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/false, progress),
      privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().width());

  longer_side_shrink_animation()->End();
  shorter_side_shrink_animation()->End();

  // When finish, the view should have the size of a dot.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().height());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().width());

  // All icon should not be visible.
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());
  EXPECT_FALSE(screen_share_icon()->GetVisible());
}

// Same test as above, but with the side shelf (the longer and shorter side will
// be flipped).
TEST_F(PrivacyIndicatorsTrayItemViewTest, SideShelfVisibilityAnimation) {
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());

  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  expand_animation()->Start();
  AnimateToValue(expand_animation(), progress);
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(GetExpectedSizeInExpandAnimation(progress),
            privacy_indicators_view()->GetPreferredSize().height());

  expand_animation()->End();

  // When expand animation ends, the view will be in `kDwellInExpand` state.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kDwellInExpand,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedLongerSideSize,
            privacy_indicators_view()->GetPreferredSize().height());

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  EXPECT_EQ(
      PrivacyIndicatorsTrayItemView::AnimationState::kOnlyLongerSideShrink,
      animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewExpandedShorterSideSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().height());

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kBothSideShrink,
            animation_state());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/false, progress),
      privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(
      GetExpectedSizeInShrinkAnimation(/*for_longer_side=*/true, progress),
      privacy_indicators_view()->GetPreferredSize().height());

  longer_side_shrink_animation()->End();
  shorter_side_shrink_animation()->End();

  // When finish, the view should have the size of a dot.
  EXPECT_EQ(PrivacyIndicatorsTrayItemView::AnimationState::kIdle,
            animation_state());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().width());
  EXPECT_EQ(kPrivacyIndicatorsViewSize,
            privacy_indicators_view()->GetPreferredSize().height());

  // All icon should not be visible.
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());
  EXPECT_FALSE(screen_share_icon()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, StateChangeDuringAnimation) {
  SetViewVisibleWithAnimation();
  double progress = 0.5;

  // Firstly, expand animation will be performed.
  expand_animation()->Start();
  AnimateToValue(expand_animation(), progress);

  // Update state in mid animation, shouldn't crash anything.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);

  expand_animation()->End();

  // After that shrink animations will be started.
  longer_side_shrink_animation()->Start();
  AnimateToValue(longer_side_shrink_animation(), progress);

  // Update the state again, no crash expected.
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/true);

  shorter_side_shrink_animation()->Start();
  AnimateToValue(shorter_side_shrink_animation(), progress);

  // The view should become invisible immediately after setting these states.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  privacy_indicators_view()->UpdateScreenShareStatus(
      /*is_screen_sharing=*/false);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  // Clean up.
  longer_side_shrink_animation()->End();
  shorter_side_shrink_animation()->End();
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, MultipleAppsAccess) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  // When a new app accessing mic/cam, we will show the icons according to the
  // access state of that particular app.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false, /*app_id=*/"app_id2");
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/true, /*app_id=*/"app_id3");
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_FALSE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  // Indicator should still show when removing 1 and 2 app(s).
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, /*app_id=*/"app_id2");
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, /*app_id=*/"app_id3");
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());

  // Indicator should hide when removing all apps.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest,
       HidingDelayTimerEnabledWithMultipleAppsAccess) {
  EXPECT_FALSE(privacy_indicators_view()->GetVisible());

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_FALSE(microphone_icon()->GetVisible());

  // When a new app accessing cam, we will show the icons according to the
  // access state of that particular app.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, /*app_id=*/"app_id2");
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  // Indicator should still show app 2 after removing app 2 not app 1.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false, /*app_id=*/"app_id2");
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());

  // When the app retries, the visibility remains the same.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, /*app_id=*/"app_id2");
  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(camera_icon()->GetVisible());
  EXPECT_TRUE(microphone_icon()->GetVisible());
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, RecordShowTypeMetrics) {
  auto check_histogram_record = [](bool is_camera_used, bool is_microphone_used,
                                   bool is_screen_sharing,
                                   PrivacyIndicatorsTrayItemView* view,
                                   PrivacyIndicatorsTrayItemView::Type type) {
    base::HistogramTester histograms;
    UpdateCameraAndMicrophoneUsage(is_camera_used, is_microphone_used);
    view->UpdateScreenShareStatus(is_screen_sharing);
    histograms.ExpectBucketCount(kPrivacyIndicatorsShowTypeHistogramName, type,
                                 1);
  };

  check_histogram_record(/*is_camera_used=*/true, /*is_microphone_used=*/false,
                         /*is_screen_sharing=*/false, privacy_indicators_view(),
                         PrivacyIndicatorsTrayItemView::Type::kCamera);

  check_histogram_record(/*is_camera_used=*/false, /*is_microphone_used=*/true,
                         /*is_screen_sharing=*/false, privacy_indicators_view(),
                         PrivacyIndicatorsTrayItemView::Type::kMicrophone);

  check_histogram_record(/*is_camera_used=*/false, /*is_microphone_used=*/false,
                         /*is_screen_sharing=*/true, privacy_indicators_view(),
                         PrivacyIndicatorsTrayItemView::Type::kScreenSharing);

  check_histogram_record(
      /*is_camera_used=*/true, /*is_microphone_used=*/true,
      /*is_screen_sharing=*/false, privacy_indicators_view(),
      PrivacyIndicatorsTrayItemView::Type::kCameraMicrophone);

  check_histogram_record(
      /*is_camera_used=*/true, /*is_microphone_used=*/false,
      /*is_screen_sharing=*/true, privacy_indicators_view(),
      PrivacyIndicatorsTrayItemView::Type::kCameraScreenSharing);

  check_histogram_record(
      /*is_camera_used=*/false, /*is_microphone_used=*/true,
      /*is_screen_sharing=*/true, privacy_indicators_view(),
      PrivacyIndicatorsTrayItemView::Type::kMicrophoneScreenSharing);

  check_histogram_record(
      /*is_camera_used=*/true, /*is_microphone_used=*/true,
      /*is_screen_sharing=*/true, privacy_indicators_view(),
      PrivacyIndicatorsTrayItemView::Type::kAllUsed);
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, RecordShowPerSessionMetrics) {
  // Set up 2 displays. Note that only one instance should be recorded for the
  // primary display when session changes.
  UpdateDisplay("100x200,300x400");
  int expected_count = 1;

  // Show the indicator in the given `show_count` number of times.
  auto trigger_show_indicator =
      [](int show_count, base::test::TaskEnvironment* task_environment) {
        // Update the state of camera/microphone access so that the indicators
        // on all displays show, then hide for `show_count` times.
        for (auto i = 0; i < show_count; i++) {
          UpdateCameraAndMicrophoneUsage(/*is_camera_used=*/true,
                                         /*is_microphone_used=*/true);
          UpdateCameraAndMicrophoneUsage(/*is_camera_used=*/false,
                                         /*is_microphone_used=*/false);
          // Fast forward by the minimum duration the privacy indicator should
          // be held.
          task_environment->FastForwardBy(
              ash::PrivacyIndicatorsController::
                  kPrivacyIndicatorsMinimumHoldDuration);
        }
      };

  base::HistogramTester histograms;

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  int expected_sample = 1;
  trigger_show_indicator(expected_sample, task_environment());

  // After session changed, metrics should be recorded.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  histograms.ExpectBucketCount(kPrivacyIndicatorsShowPerSessionHistogramName,
                               expected_sample, expected_count);

  expected_sample = 6;
  trigger_show_indicator(expected_sample, task_environment());

  // After session changed, metrics should be recorded.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  histograms.ExpectBucketCount(kPrivacyIndicatorsShowPerSessionHistogramName,
                               expected_sample, expected_count);

  expected_sample = 10;
  trigger_show_indicator(expected_sample, task_environment());

  // After session changed, metrics should be recorded.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  histograms.ExpectBucketCount(kPrivacyIndicatorsShowPerSessionHistogramName,
                               expected_sample, expected_count);
}

// When multiple apps access camera and microphone, their histograms should
// update accordingly.
TEST_F(PrivacyIndicatorsTrayItemViewTest, RecordAppAccessSimultaneously) {
  base::HistogramTester histograms;

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  histograms.ExpectBucketCount(kCountAppsAccessCameraHistogramName, 1, 1);
  histograms.ExpectBucketCount(kCountAppsAccessMicrophoneHistogramName, 1, 0);

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/true, /*app_id=*/"app_id2");
  histograms.ExpectBucketCount(kCountAppsAccessCameraHistogramName, 2, 1);
  histograms.ExpectBucketCount(kCountAppsAccessMicrophoneHistogramName, 1, 1);

  UpdateCameraAndMicrophoneUsage(/*is_camera_used=*/true,
                                 /*is_microphone_used=*/true,
                                 /*app_id=*/"app_id3");
  histograms.ExpectBucketCount(kCountAppsAccessCameraHistogramName, 3, 1);
  histograms.ExpectBucketCount(kCountAppsAccessMicrophoneHistogramName, 2, 1);
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, RecordVisibilityDuration) {
  // Set up 2 displays. Note that only one instance should be recorded for the
  // primary display.
  UpdateDisplay("100x200,300x400");

  base::HistogramTester histograms;

  auto start_time = base::Time::Now();

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(100));

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  auto expected_sample1 = base::Time::Now() - start_time;
  histograms.ExpectTimeBucketCount(kVisibilityDurationHistogramName,
                                   expected_sample1, 1);

  start_time = base::Time::Now();

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);
  task_environment()->FastForwardBy(base::Minutes(10));

  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/false,
      /*is_microphone_used=*/false);
  // Fast forward by the minimum duration the privacy indicator should be held.
  task_environment()->FastForwardBy(
      ash::PrivacyIndicatorsController::kPrivacyIndicatorsMinimumHoldDuration);
  histograms.ExpectTimeBucketCount(kVisibilityDurationHistogramName,
                                   base::Time::Now() - start_time, 1);

  // No new entries for previous bucket.
  histograms.ExpectTimeBucketCount(kVisibilityDurationHistogramName,
                                   expected_sample1, 1);
}

TEST_F(PrivacyIndicatorsTrayItemViewTest, IndicatorVisisbilityOnSecondDisplay) {
  // Update usage when there's one display.
  UpdateCameraAndMicrophoneUsage(
      /*is_camera_used=*/true,
      /*is_microphone_used=*/false);

  ASSERT_TRUE(privacy_indicators_view()->GetVisible());

  // Now set up 2 displays. The indicator should show on both displays.
  UpdateDisplay("100x200,300x400");

  EXPECT_TRUE(privacy_indicators_view()->GetVisible());
  EXPECT_TRUE(GetSecondaryDisplayPrivacyIndicatorsView()->GetVisible());
}

}  // namespace ash
