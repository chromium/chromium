// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/contextual_tasks/public/features.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_anchor.h"

namespace contextual_tasks {
namespace {

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestChipElementId);

// Vector icons can't be safely constructed dynamically, but a file-scope
// constant mirrors what the generated icon constants look like.
constexpr gfx::VectorIcon kTestIcon(nullptr, 0u, "test_icon");
constexpr gfx::VectorIcon kOtherTestIcon(nullptr, 0u, "other_test_icon");

class ContextualTasksPermissionChipTest : public testing::Test {
 public:
  void SetUp() override {
    chip_ = std::make_unique<ContextualTasksPermissionChip>(
        &location_bar_, kTestChipElementId,
        base::BindLambdaForTesting([this]() { ++update_state_count_; }));
  }

  void TearDown() override { chip_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ContextualTasksLocationBar location_bar_{nullptr};
  int update_state_count_ = 0;
  std::unique_ptr<ContextualTasksPermissionChip> chip_;
};

TEST_F(ContextualTasksPermissionChipTest, SetVisibleStoresState) {
  EXPECT_FALSE(chip_->GetVisible());
  EXPECT_FALSE(chip_->GetState()->is_visible);

  chip_->SetVisible(true);

  EXPECT_TRUE(chip_->GetVisible());
  EXPECT_TRUE(chip_->GetState()->is_visible);
  // Visibility changes must be pushed to the WebUI.
  EXPECT_EQ(update_state_count_, 1);

  chip_->SetVisible(false);

  EXPECT_FALSE(chip_->GetVisible());
  EXPECT_FALSE(chip_->GetState()->is_visible);
  EXPECT_EQ(update_state_count_, 2);
}

TEST_F(ContextualTasksPermissionChipTest, SetMessageStoresMessage) {
  EXPECT_TRUE(chip_->GetTextForTesting().empty());
  EXPECT_TRUE(chip_->GetState()->message.empty());

  const std::u16string message = u"Use your camera?";
  chip_->SetMessage(message);

  EXPECT_EQ(chip_->GetTextForTesting(), message);
  EXPECT_EQ(chip_->GetState()->message, message);
  EXPECT_EQ(update_state_count_, 1);

  // A later message replaces the previous one.
  const std::u16string new_message = u"Use your microphone?";
  chip_->SetMessage(new_message);

  EXPECT_EQ(chip_->GetTextForTesting(), new_message);
  EXPECT_EQ(chip_->GetState()->message, new_message);
  EXPECT_EQ(update_state_count_, 2);
}

TEST_F(ContextualTasksPermissionChipTest, SetChipIconStoresIconName) {
  EXPECT_TRUE(chip_->GetState()->icon_name.empty());

  // Native vector icons are serialized to the WebUI by name.
  chip_->SetChipIcon(kTestIcon);

  EXPECT_EQ(chip_->GetState()->icon_name, kTestIcon.name);
  EXPECT_EQ(update_state_count_, 1);

  // The pointer overload stores the icon name as well.
  chip_->SetChipIcon(&kOtherTestIcon);

  EXPECT_EQ(chip_->GetState()->icon_name, kOtherTestIcon.name);
  EXPECT_EQ(update_state_count_, 2);
}

TEST_F(ContextualTasksPermissionChipTest, GetStateSerializesAllFields) {
  const std::u16string message = u"Use your location?";
  const std::u16string tooltip = u"Location permission";
  const std::u16string accessibility_name = u"Location permission chip";

  chip_->SetVisible(true);
  chip_->SetChipIcon(kTestIcon);
  chip_->SetMessage(message);
  chip_->SetTooltipText(tooltip);
  chip_->SetTheme(PermissionChipTheme::kLowVisibility);
  chip_->SetUserDecision(permissions::PermissionAction::DENIED);
  chip_->SetBlockedIconShowing(true);
  chip_->SetPermissionPromptStyle(PermissionPromptStyle::kQuietChip);
  chip_->SetAccessibilityName(accessibility_name);

  toolbar_ui_api::mojom::PermissionChipStatePtr state = chip_->GetState();

  ASSERT_TRUE(state);
  EXPECT_TRUE(state->is_visible);
  EXPECT_EQ(state->icon_name, kTestIcon.name);
  EXPECT_EQ(state->message, message);
  EXPECT_EQ(state->tooltip, tooltip);
  EXPECT_EQ(state->theme,
            toolbar_ui_api::mojom::PermissionChipTheme::kLowVisibility);
  EXPECT_EQ(state->user_decision,
            toolbar_ui_api::mojom::PermissionAction::kDenied);
  EXPECT_TRUE(state->should_show_blocked_icon);
  EXPECT_EQ(state->prompt_style,
            toolbar_ui_api::mojom::PermissionPromptStyle::kQuietChip);
  EXPECT_EQ(state->accessibility_name, accessibility_name);
}

TEST_F(ContextualTasksPermissionChipTest,
       GetStateSerializesTargetCollapsedState) {
  // Unlike the native chip, the serialized `is_fully_collapsed` is the *target*
  // state that drives the frontend's CSS transition, not the current state.
  EXPECT_TRUE(chip_->GetState()->is_fully_collapsed);

  chip_->AnimateExpand(base::Milliseconds(350));

  EXPECT_FALSE(chip_->GetState()->is_fully_collapsed);

  chip_->AnimateCollapse(base::Milliseconds(350));

  EXPECT_TRUE(chip_->GetState()->is_fully_collapsed);
  EXPECT_FALSE(chip_->IsFullyCollapsed());
}

TEST_F(ContextualTasksPermissionChipTest, GetAnchorIsNullWithoutWebView) {
  // Neither a tracked element (the stub location bar has no WebContents) nor a
  // fallback container view is available.
  EXPECT_TRUE(chip_->GetAnchor().IsNull());
}

class MockContextualTasksLocationBar : public ContextualTasksLocationBar {
 public:
  explicit MockContextualTasksLocationBar(BrowserWindowInterface* browser)
      : ContextualTasksLocationBar(browser) {}
  MOCK_METHOD(ui::TrackedElement*, GetAnchorOrNull, (), (override));
};

// Tests for GetAnchor()'s fallback behavior, which requires a real
// ContextualTasksWebView to anchor to.
class ContextualTasksPermissionChipAnchorTestBase : public testing::Test {
 public:
  explicit ContextualTasksPermissionChipAnchorTestBase(
      bool rearchitecture_enabled) {
    if (rearchitecture_enabled) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{kContextualTasks,
                                kContextualTasksSidePanelRearchitecture},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{kContextualTasks},
          /*disabled_features=*/{kContextualTasksSidePanelRearchitecture});
    }
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("testing_profile");
    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_window_, GetProfile()).WillByDefault(Return(profile_));
    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    web_view_ = std::make_unique<ContextualTasksWebView>(browser_window_.get());
    location_bar_ =
        std::make_unique<testing::NiceMock<MockContextualTasksLocationBar>>(
            browser_window_.get());
    chip_ = std::make_unique<ContextualTasksPermissionChip>(location_bar_.get(),
                                                            kTestChipElementId);
  }

  void TearDown() override {
    // Destroy in reverse creation order: the chip and the location bar hold
    // raw pointers to the location bar and the browser window respectively.
    chip_.reset();
    location_bar_.reset();
    web_view_.reset();
    browser_window_.reset();
    profile_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  BrowserWindowFeatures browser_window_features_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksLocationBar>>
      location_bar_;
  std::unique_ptr<ContextualTasksWebView> web_view_;
  std::unique_ptr<ContextualTasksPermissionChip> chip_;
};

