// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"

#include "base/test/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_dashboard.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_util.h"
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

  std::unique_ptr<content::WebContents> second_web_contents =
      CreateTestWebContents();
  controller()->RegisterWebContents(second_web_contents.get());
  EXPECT_EQ(
      location_bar::GetLocationBarForWebContents(second_web_contents.get()),
      location_bar);

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

  auto* request_chip =
      static_cast<ContextualTasksPermissionChip*>(dashboard->GetRequestChip());
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
  auto* request_chip =
      static_cast<ContextualTasksPermissionChip*>(dashboard->GetRequestChip());

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

}  // namespace contextual_tasks
