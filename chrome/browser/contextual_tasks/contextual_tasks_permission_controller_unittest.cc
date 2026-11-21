// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"

#include <string>

#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/contextual_tasks/public/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_constants.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace contextual_tasks {

namespace {

class MockPermissionChipObserver : public PermissionChipInterface::Observer {
 public:
  MOCK_METHOD(void, OnMousePressed, (), (override));
};

}  // namespace

class ContextualTasksPermissionControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit ContextualTasksPermissionControllerTest(
      bool enable_rearchitecture = true)
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    if (enable_rearchitecture) {
      feature_list_.InitAndEnableFeature(
          kContextualTasksSidePanelRearchitecture);
    } else {
      feature_list_.InitAndDisableFeature(
          kContextualTasksSidePanelRearchitecture);
    }
  }
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_browser_window_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    ON_CALL(testing::Const(mock_browser_window_), GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    mock_panel_controller_ =
        std::make_unique<testing::NiceMock<MockContextualTasksPanelController>>(
            &mock_browser_window_);
    ON_CALL(*mock_panel_controller_, IsPanelOpenForContextualTask())
        .WillByDefault(testing::Return(true));

    controller_ = std::make_unique<ContextualTasksPermissionController>(
        &mock_browser_window_);
    controller_->RegisterWebContents(web_contents());
    controller_->Update(web_contents());
  }

  void TearDown() override {
    controller_.reset();
    mock_panel_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ContextualTasksPermissionController* controller() {
    return controller_.get();
  }

  MockContextualTasksPanelController& mock_panel_controller() {
    return *mock_panel_controller_;
  }

  MockBrowserWindowInterface* mock_browser_window() {
    return &mock_browser_window_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksPanelController>>
      mock_panel_controller_;
  std::unique_ptr<ContextualTasksPermissionController> controller_;
};

TEST_F(ContextualTasksPermissionControllerTest,
       RegistersSingleLocationBarAcrossMultipleWebContents) {
  auto* location_bar = controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar);
  EXPECT_EQ(location_bar::GetLocationBarForWebContents(web_contents()),
            location_bar);
  EXPECT_EQ(location_bar->GetWebContents(), web_contents());

  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  EXPECT_EQ(
      location_bar::GetLocationBarForWebContents(second_web_contents.get()),
      location_bar);

  // Switching active task updates the single location bar's WebContents.
  controller()->Update(second_web_contents.get());
  EXPECT_EQ(location_bar->GetWebContents(), second_web_contents.get());

  // Passing nullptr clears the active WebContents on the location bar.
  controller()->Update(nullptr);
  EXPECT_EQ(location_bar->GetWebContents(), nullptr);

  // Unregistering removes the override.
  controller()->UnregisterWebContents(second_web_contents.get());
  EXPECT_EQ(
      location_bar::GetLocationBarForWebContents(second_web_contents.get()),
      nullptr);
}

TEST_F(ContextualTasksPermissionControllerTest, GetStateReflectsChipChanges) {
  auto initial_state = controller()->GetState();
  ASSERT_TRUE(initial_state);
  ASSERT_TRUE(initial_state->request_chip);
  EXPECT_FALSE(initial_state->request_chip->is_visible);
  ASSERT_TRUE(initial_state->indicator_chip);
  EXPECT_FALSE(initial_state->indicator_chip->is_visible);

  auto* dashboard =
      controller()->GetLocationBarForTesting()->permission_dashboard();
  ASSERT_TRUE(dashboard);
  dashboard->SetVisible(true);

  auto* request_chip = dashboard->request_chip();
  const gfx::VectorIcon& icon =
      features::IsRoundedIconsEnabled()
          ? vector_icons::kVideocamIcon
          : vector_icons::kVideocamChromeRefreshOldIcon;
  request_chip->SetVisible(true);
  request_chip->SetChipIcon(icon);
  request_chip->SetMessage(u"Use your camera?");
  request_chip->SetTooltipText(u"Camera tooltip");
  request_chip->SetTheme(PermissionChipTheme::kNormalVisibility);
  request_chip->SetUserDecision(permissions::PermissionAction::GRANTED);
  request_chip->SetPermissionPromptStyle(PermissionPromptStyle::kChip);
  request_chip->SetAccessibilityName(u"Camera permission");
  request_chip->AnimateExpand(base::Milliseconds(350));

  auto state = controller()->GetState();
  ASSERT_TRUE(state);
  ASSERT_TRUE(state->request_chip);
  EXPECT_TRUE(state->request_chip->is_visible);
  EXPECT_EQ(state->request_chip->icon_name, "webui-toolbar-shared:videocam");
  EXPECT_EQ(state->request_chip->message, u"Use your camera?");
  EXPECT_EQ(state->request_chip->tooltip, u"Camera tooltip");
  EXPECT_EQ(state->request_chip->theme,
            toolbar_ui_api::mojom::PermissionChipTheme::kNormalVisibility);
  EXPECT_EQ(state->request_chip->user_decision,
            toolbar_ui_api::mojom::PermissionAction::kGranted);
  EXPECT_EQ(state->request_chip->prompt_style,
            toolbar_ui_api::mojom::PermissionPromptStyle::kChip);
  EXPECT_EQ(state->request_chip->accessibility_name, u"Camera permission");
  EXPECT_FALSE(state->request_chip->is_fully_collapsed);
}

