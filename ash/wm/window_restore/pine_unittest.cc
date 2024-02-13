// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_contents_view.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/in_process_data_decoder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_context_menu_model.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_utils.h"

namespace ash {

// TODO(b/322358447): Add unit tests for overflow view.

class PineTest : public AshTestBase {
 public:
  PineTest() { switches::SetIgnoreForestSecretKeyForTest(true); }
  PineTest(const PineTest&) = delete;
  PineTest& operator=(const PineTest&) = delete;
  ~PineTest() override { switches::SetIgnoreForestSecretKeyForTest(false); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kForestFeature};
};

TEST_F(PineTest, Show) {
  Shell::Get()
      ->pine_controller()
      ->MaybeStartPineOverviewSessionDevAccelerator();
  WaitForOverviewEntered();

  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  EXPECT_EQ(OverviewEnterExitType::kPine,
            overview_session->enter_exit_overview_type());
}

}  // namespace ash
