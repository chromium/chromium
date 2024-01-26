// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_ash.h"

#include <memory>

#include "ash/test/ash_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MahiManagerAshTest : public testing::Test {
 public:
  MahiManagerAshTest() = default;

  MahiManagerAshTest(const MahiManagerAshTest&) = delete;
  MahiManagerAshTest& operator=(const MahiManagerAshTest&) = delete;

  ~MahiManagerAshTest() override = default;

  // testing::Test:
  void SetUp() override {
    ash_test_helper_.SetUp();

    mahi_manager_ash_ = std::make_unique<MahiManagerAsh>();
  }

  void TearDown() override { ash_test_helper_.TearDown(); }

  views::Widget* GetMahiPanelWidget() {
    if (!mahi_manager_ash_->mahi_panel_widget_) {
      return nullptr;
    }
    return mahi_manager_ash_->mahi_panel_widget_->AsWidget();
  }

 protected:
  // This instance is needed for setting up `ash_test_helper_`.
  // See //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment task_environment_;

  // Need this to set up `Shell` and display.
  AshTestHelper ash_test_helper_;
  std::unique_ptr<MahiManagerAsh> mahi_manager_ash_;
};

TEST_F(MahiManagerAshTest, OpenPanel) {
  EXPECT_FALSE(GetMahiPanelWidget());

  auto* screen = display::Screen::GetScreen();
  auto display_id = screen->GetPrimaryDisplay().id();

  mahi_manager_ash_->OpenMahiPanel(display_id);

  // Widget should be created.
  auto* widget = GetMahiPanelWidget();
  EXPECT_TRUE(widget);

  // The widget should be in the same display as the given `display_id`.
  EXPECT_EQ(display_id,
            screen->GetDisplayNearestWindow(widget->GetNativeWindow()).id());
}

}  // namespace ash