TEST_F(ContextualTasksPermissionControllerTest, ForwardsChipInteractions) {
  auto* dashboard =
      controller()->GetLocationBarForTesting()->permission_dashboard();
  auto* request_chip = dashboard->request_chip();

  request_chip->SetVisible(true);
  request_chip->AnimateExpand(base::Milliseconds(350));
  EXPECT_TRUE(request_chip->IsAnimating());

  controller()->OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest);
  EXPECT_FALSE(request_chip->IsAnimating());
  EXPECT_FALSE(request_chip->IsFullyCollapsed());

  request_chip->AnimateCollapse(base::Milliseconds(350));
  EXPECT_TRUE(request_chip->IsAnimating());

  controller()->OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest);
  EXPECT_FALSE(request_chip->IsAnimating());
  EXPECT_TRUE(request_chip->IsFullyCollapsed());

  bool pressed_callback_ran = false;
  bool pressed_with_pointer = false;
  request_chip->SetPressedCallback(
      base::BindLambdaForTesting([&](bool is_pointer_interaction) {
        pressed_callback_ran = true;
        pressed_with_pointer = is_pointer_interaction;
      }));

  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest,
      /*is_mouse_interaction=*/true, request_chip->state_token());

  EXPECT_TRUE(pressed_callback_ran);
  EXPECT_TRUE(pressed_with_pointer);

  EXPECT_FALSE(request_chip->IsMouseHovered());
  controller()->OnChipPointerEntered(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest);
  EXPECT_TRUE(request_chip->IsMouseHovered());

  controller()->OnChipPointerExited(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest);
  EXPECT_FALSE(request_chip->IsMouseHovered());

  testing::StrictMock<MockPermissionChipObserver> request_observer;
  testing::StrictMock<MockPermissionChipObserver> indicator_observer;
  base::ScopedObservation<PermissionChipInterface,
                          PermissionChipInterface::Observer>
      request_observation(&request_observer);
  base::ScopedObservation<PermissionChipInterface,
                          PermissionChipInterface::Observer>
      indicator_observation(&indicator_observer);
  request_observation.Observe(request_chip);
  indicator_observation.Observe(dashboard->indicator_chip());

  EXPECT_CALL(request_observer, OnMousePressed());
  controller()->OnChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest);
  testing::Mock::VerifyAndClearExpectations(&request_observer);

  EXPECT_CALL(indicator_observer, OnMousePressed());
  controller()->OnChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  testing::Mock::VerifyAndClearExpectations(&indicator_observer);
}

// Switching the active task invalidates the chips' tokens even if their
// visibility doesn't change, so a click sent for the previous task is dropped.
TEST_F(ContextualTasksPermissionControllerTest,
       TaskSwitchInvalidatesChipClicks) {
  auto* dashboard =
      controller()->GetLocationBarForTesting()->permission_dashboard();
  auto* request_chip = dashboard->request_chip();
  auto* indicator_chip = dashboard->indicator_chip();

  // Both chips are hidden and stay hidden across the switch: neither
  // `ResetPermissionPromptChip()` nor the indicator update changes them, so
  // only the task switch itself can change their tokens. (A visible request
  // chip would be hidden by `ResetPermissionPromptChip()`, which changes its
  // token anyway. An indicator chip can stay visible across the switch; see
  // `TaskSwitchInvalidatesClicksOnIndicatorThatStaysVisible`.)
  ASSERT_FALSE(request_chip->GetVisible());
  ASSERT_FALSE(indicator_chip->GetVisible());
  const uint32_t first_task_request_token = request_chip->state_token();
  const uint32_t first_task_indicator_token = indicator_chip->state_token();

  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  controller()->Update(second_web_contents.get());

  ASSERT_FALSE(request_chip->GetVisible());
  ASSERT_FALSE(indicator_chip->GetVisible());
  EXPECT_NE(request_chip->state_token(), first_task_request_token);
  EXPECT_NE(indicator_chip->state_token(), first_task_indicator_token);
  // `GetState()`, which is what the WebUI is sent, reports the new tokens.
  toolbar_ui_api::mojom::PermissionDashboardStatePtr state =
      controller()->GetState();
  EXPECT_EQ(state->request_chip->state_token, request_chip->state_token());
  EXPECT_EQ(state->indicator_chip->state_token, indicator_chip->state_token());

  // Set after the switch, which resets the request chip's pressed callback.
  int request_pressed_count = 0;
  request_chip->SetPressedCallback(base::BindLambdaForTesting(
      [&request_pressed_count](bool is_pointer_interaction) {
        ++request_pressed_count;
      }));
  int indicator_pressed_count = 0;
  indicator_chip->SetPressedCallback(base::BindLambdaForTesting(
      [&indicator_pressed_count](bool is_pointer_interaction) {
        ++indicator_pressed_count;
      }));

  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest,
      /*is_mouse_interaction=*/true, first_task_request_token);
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/true, first_task_indicator_token);
  EXPECT_EQ(request_pressed_count, 0);
  EXPECT_EQ(indicator_pressed_count, 0);

  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest,
      /*is_mouse_interaction=*/true, request_chip->state_token());
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/true, indicator_chip->state_token());
  EXPECT_EQ(request_pressed_count, 1);
  EXPECT_EQ(indicator_pressed_count, 1);

  // Updating with the same task doesn't invalidate the tokens.
  const uint32_t second_task_request_token = request_chip->state_token();
  const uint32_t second_task_indicator_token = indicator_chip->state_token();
  controller()->Update(second_web_contents.get());
  EXPECT_EQ(request_chip->state_token(), second_task_request_token);
  EXPECT_EQ(indicator_chip->state_token(), second_task_indicator_token);

  controller()->UnregisterWebContents(second_web_contents.get());
}

