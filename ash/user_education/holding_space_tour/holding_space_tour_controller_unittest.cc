// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_tour/holding_space_tour_controller.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/mock_holding_space_client.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/holding_space/holding_space_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/test_widget_builder.h"
#include "ash/user_education/tutorial_controller.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "base/pickle.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Aliases.
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Pair;
using user_education::TutorialDescription;

// Helpers ---------------------------------------------------------------------

HoldingSpaceTray* GetHoldingSpaceTrayForShelf(Shelf* shelf) {
  return shelf->GetStatusAreaWidget()->holding_space_tray();
}

aura::Window* GetRootWindowForDisplayId(int64_t display_id) {
  return Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
      display_id);
}

Shelf* GetShelfForDisplayId(int64_t display_id) {
  return Shelf::ForWindow(GetRootWindowForDisplayId(display_id));
}

std::unique_ptr<views::Widget> CreateTestWidgetForDisplayId(
    int64_t display_id) {
  return TestWidgetBuilder()
      .SetWidgetType(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS)
      .SetContext(GetRootWindowForDisplayId(display_id))
      .BuildOwnsNativeWidget();
}

void SetFilesAppData(ui::OSExchangeData* data) {
  base::Pickle pickled_data;
  ui::WriteCustomDataToPickle(
      std::unordered_map<std::u16string, std::u16string>(
          {{u"fs/sources", u"filesystem:filepath"}}),
      &pickled_data);

  // NOTE: The Files app stores file system sources as custom web data.
  data->SetPickledData(ui::ClipboardFormatType::WebCustomDataType(),
                       pickled_data);
}

// DraggableView ---------------------------------------------------------------

// TODO(http://b/279211692): Modify and reuse `DraggableTestView`.
// A view supporting drag operations that relies on a `delegate_` to write data.
class DraggableView : public views::View {
 public:
  using Delegate = base::RepeatingCallback<void(ui::OSExchangeData*)>;
  explicit DraggableView(Delegate delegate) : delegate_(std::move(delegate)) {}

 private:
  // views::View:
  int GetDragOperations(const gfx::Point&) override {
    return ui::DragDropTypes::DragOperation::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point&, ui::OSExchangeData* data) override {
    delegate_.Run(data);
  }

  // The delegate for writing drag data.
  Delegate delegate_;
};

}  // namespace

// HoldingSpaceTourControllerTest ----------------------------------------------

