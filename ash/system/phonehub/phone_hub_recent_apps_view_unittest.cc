// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_apps_view.h"

#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class RecentAppButtonsViewTest : public AshTestBase {
 public:
  RecentAppButtonsViewTest() = default;
  ~RecentAppButtonsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    phone_hub_recent_apps_view_ = std::make_unique<PhoneHubRecentAppsView>();
  }

  void TearDown() override {
    phone_hub_recent_apps_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  PhoneHubRecentAppsView* recent_apps_view() {
    return phone_hub_recent_apps_view_.get();
  }

 private:
  std::unique_ptr<PhoneHubRecentAppsView> phone_hub_recent_apps_view_;
};

TEST_F(RecentAppButtonsViewTest, TaskViewVisibility) {
  // The view should not be shown when tab sync is not enabled.
  recent_apps_view()->recent_app_button_list_.clear();
  recent_apps_view()->Update();
  EXPECT_FALSE(recent_apps_view()->GetVisible());

  recent_apps_view()->recent_app_button_list_.push_back(
      std::make_unique<PhoneHubRecentAppButton>());
  recent_apps_view()->Update();
  EXPECT_TRUE(recent_apps_view()->GetVisible());
}

TEST_F(RecentAppButtonsViewTest, RecentAppButtonsView) {
  // TODO(paulzchen): Check real expected number when the recent app button
  // using real data from phone.
  size_t expected_recent_app_button = 1;
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());
}

}  // namespace ash