// If both tasks have an active indicator and each has already shown its
// verbose animation, the indicator chip stays visible across the switch and
// doesn't re-animate, so only the task switch itself invalidates its token. A
// click sent for the previous task is still dropped.
TEST_F(ContextualTasksPermissionControllerTest,
       TaskSwitchInvalidatesClicksOnIndicatorThatStaysVisible) {
  const GURL url("https://example.com");
  auto* location_bar = controller()->GetLocationBarForTesting();
  auto* indicator_chip = location_bar->permission_dashboard()->indicator_chip();
  auto* dashboard_controller = location_bar->GetPermissionDashboardController();
  // Runs the first indicator's verbose expand/collapse animation to the end.
  auto finish_verbose_animation = [&] {
    controller()->OnChipExpandAnimationEnded(
        toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
    dashboard_controller->get_collapse_timer_for_testing().FireNow();
    controller()->OnChipCollapseAnimationEnded(
        toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  };

  // Task A shows a blocked mic indicator.
  NavigateAndCommit(url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame())
      ->OnMediaStreamPermissionSet(
          url,
          {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
           content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  ASSERT_TRUE(indicator_chip->GetVisible());
  finish_verbose_animation();

  // Switch to task B, which also shows a blocked mic indicator.
  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      second_web_contents.get(), url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      second_web_contents.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          second_web_contents.get()));
  controller()->Update(second_web_contents.get());
  content_settings::PageSpecificContentSettings::GetForFrame(
      second_web_contents->GetPrimaryMainFrame())
      ->OnMediaStreamPermissionSet(
          url,
          {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
           content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  ASSERT_TRUE(indicator_chip->GetVisible());
  finish_verbose_animation();
  ASSERT_FALSE(indicator_chip->IsAnimating());
  const uint32_t second_task_token = indicator_chip->state_token();

  // Switch back to task A. A's indicator is still active and has already shown
  // its verbose animation, so the chip stays visible and doesn't animate.
  controller()->Update(web_contents());
  ASSERT_TRUE(indicator_chip->GetVisible());
  ASSERT_FALSE(indicator_chip->IsAnimating());
  EXPECT_NE(indicator_chip->state_token(), second_task_token);

  int pressed_count = 0;
  indicator_chip->SetPressedCallback(base::BindLambdaForTesting(
      [&pressed_count](bool is_pointer_interaction) { ++pressed_count; }));
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/true, second_task_token);
  EXPECT_EQ(pressed_count, 0);

  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/true, indicator_chip->state_token());
  EXPECT_EQ(pressed_count, 1);

  controller()->UnregisterWebContents(second_web_contents.get());
}

TEST_F(ContextualTasksPermissionControllerTest,
       TracksVerboseIndicatorStateForNonTabWebContents) {
  const GURL url1("https://example.com");
  const GURL url2("https://other.com");
  NavigateAndCommit(url1);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  auto* location_bar = controller()->GetLocationBarForTesting();
  auto* indicator_chip = location_bar->permission_dashboard()->indicator_chip();
  auto* dashboard_controller = location_bar->GetPermissionDashboardController();
  auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  // 1. First blocked mic indicator on `url1` expands to verbose state.
  pscs->OnMediaStreamPermissionSet(
      url1,
      {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(indicator_chip->IsFullyCollapsed());

  controller()->OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  EXPECT_TRUE(dashboard_controller->is_verbose());

  dashboard_controller->get_collapse_timer_for_testing().FireNow();
  controller()->OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  EXPECT_FALSE(dashboard_controller->is_verbose());

  pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                               /*update_indicators=*/true);
  EXPECT_FALSE(indicator_chip->GetVisible());

  // 2. Second blocked mic indicator on `url1` stays collapsed.
  pscs->OnMediaStreamPermissionSet(
      url1,
      {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_TRUE(indicator_chip->IsFullyCollapsed());
  EXPECT_FALSE(dashboard_controller->is_verbose());

  pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                               /*update_indicators=*/true);
  EXPECT_FALSE(indicator_chip->GetVisible());

  // 3. Switching the shared location bar to another task WebContents (A -> B)
  // allows B's first indicator to expand, and switching back (B -> A) preserves
  // A's already-displayed indicator state so A stays collapsed.
  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      second_web_contents.get(), url1);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      second_web_contents.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          second_web_contents.get()));
  controller()->Update(second_web_contents.get());

  auto* second_pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          second_web_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(second_pscs);
  second_pscs->OnMediaStreamPermissionSet(
      url1,
      {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(indicator_chip->IsFullyCollapsed());

  controller()->OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  dashboard_controller->get_collapse_timer_for_testing().FireNow();
  controller()->OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator);
  second_pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                                      /*update_indicators=*/true);

  // Switch back to the first WebContents (B -> A): a new indicator on `url1`
  // must still stay collapsed.
  controller()->Update(web_contents());
  pscs->OnMediaStreamPermissionSet(
      url1,
      {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_TRUE(indicator_chip->IsFullyCollapsed());
  EXPECT_FALSE(dashboard_controller->is_verbose());

  pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                               /*update_indicators=*/true);
  EXPECT_FALSE(indicator_chip->GetVisible());

  // 4. Navigating the side panel WebContents cross-origin resets its verbose
  // indicator state without relying on an active browser tab.
  NavigateAndCommit(url2);
  pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);

  pscs->OnMediaStreamPermissionSet(
      url2,
      {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
       content_settings::PageSpecificContentSettings::kMicrophoneBlocked});
  EXPECT_TRUE(indicator_chip->GetVisible());
  EXPECT_FALSE(indicator_chip->IsFullyCollapsed());
}

TEST_F(ContextualTasksPermissionControllerTest,
       RecordsPermissionChipImpressions) {
  base::HistogramTester histogram_tester;
  WebUIPermissionDashboard* dashboard =
      controller()->GetLocationBarForTesting()->permission_dashboard();
  WebUIPermissionChip* request_chip = dashboard->request_chip();
  WebUIPermissionChip* indicator_chip = dashboard->indicator_chip();
  dashboard->SetVisible(true);

  // Showing a request chip and setting its theme in the same synchronous turn
  // coalesces into a single impression when `PushStateToWebUINow()` runs.
  request_chip->SetVisible(true);
  request_chip->SetTheme(PermissionChipTheme::kLowVisibility);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kQuietPermissionRequest, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    1);

  // Non-type state updates while still visible do not record another
  // impression.
  request_chip->SetMessage(u"Use your microphone?");
  request_chip->AnimateExpand(base::Milliseconds(350));
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    1);

  // Hiding and showing a normal-visibility request chip records a new sample.
  request_chip->SetVisible(false);
  task_environment()->RunUntilIdle();
  request_chip->SetVisible(true);
  request_chip->SetTheme(PermissionChipTheme::kNormalVisibility);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kPermissionRequest, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    2);

  // Indicator chip impressions record each distinct indicator theme.
  indicator_chip->SetVisible(true);
  indicator_chip->SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kInUseActivityIndicator, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    3);

  // Transitioning directly from in-use to blocked records the blocked
  // indicator.
  indicator_chip->SetTheme(PermissionChipTheme::kBlockedActivityIndicator);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kBlockedActivityIndicator, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    4);

  // Switching tasks resets impression tracking so a visible chip on the new
  // task records an impression for that task.
  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  controller()->Update(second_web_contents.get());
  dashboard->SetVisible(true);
  indicator_chip->SetVisible(true);
  indicator_chip->SetTheme(
      PermissionChipTheme::kOnSystemBlockedActivityIndicator);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kSystemBlockedActivityIndicator, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    5);

  // When the side panel is closed, state pushes do not record impressions and
  // reset the last-seen chip type so that re-opening the side panel records the
  // visible chip.
  ON_CALL(mock_panel_controller(), IsPanelOpenForContextualTask())
      .WillByDefault(testing::Return(false));
  indicator_chip->SetTheme(PermissionChipTheme::kInUseActivityIndicator);
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    5);

  ON_CALL(mock_panel_controller(), IsPanelOpenForContextualTask())
      .WillByDefault(testing::Return(true));
  controller()->PushStateToWebUI();
  task_environment()->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Impression",
      ContextualTasksPermissionChipType::kInUseActivityIndicator, 2);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    6);

  controller()->UnregisterWebContents(second_web_contents.get());
}

