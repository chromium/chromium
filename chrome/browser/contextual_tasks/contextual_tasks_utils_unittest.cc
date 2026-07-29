// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace contextual_tasks {

namespace {

// Fake class to stub the layout of BrowserActions so we can inspect
// and set root_action_item_ in testing without triggering full
// BrowserActions initialization.
struct FakeBrowserActions {
  raw_ptr<actions::ActionItem> root_action_item = nullptr;
};

class ContextualTasksUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    InitializeActionIdStringMapping();
    profile_ = std::make_unique<TestingProfile>();
    browser_window_ = std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

  void TearDown() override {
    actions::ActionIdMap::ResetMapsForTesting();
    actions::ActionManager::Get().ResetForTesting();
    actions::ActionIdMap::ResetMapsForTesting();
    browser_window_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>> browser_window_;
};

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullWindow) {
  // Should return early and not crash.
  UpdatePinButtonVisibilityState(nullptr, true);
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullActions) {
  UpdatePinButtonVisibilityState(browser_window_.get(), true);
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_NullRootActionItem) {
  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = nullptr;
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  UpdatePinButtonVisibilityState(browser_window_.get(), true);
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_ActionNotFound) {
  // Root action item with no children (no contextual tasks action item).
  std::unique_ptr<actions::ActionItem> root_action =
      actions::ActionItem::Builder().Build();

  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = root_action.get();
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  UpdatePinButtonVisibilityState(browser_window_.get(), true);
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Eligible_Pinned) {
  std::unique_ptr<actions::ActionItem> root_action =
      actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(false)
          .SetEnabled(true)
          .Build());

  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = root_action.get();
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  UpdatePinButtonVisibilityState(browser_window_.get(), true);

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Pinned) {
  std::unique_ptr<actions::ActionItem> root_action =
      actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = root_action.get();
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  UpdatePinButtonVisibilityState(browser_window_.get(), false);

  EXPECT_FALSE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Unpinned) {
  std::unique_ptr<actions::ActionItem> root_action =
      actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = root_action.get();
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  ASSERT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));

  // Should NOT hide the action item because it is unpinned.
  UpdatePinButtonVisibilityState(browser_window_.get(), false);

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksUtilsTest, UpdatePinButtonVisibilityState_Ineligible_Pinned_IncognitoProfile) {
  std::unique_ptr<actions::ActionItem> root_action =
      actions::ActionItem::Builder().Build();
  actions::ActionItem* action_item = root_action->AddChild(
      actions::ActionItem::Builder()
          .SetActionId(kActionSidePanelShowContextualTasks)
          .SetVisible(true)
          .SetEnabled(true)
          .Build());

  FakeBrowserActions fake_actions;
  fake_actions.root_action_item = root_action.get();
  ui::ScopedUnownedUserData<BrowserActions> scoped_actions(
      browser_window_->GetUnownedUserDataHost(),
      *reinterpret_cast<BrowserActions*>(&fake_actions));

  // Setup original profile and OTR profile
  Profile* otr_profile = profile_->GetPrimaryOTRProfile(true);
  ON_CALL(*browser_window_, GetProfile())
      .WillByDefault(testing::Return(otr_profile));

  auto* model = PinnedToolbarActionsModel::Get(profile_.get());
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  ASSERT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  // Should NOT hide the action item because we are in an incognito window.
  UpdatePinButtonVisibilityState(browser_window_.get(), false);

  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));
}

}  // namespace

}  // namespace contextual_tasks
