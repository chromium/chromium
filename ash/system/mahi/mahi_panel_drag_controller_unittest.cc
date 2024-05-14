// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_drag_controller.h"

#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class MahiPanelDragControllerTest : public AshTestBase {
 public:
  MahiPanelDragControllerTest() {
    ON_CALL(mock_mahi_manager_, IsEnabled).WillByDefault(Return(true));
  }
  ~MahiPanelDragControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    ui_controller_.OpenMahiPanel(GetPrimaryDisplay().id());
  }

  void TearDown() override {
    ui_controller_.CloseMahiPanel();
    AshTestBase::TearDown();
  }

  MahiUiController& ui_controller() { return ui_controller_; }

 private:
  base::test::ScopedFeatureList feature_list_{chromeos::features::kMahi};
  MahiUiController ui_controller_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  chromeos::ScopedMahiManagerSetter scoped_manager_setter_{&mock_mahi_manager_};
};

TEST_F(MahiPanelDragControllerTest, MouseDragRepositionsPanel) {
  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  constexpr gfx::Rect kInitialBounds(100, 100, 200, 300);
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->DragMouseBy(kDragOffset.x(), kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

TEST_F(MahiPanelDragControllerTest, GestureDragRepositionsPanel) {
  views::Widget* panel_widget = ui_controller().mahi_panel_widget();
  constexpr gfx::Rect kInitialBounds(100, 100, 200, 300);
  panel_widget->SetBounds(kInitialBounds);

  GetEventGenerator()->set_current_screen_location(
      panel_widget->GetWindowBoundsInScreen().origin() + gfx::Vector2d(20, 20));
  constexpr gfx::Vector2d kDragOffset(20, 10);
  GetEventGenerator()->PressMoveAndReleaseTouchBy(kDragOffset.x(),
                                                  kDragOffset.y());

  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(),
            kInitialBounds + kDragOffset);
}

}  // namespace

}  // namespace ash