TEST_F(ContextualTasksPermissionControllerTest, RecordsPermissionChipClicks) {
  base::HistogramTester histogram_tester;
  WebUIPermissionDashboard* dashboard =
      controller()->GetLocationBarForTesting()->permission_dashboard();
  WebUIPermissionChip* request_chip = dashboard->request_chip();
  WebUIPermissionChip* indicator_chip = dashboard->indicator_chip();

  request_chip->SetVisible(true);
  request_chip->SetTheme(PermissionChipTheme::kNormalVisibility);
  // Even if the pressed callback hides the chip during the click, the click
  // histogram captures the chip type at the time of the click.
  request_chip->SetPressedCallback(
      base::BindLambdaForTesting([request_chip](bool is_pointer_interaction) {
        request_chip->SetVisible(false);
      }));

  const uint32_t valid_request_token = request_chip->state_token();
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest,
      /*is_mouse_interaction=*/true, valid_request_token);
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Click",
      ContextualTasksPermissionChipType::kPermissionRequest, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Click", 1);

  // A stale token click is dropped and does not record a click sample.
  indicator_chip->SetVisible(true);
  indicator_chip->SetTheme(PermissionChipTheme::kBlockedActivityIndicator);
  indicator_chip->SetPressedCallback(
      base::BindLambdaForTesting([](bool is_pointer_interaction) {}));
  const uint32_t stale_indicator_token = indicator_chip->state_token() - 1;
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/true, stale_indicator_token);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Click", 1);

  // Clicking with the current token records the indicator click.
  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator,
      /*is_mouse_interaction=*/false, indicator_chip->state_token());
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.PermissionChip.Click",
      ContextualTasksPermissionChipType::kBlockedActivityIndicator, 1);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Click", 2);
}

