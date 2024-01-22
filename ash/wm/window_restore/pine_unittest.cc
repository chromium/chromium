// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_utils.h"

namespace ash {

class PineTest : public AshTestBase {
 public:
  PineTest() = default;
  PineTest(const PineTest&) = delete;
  PineTest& operator=(const PineTest&) = delete;
  ~PineTest() override = default;

 private:
  InProcessDataDecoder decoder_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kPine};
};

TEST_F(PineTest, Show) {
  Shell::Get()->window_restore_controller()->MaybeStartPineOverviewSession();

  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  EXPECT_EQ(OverviewEnterExitType::kPine,
            overview_session->enter_exit_overview_type());
}

TEST_F(PineTest, ShowContextMenuOnSettingsButtonClicked) {
  base::RunLoop run_loop;
  OverviewController::Get()->set_pine_callback_for_test(run_loop.QuitClosure());
  Shell::Get()->window_restore_controller()->MaybeStartPineOverviewSession();
  run_loop.Run();

  // Get the active Pine widget.
  OverviewGrid* grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(grid);
  auto* pine_widget = grid->pine_widget_for_testing();
  ASSERT_TRUE(pine_widget);

  // The context menu should not be open.
  PineContentsView* const contents_view =
      views::AsViewClass<PineContentsView>(pine_widget->GetContentsView());
  ASSERT_TRUE(contents_view);
  PineContextMenuModel* context_menu = contents_view->context_menu_model_.get();
  EXPECT_FALSE(context_menu);

  // Click on the settings button, the context menu should appear.
  LeftClickOn(contents_view->settings_button_view_.get());
  EXPECT_TRUE(contents_view->context_menu_model_.get());
}

}  // namespace ash
