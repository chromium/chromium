// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_panel.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace {

crosapi::mojom::VideoConferenceMediaAppInfoPtr CreateFakeMediaApp(
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& title,
    std::string url,
    const crosapi::mojom::VideoConferenceAppType app_type =
        crosapi::mojom::VideoConferenceAppType::kChromeTab,
    const base::UnguessableToken& id = base::UnguessableToken::Create()) {
  return crosapi::mojom::VideoConferenceMediaAppInfo::New(
      id,
      /*last_activity_time=*/base::Time::Now(), is_capturing_camera,
      is_capturing_microphone, is_capturing_screen, title,
      /*url=*/GURL(url), app_type);
}

// Verifies the information of `ReturnToAppButton`.
void VerifyReturnToAppButtonInfo(
    ash::video_conference::ReturnToAppButton* button,
    bool is_capturing_camera,
    bool is_capturing_microphone,
    bool is_capturing_screen,
    const std::u16string& display_text) {
  EXPECT_EQ(is_capturing_camera, button->is_capturing_camera());
  EXPECT_EQ(is_capturing_microphone, button->is_capturing_microphone());
  EXPECT_EQ(is_capturing_screen, button->is_capturing_screen());
  EXPECT_EQ(display_text, button->label()->GetText());
}

// Used for verifying displayed url.
const std::string kMeetTestUrl = "https://meet.google.com/abc-xyz/ab-123";
const std::u16string kExpectedMeetDisplayedUrl =
    u"meet.google.com/abc-xyz/ab-123";

}  // namespace

namespace ash::video_conference {

class ReturnToAppPanelTest : public AshTestBase {
 public:
  ReturnToAppPanelTest() = default;
  ReturnToAppPanelTest(const ReturnToAppPanelTest&) = delete;
  ReturnToAppPanelTest& operator=(const ReturnToAppPanelTest&) = delete;
  ~ReturnToAppPanelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();

    // Make the video conference tray visible for testing.
    video_conference_tray()->SetVisiblePreferred(true);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  // Get the `ReturnToAppPanel` from the test `StatusAreaWidget`.
  ReturnToAppPanel* GetReturnToAppPanel() {
    return static_cast<ReturnToAppPanel*>(
        video_conference_tray()->GetBubbleView()->GetViewByID(
            BubbleViewID::kReturnToApp));
  }

  ReturnToAppPanel::ReturnToAppContainer* GetReturnToAppContainer(
      ReturnToAppPanel* panel) {
    return panel->container_view_;
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

  // Get the instance that handle bounds change for the expand/collapse
  // animation.
  gfx::LinearAnimation* GetBoundsChangeAnimation() {
    return GetReturnToAppContainer(GetReturnToAppPanel())->animation_.get();
  }

  void AnimateToValue(double animation_value) {
    auto* animation = GetBoundsChangeAnimation();
    EXPECT_TRUE(animation->is_animating());
    animation->SetCurrentValue(animation_value);
    GetReturnToAppContainer(GetReturnToAppPanel())
        ->AnimationProgressed(animation);
  }

  // Wait until the bounds change animation is completed.
  void WaitForAnimation() {
    do {
      base::RunLoop().RunUntilIdle();
    } while (GetBoundsChangeAnimation()->is_animating());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(ReturnToAppPanelTest, NoApp) {
  MediaApps apps;

  // The view should not be visible when there's no app.
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>(apps);
  EXPECT_FALSE(return_to_app_panel->GetVisible());
}

TEST_F(ReturnToAppPanelTest, OneApp) {
  bool is_capturing_camera = true;
  bool is_capturing_microphone = false;
  bool is_capturing_screen = false;
  auto* title = u"Meet";

  MediaApps apps;
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, title,
      /*url=*/kMeetTestUrl));

  // There should be one child representing the only one running media app.
  auto panel = std::make_unique<ReturnToAppPanel>(apps);
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());

  EXPECT_EQ(1u, return_to_app_container->children().size());

