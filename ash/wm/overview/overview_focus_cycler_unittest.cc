// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

#include "ash/constants/ash_features.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class OverviewFocusCyclerTest : public OverviewTestBase {
 public:
  OverviewFocusCyclerTest() = default;
  OverviewFocusCyclerTest(const OverviewFocusCyclerTest&) = delete;
  OverviewFocusCyclerTest& operator=(const OverviewFocusCyclerTest&) = delete;
  ~OverviewFocusCyclerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kOverviewNewFocus};
};

// Temporary test to make sure we don't have critical problems with the flag
// enabled. These should be covered in separate tests once the feature is done.
// TODO(http://b/325335020): Remove this test once the feature is complete.
TEST_F(OverviewFocusCyclerTest, NoCrashOnTab) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());

  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  ToggleOverview();
  for (int i = 0; i < 15; ++i) {
    SendKey(ui::VKEY_TAB);
  }
}

}  // namespace ash
