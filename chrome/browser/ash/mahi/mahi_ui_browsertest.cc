// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/test/ash_test_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MahiUiBrowserTest : public InProcessBrowserTest {
 protected:
  ui::test::EventGenerator& event_generator() { return *event_generator_; }

 private:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kUseFakeMahiManager);
  }

  void SetUpOnMainThread() override {
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());
  }

  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      ash::switches::SetIgnoreMahiSecretKeyForTest();
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

IN_PROC_BROWSER_TEST_F(MahiUiBrowserTest, OnContextMenuClickedSummary) {
  EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));

  // Open the Mahi menu by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const mahi_menu_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_menu_widget);

  // Open the Mahi panel by left clicking the menu's summary button.
  const views::View* const summary_button =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kSummaryButton);
  ASSERT_TRUE(summary_button);
  event_generator().MoveMouseTo(
      summary_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // Check the existence of the Mahi panel.
  EXPECT_TRUE(FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName()));
}

}  // namespace ash