  auto* app_button = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  EXPECT_FALSE(app_button->expand_indicator_for_testing()->GetVisible());
  VerifyReturnToAppButtonInfo(app_button, is_capturing_camera,
                              is_capturing_microphone, is_capturing_screen,
                              /*display_text=*/title);
}

TEST_F(ReturnToAppPanelTest, MultipleApps) {
  auto* title = u"Meet";

  MediaApps apps;
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, title,
      /*url=*/kMeetTestUrl));
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"",
      /*url=*/kMeetTestUrl));

  // There should be three children, one representing the summary row and two
  // for two running media apps.
  auto panel = std::make_unique<ReturnToAppPanel>(apps);
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());
  EXPECT_EQ(3u, return_to_app_container->children().size());

  // The first row should be the summary row, representing the state of
  // capturing from all apps and showing that 2 apps are running.
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  VerifyReturnToAppButtonInfo(
      summary_row, /*is_capturing_camera=*/true,
      /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true,
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT, 2));

  // Verify the next 2 rows, representing the 2 running apps.
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  VerifyReturnToAppButtonInfo(first_app_row, /*is_capturing_camera=*/true,
                              /*is_capturing_microphone=*/false,
                              /*is_capturing_screen=*/false,
                              /*display_text=*/title);

  // If the title is empty, the button should display the app url.
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);
  VerifyReturnToAppButtonInfo(second_app_row, /*is_capturing_camera=*/false,
                              /*is_capturing_microphone=*/true,
                              /*is_capturing_screen=*/true,
                              /*display_text=*/kExpectedMeetDisplayedUrl);
}

TEST_F(ReturnToAppPanelTest, ExpandCollapse) {
  MediaApps apps;
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  auto panel = std::make_unique<ReturnToAppPanel>(apps);
  auto* return_to_app_container = GetReturnToAppContainer(panel.get());
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  EXPECT_TRUE(summary_row->expand_indicator_for_testing()->GetVisible());

  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  // The panel should be collapsed by default.
  EXPECT_FALSE(summary_row->expanded());

  // Verify the views in collapsed state:
  EXPECT_TRUE(summary_row->icons_container()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SHOW_TOOLTIP),
            summary_row->expand_indicator_for_testing()->GetTooltipText());
  EXPECT_FALSE(first_app_row->GetVisible());
  EXPECT_FALSE(second_app_row->GetVisible());

  // Clicking the summary row should expand the panel.
  summary_row->OnButtonClicked(
      /*id=*/base::UnguessableToken::Null(),
      /*app_type=*/crosapi::mojom::VideoConferenceAppType::kDefaultValue);
  EXPECT_TRUE(summary_row->expanded());

  // Verify the views in expanded state:
  EXPECT_FALSE(summary_row->icons_container()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_HIDE_TOOLTIP),
            summary_row->expand_indicator_for_testing()->GetTooltipText());
  EXPECT_TRUE(first_app_row->GetVisible());
  EXPECT_TRUE(second_app_row->GetVisible());

  // Click again. Should be in collapsed state.
  summary_row->OnButtonClicked(
      /*id=*/base::UnguessableToken::Null(),
      /*app_type=*/crosapi::mojom::VideoConferenceAppType::kDefaultValue);
  EXPECT_FALSE(summary_row->expanded());
}

TEST_F(ReturnToAppPanelTest, MaxCapturingCount) {
  // Test the panel's `max_capturing_count_` to make sure the buttons are
  // aligned correctly.
  MediaApps apps;
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  auto return_to_app_panel = std::make_unique<ReturnToAppPanel>(apps);
  EXPECT_EQ(1, return_to_app_panel->max_capturing_count());

  apps.clear();
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  return_to_app_panel = std::make_unique<ReturnToAppPanel>(apps);
  EXPECT_EQ(2, return_to_app_panel->max_capturing_count());

  apps.clear();
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  apps.emplace_back(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));
  return_to_app_panel = std::make_unique<ReturnToAppPanel>(apps);
  EXPECT_EQ(3, return_to_app_panel->max_capturing_count());
}

TEST_F(ReturnToAppPanelTest, ReturnToApp) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  base::HistogramTester histogram_tester;

  auto app_id1 = base::UnguessableToken::Create();
  auto app_id2 = base::UnguessableToken::Create();

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl,
      /*app_type=*/crosapi::mojom::VideoConferenceAppType::kChromeApp,
      /*id=*/app_id1));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/"", /*app_type=*/crosapi::mojom::VideoConferenceAppType::kArcApp,
      /*id=*/app_id2));

  LeftClickOn(toggle_bubble_button());
  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);

  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  // Clicking on the summary row should not launch any apps (it switched the
  // panel to expanded state).
  LeftClickOn(summary_row);
  ASSERT_TRUE(summary_row->expanded());
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id1]);
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id2]);

  // Clicking each row should open the corresponding app.
  LeftClickOn(first_app_row);
  EXPECT_TRUE(controller()->app_to_launch_state_[app_id1]);
  EXPECT_FALSE(controller()->app_to_launch_state_[app_id2]);
  histogram_tester.ExpectBucketCount(
      "Ash.VideoConference.ReturnToApp.Click",
      crosapi::mojom::VideoConferenceAppType::kChromeApp, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.VideoConference.ReturnToApp.Click",
      crosapi::mojom::VideoConferenceAppType::kArcApp, 0);

  LeftClickOn(second_app_row);
  EXPECT_TRUE(controller()->app_to_launch_state_[app_id2]);
  histogram_tester.ExpectBucketCount(
      "Ash.VideoConference.ReturnToApp.Click",
      crosapi::mojom::VideoConferenceAppType::kArcApp, 1);
}

