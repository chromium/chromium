// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_window_utils.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/app_restore/features.h"
#include "components/exo/wm_helper.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"

namespace {
const int TEST_DISPLAY_ID = 0x1387;
const int TEST_DISPLAY_WIDTH = 2560;
const int TEST_DISPLAY_HEIGHT = 1440;
const double TEST_SCALE_FACTOR = 2.0;
}  // namespace

namespace ash::full_restore {

class ArcWindowUtilsTest : public testing::Test {
 protected:
  ArcWindowUtilsTest() = default;
  ArcWindowUtilsTest(const ArcWindowUtilsTest&) = delete;
  ArcWindowUtilsTest& operator=(const ArcWindowUtilsTest&) = delete;
  ~ArcWindowUtilsTest() override = default;

  void SetUp() override {
    const display::Display test_display = test_screen_.GetPrimaryDisplay();
    display::Display display(test_display);
    display.set_id(TEST_DISPLAY_ID);
    display.set_bounds(
        gfx::Rect(0, 0, TEST_DISPLAY_WIDTH, TEST_DISPLAY_HEIGHT));
    display.set_device_scale_factor(TEST_SCALE_FACTOR);
    test_screen_.display_list().RemoveDisplay(test_display.id());
    test_screen_.display_list().AddDisplay(display,
                                           display::DisplayList::Type::PRIMARY);
    display::Screen::SetScreenInstance(&test_screen_);
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        {"", "--enable-arcvm"});
    wm_helper_ = std::make_unique<exo::WMHelper>();
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());
  }

  void TearDown() override {
    test_user_session_manager_.reset();
    wm_helper_.reset();
    display::Screen::SetScreenInstance(nullptr);
  }

 private:
  display::test::TestScreen test_screen_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
};

TEST_F(ArcWindowUtilsTest, ArcWindowInfoInvalidDisplayValidBoundsTest) {
  apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>();

  window_info->display_id = display::kInvalidDisplayId;

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_FALSE(arc_window_info->bounds.has_value());
}

TEST_F(ArcWindowUtilsTest, ArcWindowInfoValidDisplayInvalidBoundsTest) {
  apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>();

  window_info->display_id = TEST_DISPLAY_ID;

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_FALSE(arc_window_info->bounds.has_value());
}

TEST_F(ArcWindowUtilsTest, ArcWindowInfoValidDisplayAndBoundsTest) {
  apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>();
  window_info->display_id = TEST_DISPLAY_ID;
  window_info->bounds = gfx::Rect(100, 200, 300, 400);

  auto arc_window_info = HandleArcWindowInfo(std::move(window_info));
  EXPECT_TRUE(arc_window_info->bounds.has_value());
  EXPECT_EQ(arc_window_info->bounds->x(), 200);
  EXPECT_EQ(arc_window_info->bounds->y(), 400);
  EXPECT_EQ(arc_window_info->bounds->width(), 600);
  EXPECT_EQ(arc_window_info->bounds->height(), 800);
}

}  // namespace ash::full_restore