TEST_F(ContextualTasksPermissionControllerTest,
       IndicatorAccessibilityNamePreservedAcrossTaskSwitchesAndClearedOnHide) {
  const GURL url("https://example.com");
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      CreateTestWebContents();
  ON_CALL(mock_panel_controller(), GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));

  ContextualTasksLocationBar* location_bar =
      controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar->CanAnnounceAlert());
  WebUIPermissionChip* indicator_chip =
      location_bar->permission_dashboard()->indicator_chip();
  PermissionDashboardController* dashboard_controller =
      location_bar->GetPermissionDashboardController();

  NavigateAndCommit(url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  content_settings::PageSpecificContentSettings* first_pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  first_pscs->OnMediaStreamPermissionSet(
      url, {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
            content_settings::PageSpecificContentSettings::kMicrophoneBlocked});

  const std::u16string expected_mic_blocked_name =
      l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED);
  EXPECT_EQ(indicator_chip->GetState()->accessibility_name,
            expected_mic_blocked_name);
  ASSERT_TRUE(dashboard_controller->indicator_model_for_testing());
  EXPECT_FALSE(dashboard_controller->indicator_model_for_testing()
                   ->ShouldNotifyAccessibility(web_contents()));

  // Switch to Task B with a blocked camera indicator.
  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      second_web_contents.get(), url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      second_web_contents.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          second_web_contents.get()));
  controller()->Update(second_web_contents.get());
  content_settings::PageSpecificContentSettings::GetForFrame(
      second_web_contents->GetPrimaryMainFrame())
      ->OnMediaStreamPermissionSet(
          url, {content_settings::PageSpecificContentSettings::kCameraAccessed,
                content_settings::PageSpecificContentSettings::kCameraBlocked});

  const std::u16string expected_camera_blocked_name =
      l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED);
  EXPECT_EQ(indicator_chip->GetState()->accessibility_name,
            expected_camera_blocked_name);

  // Switch back to Task A: even though Task A already notified accessibility on
  // its first visit, the indicator chip's accessible name must restore Task A's
  // full blocked-microphone string rather than leaking Task B's camera string
  // or falling back to "Not allowed".
  controller()->Update(web_contents());
  EXPECT_EQ(indicator_chip->GetState()->accessibility_name,
            expected_mic_blocked_name);

  first_pscs->ResetMediaBlockedState(ContentSettingsType::MEDIASTREAM_MIC,
                                     /*update_indicators=*/true);
  EXPECT_FALSE(indicator_chip->GetVisible());
  EXPECT_TRUE(indicator_chip->GetState()->accessibility_name.empty());

  controller()->UnregisterWebContents(second_web_contents.get());
}

