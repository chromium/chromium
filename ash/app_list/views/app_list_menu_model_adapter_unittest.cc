// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Tests that NOTIFICATION_CONTAINER is enabled. This ensures that the
// container is able to handle gesture events.
TEST(AppListMenuModelAdapterTest, NotificationContainerEnabled) {
  AppListMenuModelAdapter adapter(
      "test-app-id", std::make_unique<ui::SimpleMenuModel>(nullptr), nullptr,
      ui::MenuSourceType::MENU_SOURCE_TYPE_LAST, AppLaunchedMetricParams(),
      AppListMenuModelAdapter::FULLSCREEN_APP_GRID, base::OnceClosure(),
      false /* is_tablet_mode */);
  EXPECT_TRUE(adapter.IsCommandEnabled(NOTIFICATION_CONTAINER));
}

}  // namespace
}  // namespace ash