TEST_F(ReturnToAppPanelTest, ExpandAnimation) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());

  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  ASSERT_FALSE(summary_row->expanded());

  auto panel_initial_height = return_to_app_panel->size().height();
  auto* vc_bubble = video_conference_tray()->GetBubbleView();
  auto bubble_initial_height = vc_bubble->size().height();

  // The animation should start after we click the summary row to expand the
  // panel.
  LeftClickOn(summary_row);
  EXPECT_TRUE(GetBoundsChangeAnimation()->is_animating());

  AnimateToValue(0.5);

  auto panel_mid_animation_height = return_to_app_panel->size().height();
  auto bubble_mid_animation_height = vc_bubble->size().height();

  // Make sure that the panel is expanding and the bubble is also expanding in
  // the same amount.
  EXPECT_GT(panel_mid_animation_height, panel_initial_height);
  EXPECT_EQ(panel_mid_animation_height - panel_initial_height,
            bubble_mid_animation_height - bubble_initial_height);

  // Test the same thing when animation ends.
  WaitForAnimation();

  auto panel_end_animation_height = return_to_app_panel->size().height();
  auto bubble_end_animation_height = vc_bubble->size().height();

  EXPECT_GT(panel_end_animation_height, panel_mid_animation_height);
  EXPECT_EQ(panel_end_animation_height - panel_mid_animation_height,
            bubble_end_animation_height - bubble_mid_animation_height);
}

TEST_F(ReturnToAppPanelTest, CollapseAnimation) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());

  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());

  LeftClickOn(summary_row);
  WaitForAnimation();

  ASSERT_TRUE(summary_row->expanded());

  auto panel_initial_height = return_to_app_panel->size().height();
  auto* vc_bubble = video_conference_tray()->GetBubbleView();
  auto bubble_initial_height = vc_bubble->size().height();

  // The animation should start after we click the summary row again to collapse
  // the panel.
  LeftClickOn(summary_row);
  EXPECT_TRUE(GetBoundsChangeAnimation()->is_animating());

  // Normally, a layer animation will be performed to fade out the return to app
  // buttons. However, since we are simulating different stage of the bounds
  // change animation, we will set visibility right away here to prevent the
  // layer animation from interfering with the bounds change animation
  // simulation.
  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);
  first_app_row->SetVisible(false);
  second_app_row->SetVisible(false);

  AnimateToValue(0.5);

  auto panel_mid_animation_height = return_to_app_panel->size().height();
  auto bubble_mid_animation_height = vc_bubble->size().height();

  // Make sure that the panel is collapsing and the bubble is also collapsing in
  // the same amount.
  EXPECT_LT(panel_mid_animation_height, panel_initial_height);
  EXPECT_EQ(panel_mid_animation_height - panel_initial_height,
            bubble_mid_animation_height - bubble_initial_height);

  // Test the same thing when animation ends.
  WaitForAnimation();

  auto panel_end_animation_height = return_to_app_panel->size().height();
  auto bubble_end_animation_height = vc_bubble->size().height();

  EXPECT_LT(panel_end_animation_height, panel_mid_animation_height);
  EXPECT_EQ(panel_end_animation_height - panel_mid_animation_height,
            bubble_end_animation_height - bubble_mid_animation_height);
}