TEST_F(
    ContextualTasksPermissionControllerTest,
    DefersIndicatorAccessibilityNotificationUntilAlertDeliverableWithoutDoubleCounting) {
  class DeliverableLocationBar : public LocationBarStub,
                                 public ContentSettingImageViewDelegate {
   public:
    explicit DeliverableLocationBar(content::WebContents* contents)
        : web_contents_(contents),
          media_model_(ContentSettingImageModel::CreateForContentType(
              ContentSettingImageModel::ImageType::kMediaStream)),
          dashboard_(this),
          dashboard_controller_(this, this, &dashboard_) {}

    content::WebContents* GetWebContents() override { return web_contents_; }
    bool CanAnnounceAlert() const override { return true; }
    void AnnounceAlert(const std::u16string& announcement) override {
      announcements_.push_back(announcement);
    }

    bool ShouldHideContentSettingImage() override { return false; }
    content::WebContents* GetContentSettingWebContents() override {
      return web_contents_;
    }
    ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
        override {
      return nullptr;
    }

    bool UpdateDashboard() {
      return dashboard_controller_.Update(media_model_.get());
    }
    ContentSettingImageModel* media_model() { return media_model_.get(); }
    const std::vector<std::u16string>& announcements() const {
      return announcements_;
    }

   private:
    raw_ptr<content::WebContents> web_contents_;
    std::unique_ptr<ContentSettingImageModel> media_model_;
    WebUIPermissionDashboard dashboard_;
    PermissionDashboardController dashboard_controller_;
    std::vector<std::u16string> announcements_;
  };

  base::HistogramTester histogram_tester;
  const GURL url("https://example.com");
  NavigateAndCommit(url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  // While the side panel is closed, `CanAnnounceAlert()` is false and indicator
  // updates (along with `Show` metrics) are suppressed completely.
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      CreateTestWebContents();
  ON_CALL(mock_panel_controller(), IsPanelOpenForContextualTask())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_panel_controller(), GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));

  ContextualTasksLocationBar* location_bar =
      controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar);
  EXPECT_FALSE(location_bar->CanAnnounceAlert());

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  pscs->OnMediaStreamPermissionSet(
      url, {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
            content_settings::PageSpecificContentSettings::kMicrophoneBlocked});

  EXPECT_FALSE(
      location_bar->permission_dashboard()->indicator_chip()->GetVisible());
  histogram_tester.ExpectTotalCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show", 0);

  // When the panel is open before `GetToolbarWebContents()` is ready,
  // `CanAnnounceAlert()` remains false: the indicator updates once without
  // flipping `AccessibilityWasNotified()`.
  ON_CALL(mock_panel_controller(), IsPanelOpenForContextualTask())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_panel_controller(), GetToolbarWebContents())
      .WillByDefault(testing::Return(nullptr));
  EXPECT_FALSE(location_bar->CanAnnounceAlert());
  controller()->RefreshContentSettingsIcons();

  PermissionDashboardController* dashboard_controller =
      location_bar->GetPermissionDashboardController();
  ASSERT_TRUE(dashboard_controller);
  ASSERT_TRUE(dashboard_controller->indicator_model_for_testing());
  EXPECT_TRUE(dashboard_controller->indicator_model_for_testing()
                  ->ShouldNotifyAccessibility(web_contents()));
  histogram_tester.ExpectUniqueSample(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show",
      permissions::ActivityIndicatorState::kBlockedOnSiteLevel, 1);

  // Repeated updates while alerts remain undeliverable must neither flip
  // `AccessibilityWasNotified()` nor double-count the Show metric.
  controller()->Update(web_contents());
  EXPECT_TRUE(dashboard_controller->indicator_model_for_testing()
                  ->ShouldNotifyAccessibility(web_contents()));
  histogram_tester.ExpectTotalCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show", 1);

  // Simulate transferring the task WebContents to a main-window LocationBar
  // where alerts are deliverable: the deferred alert fires once and the Show
  // metric is still recorded only once.
  controller()->UnregisterWebContents(web_contents());
  DeliverableLocationBar main_window_location_bar(web_contents());
  EXPECT_TRUE(main_window_location_bar.UpdateDashboard());
  EXPECT_EQ(main_window_location_bar.announcements().size(), 1u);
  EXPECT_FALSE(
      main_window_location_bar.media_model()->ShouldNotifyAccessibility(
          web_contents()));
  histogram_tester.ExpectTotalCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show", 1);
}

