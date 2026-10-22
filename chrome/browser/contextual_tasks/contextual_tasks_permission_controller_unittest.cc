// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"

#include "base/test/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_chip.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_util.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace contextual_tasks {

class ContextualTasksPermissionControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_browser_window_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    controller_ = std::make_unique<ContextualTasksPermissionController>(
        &mock_browser_window_);
    controller_->RegisterWebContents(web_contents());
    controller_->Update(web_contents());
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ContextualTasksPermissionController* controller() {
    return controller_.get();
  }

 private:
  ui::UnownedUserDataHost unowned_user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
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
      features::IsRoundedIconsEnabled() ? kPhotoCameraIcon : kCameraOldIcon;
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
  EXPECT_EQ(state->request_chip->icon_name, icon.name);
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
      /*is_mouse_interaction=*/true);

  EXPECT_TRUE(pressed_callback_ran);
  EXPECT_TRUE(pressed_with_pointer);
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

}  // namespace contextual_tasks
