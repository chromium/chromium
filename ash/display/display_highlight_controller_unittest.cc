// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_highlight_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/display/manager/display_manager.h"

namespace ash {

class DisplayHighlightControllerTest : public AshTestBase {
 public:
  DisplayHighlightControllerTest() = default;
  ~DisplayHighlightControllerTest() override = default;

  void LockScreen() { GetSessionControllerClient()->LockScreen(); }

  DisplayHighlightController* display_highlight_controller() const {
    return Shell::Get()->display_highlight_controller();
  }

  void ExpectNoHighlight() {
    EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);
  }

  void ExpectHighlightWithDisplay(const display::Display target) {
    views::Widget* widget =
        display_highlight_controller()->GetWidgetForTesting();
    const int64_t display_id = target.id();

    EXPECT_EQ(target, display_manager()->FindDisplayContainingPoint(
                          widget->GetWindowBoundsInScreen().origin()));

    ASSERT_NE(widget, nullptr);
    EXPECT_EQ(widget->GetNativeWindow()->GetRootWindow(),
              Shell::GetRootWindowForDisplayId(display_id));
    EXPECT_EQ(widget->GetWindowBoundsInScreen(), target.bounds());
    EXPECT_TRUE(widget->IsVisible());
  }
};

TEST_F(DisplayHighlightControllerTest, OnDisplayChangedNoDisplaySelected) {
  UpdateDisplay("1920x1080");

  ExpectNoHighlight();
}

TEST_F(DisplayHighlightControllerTest, SetHighlightedDisplaySingle) {
  UpdateDisplay("1920x1080");

  const display::Display& display = display_manager()->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);
}

TEST_F(DisplayHighlightControllerTest, OnDisplayChangedInvalidDisplaySelected) {
  UpdateDisplay("1920x1080");

  display_highlight_controller()->SetHighlightedDisplay(
      display::kInvalidDisplayId);

  ExpectNoHighlight();
}

TEST_F(DisplayHighlightControllerTest, SetHighlightedDisplayCycleMultiple) {
  UpdateDisplay("1920x1080,800x600,1366x768");

  DisplayHighlightController* highlight_controller =
      display_highlight_controller();
  display::DisplayManager* display_manager_ptr = display_manager();
  const display::Display& display = display_manager_ptr->GetDisplayAt(0);

  highlight_controller->SetHighlightedDisplay(display.id());

  EXPECT_NE(display.id(), display::kInvalidDisplayId);

  ExpectHighlightWithDisplay(display);

  const display::Display& second_display = display_manager_ptr->GetDisplayAt(1);
  highlight_controller->SetHighlightedDisplay(second_display.id());

  ExpectHighlightWithDisplay(second_display);

  const display::Display& third_display = display_manager_ptr->GetDisplayAt(2);
  highlight_controller->SetHighlightedDisplay(third_display.id());

  ExpectHighlightWithDisplay(third_display);
}

TEST_F(DisplayHighlightControllerTest, AddDisplay) {
  UpdateDisplay("1920x1080");

  display::DisplayManager* display_manager_ptr = display_manager();

  const display::Display& display = display_manager_ptr->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);

  UpdateDisplay("1920x1080,800x600");

  const display::Display& updated_display =
      display_manager_ptr->GetDisplayAt(0);

  ExpectHighlightWithDisplay(updated_display);
}

TEST_F(DisplayHighlightControllerTest, NotVisibleUnifiedMode) {
  UpdateDisplay("1920x1080,800x600");

  display::DisplayManager* display_manager_ptr = display_manager();
  display_manager_ptr->SetUnifiedDesktopEnabled(true);

  const display::Display& display = display_manager_ptr->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);
}

TEST_F(DisplayHighlightControllerTest, NotVisibleMirroredMode) {
  UpdateDisplay("1920x1080,800x600");

  display::DisplayManager* display_manager_ptr = display_manager();
  display_manager_ptr->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager_ptr->UpdateDisplays();

  const display::Display& display = display_manager_ptr->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);
}

TEST_F(DisplayHighlightControllerTest, VisibleMixedMode) {
  UpdateDisplay("300x400,400x500,500x600");

  // Turn on mixed mirror mode. (Mirror from the first display to the second
  // display)
  display::DisplayManager* display_manager_ptr = display_manager();
  display::DisplayIdList id_list =
      display_manager_ptr->GetConnectedDisplayIdList();
  display::DisplayIdList dst_ids;

  dst_ids.emplace_back(id_list[1]);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, id_list[0], dst_ids);

  display_manager_ptr->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);

  const display::Display& display = display_manager_ptr->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  ExpectHighlightWithDisplay(display);
}

TEST_F(DisplayHighlightControllerTest, LockedScreenHighlightVisiblity) {
  UpdateDisplay("1920x1080,800x600,1366x768");

  const display::Display& display = display_manager()->GetDisplayAt(0);

  display_highlight_controller()->SetHighlightedDisplay(display.id());

  EXPECT_NE(display.id(), display::kInvalidDisplayId);

  ExpectHighlightWithDisplay(display);

  // Lock screen
  LockScreen();

  EXPECT_EQ(display_highlight_controller()->GetWidgetForTesting(), nullptr);
}

}  // namespace ash
