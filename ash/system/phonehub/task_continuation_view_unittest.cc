// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/task_continuation_view.h"

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/phonehub/continue_browsing_chip.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/components/phonehub/phone_model_test_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

using BrowserTabsModel = chromeos::phonehub::BrowserTabsModel;

namespace {

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              NewTabWithUrl,
              (const GURL& url, bool from_user_interaction),
              (override));
};

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

}  // namespace

class TaskContinuationViewTest : public AshTestBase {
 public:
  TaskContinuationViewTest() = default;
  ~TaskContinuationViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    task_continuation_view_ =
        std::make_unique<TaskContinuationView>(&phone_model_);
  }

  void TearDown() override {
    task_continuation_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TaskContinuationView* task_view() { return task_continuation_view_.get(); }
  chromeos::phonehub::MutablePhoneModel* phone_model() { return &phone_model_; }
  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 private:
  std::unique_ptr<TaskContinuationView> task_continuation_view_;
  chromeos::phonehub::MutablePhoneModel phone_model_;
  base::test::ScopedFeatureList feature_list_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(TaskContinuationViewTest, TaskViewVisibility) {
  phone_model()->SetBrowserTabsModel(
      BrowserTabsModel(false /* is_tab_sync_enabled */));
  // The view should not be shown when tab sync is not enabled.
  EXPECT_FALSE(task_view()->GetVisible());

  phone_model()->SetBrowserTabsModel(
      BrowserTabsModel(true /* is_tab_sync_enabled */));
  // The view should not be shown when tab sync is enabled but no browser tabs
  // open.
  EXPECT_FALSE(task_view()->GetVisible());

  BrowserTabsModel::BrowserTabMetadata metadata =
      chromeos::phonehub::CreateFakeBrowserTabMetadata();

  std::vector<BrowserTabsModel::BrowserTabMetadata> tabs = {metadata};

  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The view should be shown when there is one browser tab.
  EXPECT_TRUE(task_view()->GetVisible());

  tabs.push_back(metadata);
  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The view should be shown when there are two or more browser tabs.
  EXPECT_TRUE(task_view()->GetVisible());
}

TEST_F(TaskContinuationViewTest, TaskChipsView) {
  BrowserTabsModel::BrowserTabMetadata metadata =
      chromeos::phonehub::CreateFakeBrowserTabMetadata();

  std::vector<BrowserTabsModel::BrowserTabMetadata> tabs = {metadata};

  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The chips view should contains 1 tab.
  size_t expected_tabs = 1;
  EXPECT_EQ(expected_tabs, task_view()->chips_view_->children().size());

  tabs.push_back(metadata);
  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The chips view should contains 2 tab.
  expected_tabs = 2;
  EXPECT_EQ(expected_tabs, task_view()->chips_view_->children().size());

  tabs.push_back(metadata);
  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The chips view should contains 3 tab.
  expected_tabs = 3;
  EXPECT_EQ(expected_tabs, task_view()->chips_view_->children().size());

  tabs.push_back(metadata);
  phone_model()->SetBrowserTabsModel(BrowserTabsModel(true, tabs));
  // The chips view should contains 4 tab.
  expected_tabs = 4;
  EXPECT_EQ(expected_tabs, task_view()->chips_view_->children().size());

  for (auto* child : task_view()->chips_view_->children()) {
    ContinueBrowsingChip* chip = static_cast<ContinueBrowsingChip*>(child);
    // NewTabWithUrl is expected to call after button pressed simulation.
    EXPECT_CALL(new_window_delegate(), NewTabWithUrl)
        .WillOnce([](const GURL& url, bool from_user_interaction) {
          EXPECT_EQ(GURL("https://www.example.com/tab1"), url);
          EXPECT_TRUE(from_user_interaction);
        });
    // Simulate clicking button using dummy event.
    chip->ButtonPressed(nullptr, DummyEvent());
  }
}

}  // namespace ash