class ContextualTasksPermissionChipAnchorTest
    : public ContextualTasksPermissionChipAnchorTestBase {
 public:
  ContextualTasksPermissionChipAnchorTest()
      : ContextualTasksPermissionChipAnchorTestBase(
            /*rearchitecture_enabled=*/true) {}
};

class ContextualTasksPermissionChipNoToolbarAnchorTest
    : public ContextualTasksPermissionChipAnchorTestBase {
 public:
  ContextualTasksPermissionChipNoToolbarAnchorTest()
      : ContextualTasksPermissionChipAnchorTestBase(
            /*rearchitecture_enabled=*/false) {}
};

TEST_F(ContextualTasksPermissionChipAnchorTest,
       GetAnchorFallsBackToLocationBarAnchor) {
  EXPECT_CALL(*location_bar_, GetAnchorOrNull())
      .WillOnce(testing::Return(nullptr));

  views::BubbleAnchor anchor = chip_->GetAnchor();

  EXPECT_TRUE(anchor.IsNull());
}

TEST_F(ContextualTasksPermissionChipNoToolbarAnchorTest,
       GetAnchorFallsBackToLocationBarAnchor) {
  EXPECT_CALL(*location_bar_, GetAnchorOrNull())
      .WillOnce(testing::Return(nullptr));

  views::BubbleAnchor anchor = chip_->GetAnchor();

  EXPECT_TRUE(anchor.IsNull());
}

}  // namespace
}  // namespace contextual_tasks