// Base class for tests of the `HoldingSpaceTourController`.
class HoldingSpaceTourControllerTest : public UserEducationAshTestBase {
 public:
  HoldingSpaceTourControllerTest() {
    // NOTE: The `HoldingSpaceTourController` exists only when the Holding Space
    // Tour feature is enabled. Controller existence is verified in test
    // coverage for the controller's owner.
    scoped_feature_list_.InitAndEnableFeature(features::kHoldingSpaceTour);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `GetTutorialDescriptions()` returns expected values.
TEST_F(HoldingSpaceTourControllerTest, GetTutorialDescriptions) {
  auto* holding_space_tour_controller = HoldingSpaceTourController::Get();
  ASSERT_TRUE(holding_space_tour_controller);

  std::map<TutorialId, TutorialDescription> tutorial_descriptions_by_id =
      static_cast<TutorialController*>(holding_space_tour_controller)
          ->GetTutorialDescriptions();

  // TODO(http://b/275909980): Implement tutorial descriptions.
  EXPECT_THAT(
      tutorial_descriptions_by_id,
      ElementsAre(Pair(Eq(TutorialId::kHoldingSpaceTourPrototype1), _),
                  Pair(Eq(TutorialId::kHoldingSpaceTourPrototype2), _)));
}

// HoldingSpaceTourControllerDragAndDropTest -----------------------------------

// Base class for drag-and-drop tests of the `HoldingSpaceTourController`,
// parameterized by (a) whether to drag Files app data and (b) whether to
// complete the drop (as opposed to cancelling it).
class HoldingSpaceTourControllerDragAndDropTest
    : public HoldingSpaceTourControllerTest,
      public testing::WithParamInterface<
          std::tuple</*drag_files_app_data=*/bool, /*complete_drop=*/bool>> {
 public:
  // Whether to drag Files app data given test parameterization.
  bool drag_files_app_data() const { return std::get<0>(GetParam()); }

  // Whether to complete the drop (as opposed to cancelling it) given test
  // parameterization.
  bool complete_drop() const { return std::get<1>(GetParam()); }

  // Moves the mouse to the center of the specified `widget`.
  void MoveMouseTo(views::Widget* widget) {
    GetEventGenerator()->MoveMouseTo(
        widget->GetWindowBoundsInScreen().CenterPoint(), /*count=*/10);
  }

  // Moves the mouse by the specified `x` and `y` offsets.
  void MoveMouseBy(int x, int y) {
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(
        event_generator->current_screen_location() + gfx::Vector2d(x, y),
        /*count=*/10);
  }

  // Presses and releases the key associated with the specified `key_code`.
  void PressAndReleaseKey(ui::KeyboardCode key_code) {
    GetEventGenerator()->PressAndReleaseKey(key_code);
  }

  // Presses/releases the left mouse button.
  void PressLeftButton() { GetEventGenerator()->PressLeftButton(); }
  void ReleaseLeftButton() { GetEventGenerator()->ReleaseLeftButton(); }

 private:
  // HoldingSpaceTourControllerTest:
  void SetUp() override {
    HoldingSpaceTourControllerTest::SetUp();

    // Prevent blocking during drag-and-drop sequences.
    ShellTestApi().drag_drop_controller()->set_should_block_during_drag_drop(
        false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceTourControllerDragAndDropTest,
    testing::Combine(/*drag_files_app_data=*/testing::Bool(),
                     /*complete_drop=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that the `HoldingSpaceTourController` handles drag-and-drop events
// as expected.
TEST_P(HoldingSpaceTourControllerDragAndDropTest, DragAndDrop) {
  // The holding space tray is always visible in the shelf when the
  // predictability feature is enabled. Force disable it so that we verify that
  // holding space visibility is updated by the `HoldingSpaceTourController`.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kHoldingSpacePredictability);

  // Set up a primary and secondary display and cache IDs.
  UpdateDisplay("1024x768,1024x768");
  const int64_t primary_display_id = GetPrimaryDisplay().id();
  const int64_t secondary_display_id = GetSecondaryDisplay().id();

  // Log in a regular user.
  const AccountId& account_id = AccountId::FromUserEmail("user@test");
  SimulateUserLogin(account_id);

  // Register a model and client for holding space.
  HoldingSpaceModel holding_space_model;
  MockHoldingSpaceClient holding_space_client;
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client, &holding_space_model);

  // Create and show a widget on the primary display from which data can be
  // drag-and-dropped.
  auto primary_widget = CreateTestWidgetForDisplayId(primary_display_id);
  primary_widget->SetContentsView(std::make_unique<DraggableView>(
      base::BindLambdaForTesting([&](ui::OSExchangeData* data) {
        data->SetString(u"Payload");
        if (drag_files_app_data()) {
          SetFilesAppData(data);
        }
      })));
  primary_widget->CenterWindow(gfx::Size(100, 100));
  primary_widget->Show();

  // Create and show a widget on the secondary display.
  auto secondary_widget = CreateTestWidgetForDisplayId(secondary_display_id);
  secondary_widget->CenterWindow(gfx::Size(100, 100));
  secondary_widget->Show();

  // Cache both shelves and holding space trays.
  auto* const primary_shelf = GetShelfForDisplayId(primary_display_id);
  auto* const secondary_shelf = GetShelfForDisplayId(secondary_display_id);
  auto* const primary_tray = GetHoldingSpaceTrayForShelf(primary_shelf);
  auto* const secondary_tray = GetHoldingSpaceTrayForShelf(secondary_shelf);

  // Set auto-hide behavior and verify that neither shelf is visible.
  primary_shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  secondary_shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(primary_shelf->IsVisible());
  EXPECT_FALSE(secondary_shelf->IsVisible());

  // Verify that neither holding space tray is visible.
  EXPECT_FALSE(primary_tray->GetVisible());
  EXPECT_FALSE(secondary_tray->GetVisible());

  // Drag data from the `primary_widget` to the wallpaper.
  MoveMouseTo(primary_widget.get());
  PressLeftButton();
  MoveMouseBy(/*x=*/primary_widget->GetWindowBoundsInScreen().width(), /*y=*/0);

  // Expect the primary shelf and both holding space trays to be visible if
  // and only if Files app data was dragged.
  EXPECT_EQ(primary_shelf->IsVisible(), drag_files_app_data());
  EXPECT_EQ(primary_tray->GetVisible(), drag_files_app_data());
  EXPECT_EQ(secondary_tray->GetVisible(), drag_files_app_data());
  EXPECT_FALSE(secondary_shelf->IsVisible());

  // Drag the data to the `secondary_widget`.
  MoveMouseTo(secondary_widget.get());

  // Expect the secondary shelf and both holding space trays to be visible if
  // and only if Files app data was dragged.
  EXPECT_EQ(secondary_shelf->IsVisible(), drag_files_app_data());
  EXPECT_EQ(primary_tray->GetVisible(), drag_files_app_data());
  EXPECT_EQ(secondary_tray->GetVisible(), drag_files_app_data());
  EXPECT_FALSE(primary_shelf->IsVisible());

  // Conditionally complete or cancel the drop depending on test
  // parameterization.
  if (complete_drop()) {
    ReleaseLeftButton();
  } else {
    PressAndReleaseKey(ui::VKEY_ESCAPE);
  }

  // Expect both shelves and holding space trays to no longer be visible.
  EXPECT_FALSE(primary_shelf->IsVisible());
  EXPECT_FALSE(secondary_shelf->IsVisible());
  EXPECT_FALSE(primary_tray->GetVisible());
  EXPECT_FALSE(secondary_tray->GetVisible());

  // Clean up holding space controller.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, /*client=*/nullptr, /*model=*/nullptr);
}

}  // namespace ash