TEST_F(ContextualTasksPermissionControllerTest,
       FailsClosedWithoutTrackedAnchor) {
  class FakePermissionPromptDelegate
      : public permissions::PermissionPrompt::Delegate {
   public:
    FakePermissionPromptDelegate(content::WebContents* web_contents,
                                 const GURL& origin,
                                 bool quiet_ui = false)
        : web_contents_(web_contents), origin_(origin), quiet_ui_(quiet_ui) {
      requests_.push_back(std::make_unique<permissions::MockPermissionRequest>(
          origin, permissions::RequestType::kNotifications));
    }
    ~FakePermissionPromptDelegate() override = default;

    const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
    Requests() const override {
      return requests_;
    }
    GURL GetRequestingOrigin() const override { return origin_; }
    GURL GetEmbeddingOrigin() const override { return origin_; }
    void Accept(const PromptOptions&) override {}
    void AcceptThisTime(const PromptOptions&) override {}
    void Deny(const PromptOptions&) override {}
    void Dismiss(const PromptOptions&) override {}
    void Ignore(const PromptOptions&) override {}
    void SwitchToLoudPrompt() override {}
    GeolocationAccuracy GetInitialGeolocationAccuracySelection()
        const override {
      return GeolocationAccuracy::kPrecise;
    }
    std::optional<permissions::GeolocationPromptType> GetGeolocationPromptType()
        const override {
      return std::nullopt;
    }
    void FinalizeCurrentRequests() override {}
    void OpenHelpCenterLink(const ui::Event&) override {}
    void PreIgnoreQuietPrompt() override {}
    std::optional<permissions::PermissionUiSelector::QuietUiReason>
    ReasonForUsingQuietUi() const override {
      return quiet_ui_ ? std::optional(permissions::PermissionUiSelector::
                                           QuietUiReason::kEnabledInPrefs)
                       : std::nullopt;
    }
    bool ShouldCurrentRequestUseQuietUI() const override { return quiet_ui_; }
    bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
      return false;
    }
    bool WasCurrentRequestAlreadyDisplayed() override { return false; }
    void SetDismissOnTabClose() override {}
    void SetPromptShown() override { ++prompt_shown_count_; }
    void SetDecisionTime() override {}
    void SetManageClicked() override {}
    void SetLearnMoreClicked() override {}
    void SetHatsShownCallback(base::OnceCallback<void()>) override {}
    content::WebContents* GetAssociatedWebContents() override {
      return web_contents_;
    }
    base::WeakPtr<Delegate> GetWeakPtr() override {
      return weak_factory_.GetWeakPtr();
    }
    bool RecreateView() override { return false; }
    const permissions::PermissionPrompt* GetCurrentPrompt() const override {
      return nullptr;
    }
    int prompt_shown_count() const { return prompt_shown_count_; }

   private:
    raw_ptr<content::WebContents> web_contents_;
    GURL origin_;
    bool quiet_ui_ = false;
    int prompt_shown_count_ = 0;
    std::vector<std::unique_ptr<permissions::PermissionRequest>> requests_;
    base::WeakPtrFactory<FakePermissionPromptDelegate> weak_factory_{this};
  };

  const GURL url("https://example.com");
  NavigateAndCommit(url);

  ContextualTasksLocationBar* location_bar =
      controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar);
  EXPECT_FALSE(location_bar->ShouldFallbackToWindowContentsAnchor());

  PermissionDashboardController* dashboard_controller =
      location_bar->GetPermissionDashboardController();
  ASSERT_TRUE(dashboard_controller);
  EXPECT_TRUE(location_bar->permission_dashboard()->GetAnchor().IsNull());

  // Without a tracked WebUI chip or toolbar anchor, `ShowPageInfoDialog()`
  // fails closed instead of falling back to the main window's contents view.
  dashboard_controller->ShowPageInfoDialogForTesting();
  EXPECT_EQ(dashboard_controller->page_info_for_testing(), nullptr);

  // Likewise, clicking a confirmation request chip while its WebUI anchor is
  // untracked bails out before `ResetTimers()` so the confirmation collapse
  // timer keeps running.
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      CreateTestWebContents();
  ON_CALL(mock_panel_controller(), GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));
  permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  FakePermissionPromptDelegate fake_delegate(web_contents(), url);
  ChipController* chip_controller = location_bar->GetChipController();
  ASSERT_TRUE(chip_controller);
  chip_controller->InitializePermissionPrompt(fake_delegate.GetWeakPtr());
  chip_controller->OnRequestDecided(permissions::PermissionAction::GRANTED);
  ASSERT_TRUE(chip_controller->is_confirmation_showing());
  ASSERT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  ASSERT_TRUE(chip_controller->chip()->GetAnchor().IsNull());

  chip_controller->chip()->ExecuteForTesting();
  EXPECT_EQ(chip_controller->get_prompt_bubble_view_for_testing(), nullptr);
  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  chip_controller->ResetPermissionPromptChip();

  // Clicking a quiet request chip pauses timers while waiting for the WebUI
  // anchor; if the fallback times out with no tracked toolbar anchor, no
  // bubble opens, `SetPromptShown()` is not called, and the collapse timer
  // resumes instead of leaving the chip stuck open.
  FakePermissionPromptDelegate quiet_delegate(web_contents(), url,
                                              /*quiet_ui=*/true);
  chip_controller->ShowPermissionPrompt(quiet_delegate.GetWeakPtr());
  ASSERT_TRUE(chip_controller->IsPermissionPromptChipVisible());
  chip_controller->chip()->EndAnimationForTesting();
  ASSERT_TRUE(chip_controller->is_collapse_timer_running_for_testing());

  chip_controller->chip()->ExecuteForTesting();
  EXPECT_TRUE(chip_controller->is_waiting_for_anchor_for_testing());
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());

  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(chip_controller->is_waiting_for_anchor_for_testing());
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  EXPECT_EQ(chip_controller->get_prompt_bubble_view_for_testing(), nullptr);
  EXPECT_EQ(quiet_delegate.prompt_shown_count(), 0);
  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  chip_controller->ResetPermissionPromptChip();
}

