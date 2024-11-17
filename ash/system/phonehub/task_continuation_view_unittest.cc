// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/task_continuation_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/phonehub/continue_browsing_chip.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using BrowserTabsModel = phonehub::BrowserTabsModel;

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class TaskContinuationViewTest : public AshTestBase {
 public:
  TaskContinuationViewTest() = default;
  ~TaskContinuationViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kPhoneHub);
    AshTestBase::SetUp();

    task_continuation_view_ = std::make_unique<TaskContinuationView>(
        &phone_model_, &fake_user_action_recorder_);
  }

  void TearDown() override {
    task_continuation_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  TaskContinuationView* task_view() { return task_continuation_view_.get(); }
  phonehub::MutablePhoneModel* phone_model() { return &phone_model_; }
  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 private:
  std::unique_ptr<TaskContinuationView> task_continuation_view_;
  phonehub::FakeUserActionRecorder fake_user_action_recorder_;
  phonehub::MutablePhoneModel phone_model_;
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
      phonehub::CreateFakeBrowserTabMetadata();

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
      phonehub::CreateFakeBrowserTabMetadata();

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

  for (views::View* child : task_view()->chips_view_->children()) {
    ContinueBrowsingChip* chip = static_cast<ContinueBrowsingChip*>(child);
    // OpenUrl is expected to call after button pressed simulation.
    EXPECT_CALL(new_window_delegate(),
                OpenUrl(GURL("https://www.example.com/tab1"),
                        NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                        NewWindowDelegate::Disposition::kNewForegroundTab));
    // Simulate clicking button using dummy event.
    views::test::ButtonTestApi(chip).NotifyClick(ui::test::TestEvent());
  }
}

}  // namespace ash
