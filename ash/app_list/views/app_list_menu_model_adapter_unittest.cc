// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ash {

class AppListMenuModelAdapterTest : public views::ViewsTestBase {
 public:
  AppListMenuModelAdapterTest() {}
  ~AppListMenuModelAdapterTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    app_list_menu_model_adapter_ = std::make_unique<AppListMenuModelAdapter>(
        "test-app-id", std::make_unique<ui::SimpleMenuModel>(nullptr), nullptr,
        ui::MenuSourceType::MENU_SOURCE_TYPE_LAST, AppLaunchedMetricParams(),
        AppListMenuModelAdapter::FULLSCREEN_APP_GRID, base::OnceClosure(),
        false /* is_tablet_mode */);
  }

  std::unique_ptr<AppListMenuModelAdapter> app_list_menu_model_adapter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListMenuModelAdapterTest);
};

// Tests that NOTIFICATION_CONTAINER is enabled. This ensures that the
// container is able to handle gesture events.
TEST_F(AppListMenuModelAdapterTest, NotificationContainerEnabled) {
  EXPECT_TRUE(app_list_menu_model_adapter_->IsCommandEnabled(
      ash::NOTIFICATION_CONTAINER));
}

}  // namespace ash