class ContextualTasksPermissionControllerRearchitectureDisabledTest
    : public ContextualTasksPermissionControllerTest {
 public:
  ContextualTasksPermissionControllerRearchitectureDisabledTest()
      : ContextualTasksPermissionControllerTest(
            /*enable_rearchitecture=*/false) {}
};

TEST_F(ContextualTasksPermissionControllerRearchitectureDisabledTest,
       DoesNotCreateLocationBarOrOverrideData) {
  EXPECT_EQ(controller()->GetLocationBarForTesting(), nullptr);
  EXPECT_EQ(
      location_bar::LocationBarOverrideData::FromWebContents(web_contents()),
      nullptr);
  EXPECT_EQ(location_bar::GetLocationBarForWebContents(web_contents()),
            nullptr);

  base::HistogramTester histogram_tester;
  const GURL url("https://example.com");
  NavigateAndCommit(url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);
  pscs->OnMediaStreamPermissionSet(
      url, {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
            content_settings::PageSpecificContentSettings::kMicrophoneBlocked});

  EXPECT_FALSE(pscs->IsIndicatorVisible(ContentSettingsType::MEDIASTREAM_MIC));
  histogram_tester.ExpectTotalCount(
      "Permissions.ActivityIndicator.LHS.AudioCapture.Show", 0);

  controller()->Update(nullptr);
  controller()->Update(web_contents());
  task_environment()->RunUntilIdle();

  controller()->OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest,
      /*is_mouse_interaction=*/true, /*state_token=*/0);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Impression",
                                    0);
  histogram_tester.ExpectTotalCount("ContextualTasks.PermissionChip.Click", 0);

  controller()->UnregisterWebContents(web_contents());
  EXPECT_EQ(
      location_bar::LocationBarOverrideData::FromWebContents(web_contents()),
      nullptr);
}

TEST_F(ContextualTasksPermissionControllerTest,
       LocationBarResolvesBubbleModelDelegate) {
  ContextualTasksLocationBar* location_bar =
      controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar);

  EXPECT_EQ(location_bar->GetContentSettingBubbleModelDelegate(), nullptr);
  BrowserContentSettingBubbleModelDelegate bubble_model_delegate(
      mock_browser_window());
  ContentSettingBubbleModelDelegate* delegate =
      location_bar->GetContentSettingBubbleModelDelegate();
  ASSERT_NE(delegate, nullptr);

  EXPECT_CALL(
      mock_panel_controller(),
      OpenUrlInNewTab(GURL("https://support.google.com/chrome/answer/3220216")))
      .Times(1);
  delegate->ShowLearnMorePage(ContentSettingsType::NOTIFICATIONS);
}

TEST_F(ContextualTasksPermissionControllerTest,
       IndicatorAnnouncementOmitsAddressBarHint) {
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      CreateTestWebContents();
  ON_CALL(mock_panel_controller(), GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));

  ContextualTasksLocationBar* location_bar =
      controller()->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar);
  ASSERT_TRUE(location_bar->CanAnnounceAlert());
  EXPECT_FALSE(location_bar->ShouldAnnounceAddressBarHint());

  const std::u16string expected_announcement =
      l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED);
  const std::u16string address_bar_hint =
      l10n_util::GetStringUTF16(IDS_A11Y_OMNIBOX_CHIP_HINT);
  ASSERT_FALSE(expected_announcement.empty());
  ASSERT_FALSE(address_bar_hint.empty());

  std::u16string captured_announcement;
  EXPECT_CALL(mock_panel_controller(), AnnounceAlert(testing::_))
      .WillOnce(testing::SaveArg<0>(&captured_announcement));

  const GURL url("https://example.com");
  NavigateAndCommit(url);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);
  pscs->OnMediaStreamPermissionSet(
      url, {content_settings::PageSpecificContentSettings::kMicrophoneAccessed,
            content_settings::PageSpecificContentSettings::kMicrophoneBlocked});

  EXPECT_EQ(captured_announcement, expected_announcement);
  EXPECT_EQ(captured_announcement.find(address_bar_hint), std::u16string::npos);
}

}  // namespace contextual_tasks