// Verify that the layer animations to show/hide the view are performed with
// the expected visibility and opacity before and after the animation.
TEST_F(ReturnToAppPanelTest, LayerAnimations) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());

  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);
  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());

  // Expand animation: The return to app buttons should fade in.
  LeftClickOn(summary_row);

  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  EXPECT_EQ(0, first_app_row->layer()->opacity());
  EXPECT_EQ(0, second_app_row->layer()->opacity());

  ui::LayerAnimationStoppedWaiter layer_animation_waiter;
  layer_animation_waiter.Wait(first_app_row->layer());
  layer_animation_waiter.Wait(second_app_row->layer());

  EXPECT_EQ(1, first_app_row->layer()->opacity());
  EXPECT_EQ(1, second_app_row->layer()->opacity());

  // End the rest of the animation to test collapse animation.
  WaitForAnimation();
  ASSERT_TRUE(summary_row->expanded());

  // Collapse animation: The return to app buttons should fade out and the
  // summary icons should fade in.
  LeftClickOn(summary_row);
  EXPECT_TRUE(GetBoundsChangeAnimation()->is_animating());

  auto* summary_icons = summary_row->icons_container();
  EXPECT_EQ(0, summary_icons->layer()->opacity());

  EXPECT_TRUE(first_app_row->GetVisible());
  EXPECT_TRUE(second_app_row->GetVisible());

  layer_animation_waiter.Wait(summary_icons->layer());
  layer_animation_waiter.Wait(first_app_row->layer());
  layer_animation_waiter.Wait(second_app_row->layer());

  EXPECT_EQ(1, summary_icons->layer()->opacity());
  EXPECT_FALSE(first_app_row->GetVisible());
  EXPECT_FALSE(second_app_row->GetVisible());
}

TEST_F(ReturnToAppPanelTest, ReturnToAppButtonTextElide) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false,
      /*title=*/
      u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      u"eiusmod tempor incididunt ut labore et dolore magna aliqua.",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());

  auto* return_to_app_container =
      GetReturnToAppContainer(GetReturnToAppPanel());
  auto* app_button = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());
  auto* app_button_label = app_button->label();

  // With a long title, the app title should still fit inside the button (the
  // width of the label should still be smaller).
  EXPECT_LT(app_button_label->width(), app_button->width());

  const char16_t kEllipsisString[] = {0x2026, 0};

  // The display text should end with the ellipsis.
  EXPECT_TRUE(base::EndsWith(app_button_label->GetDisplayTextForTesting(),
                             kEllipsisString));
}

TEST_F(ReturnToAppPanelTest, ReturnToAppButtonAccessibleName) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());
  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);

  auto* first_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[1]);
  auto* second_app_row =
      static_cast<ReturnToAppButton*>(return_to_app_container->children()[2]);

  auto expected_camera_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  auto expected_microphone_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  auto expected_screen_share_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));

  // Verify accessible name for each row.
  EXPECT_EQ(expected_camera_text + u"Meet",
            first_app_row->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(expected_microphone_text + expected_screen_share_text + u"Zoom",
            second_app_row->GetViewAccessibility().GetCachedName());
}

TEST_F(ReturnToAppPanelTest, ReturnToAppButtonSummaryRowAccessibleName) {
  controller()->ClearMediaApps();
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Meet",
      /*url=*/kMeetTestUrl));
  controller()->AddMediaApp(CreateFakeMediaApp(
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/""));

  LeftClickOn(toggle_bubble_button());
  auto* return_to_app_panel = GetReturnToAppPanel();
  auto* return_to_app_container = GetReturnToAppContainer(return_to_app_panel);

  auto* summary_row = static_cast<ReturnToAppButton*>(
      return_to_app_container->children().front());

  auto expected_camera_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_CAMERA));
  auto expected_microphone_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_MICROPHONE));
  auto expected_screen_share_text = l10n_util::GetStringFUTF16(
      VIDEO_CONFERENCE_RETURN_TO_APP_PERIPHERALS_ACCESSIBLE_NAME,
      l10n_util::GetStringUTF16(
          VIDEO_CONFERENCE_TOGGLE_BUTTON_TYPE_SCREEN_SHARE));
  auto expected_button_text =
      expected_camera_text + expected_microphone_text +
      expected_screen_share_text +
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_VIDEO_CONFERENCE_RETURN_TO_APP_SUMMARY_TEXT, 2);

  EXPECT_EQ(expected_button_text +
                l10n_util::GetStringUTF16(
                    VIDEO_CONFERENCE_RETURN_TO_APP_COLLAPSED_ACCESSIBLE_NAME),
            summary_row->GetViewAccessibility().GetCachedName());

  LeftClickOn(summary_row);

  EXPECT_EQ(expected_button_text +
                l10n_util::GetStringUTF16(
                    VIDEO_CONFERENCE_RETURN_TO_APP_EXPANDED_ACCESSIBLE_NAME),
            summary_row->GetViewAccessibility().GetCachedName());

  LeftClickOn(summary_row);
  EXPECT_EQ(expected_button_text +
                l10n_util::GetStringUTF16(
                    VIDEO_CONFERENCE_RETURN_TO_APP_COLLAPSED_ACCESSIBLE_NAME),
            summary_row->GetViewAccessibility().GetCachedName());
}

}  // namespace ash::video_conference
