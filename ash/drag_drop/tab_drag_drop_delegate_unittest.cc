// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/vector2d.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace ash {

namespace {

class MockShellDelegate : public TestShellDelegate {
 public:
  MockShellDelegate() = default;
  ~MockShellDelegate() override = default;

  MOCK_METHOD(bool, IsTabDrag, (const ui::OSExchangeData&), (override));

  MOCK_METHOD(aura::Window*,
              CreateBrowserForTabDrop,
              (aura::Window*, const ui::OSExchangeData&),
              (override));
};

}  // namespace

class TabDragDropDelegateTest : public AshTestBase {
 public:
  TabDragDropDelegateTest() {
    ash::features::SetWebUITabStripEnabled(true);
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kWebUITabStripTabDragIntegration);
  }

  // AshTestBase:
  void SetUp() override {
    auto mock_shell_delegate = std::make_unique<NiceMock<MockShellDelegate>>();
    mock_shell_delegate_ = mock_shell_delegate.get();
    AshTestBase::SetUp(std::move(mock_shell_delegate));
    ash::TabletModeControllerTestApi().EnterTabletMode();

    // Create a dummy window and exit overview mode since drags can't be
    // initiated from overview mode.
    dummy_window_ = CreateToplevelTestWindow();
    ASSERT_TRUE(Shell::Get()->overview_controller()->EndOverview());
  }

  void TearDown() override {
    // Must be deleted before AshTestBase's tear down.
    dummy_window_.reset();

    // Clear our pointer before the object is destroyed.
    mock_shell_delegate_ = nullptr;
    AshTestBase::TearDown();
  }

  MockShellDelegate* mock_shell_delegate() { return mock_shell_delegate_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockShellDelegate>* mock_shell_delegate_ = nullptr;

  std::unique_ptr<aura::Window> dummy_window_;
};

TEST_F(TabDragDropDelegateTest, ForwardsDragCheckToShellDelegate) {
  ON_CALL(*mock_shell_delegate(), IsTabDrag(_)).WillByDefault(Return(false));
  EXPECT_FALSE(TabDragDropDelegate::IsChromeTabDrag(ui::OSExchangeData()));

  ON_CALL(*mock_shell_delegate(), IsTabDrag(_)).WillByDefault(Return(true));
  EXPECT_TRUE(TabDragDropDelegate::IsChromeTabDrag(ui::OSExchangeData()));
}

TEST_F(TabDragDropDelegateTest, DragToExistingTabStrip) {
  // Create a fake source window. Its details don't matter.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // A new window shouldn't be created in this case.
  EXPECT_CALL(*mock_shell_delegate(), CreateBrowserForTabDrop(_, _)).Times(0);

  // Emulate a drag session whose drop target accepts the drop. In this
  // case, TabDragDropDelegate::Drop() is not called.
  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                               source_window.get(), gfx::Point(0, 0));
  delegate.DragUpdate(gfx::Point(1, 0));
  delegate.DragUpdate(gfx::Point(2, 0));

  // Let |delegate| be destroyed without a Drop() call.
}

TEST_F(TabDragDropDelegateTest, DragToNewWindow) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  // Emulate a drag session ending in a drop to a new window.
  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                               source_window.get(), drag_start_location);
  delegate.DragUpdate(drag_start_location);
  delegate.DragUpdate(drag_start_location + gfx::Vector2d(1, 0));
  delegate.DragUpdate(drag_start_location + gfx::Vector2d(2, 0));

  // Check that a new window is requested. Assume the correct drop data
  // is passed. Return the new window.
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_shell_delegate(),
              CreateBrowserForTabDrop(source_window.get(), _))
      .Times(1)
      .WillOnce(Return(new_window.get()));

  delegate.Drop(drag_start_location + gfx::Vector2d(2, 0),
                ui::OSExchangeData());

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());
}

TEST_F(TabDragDropDelegateTest, DropOnEdgeEntersSplitView) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // We want to avoid entering overview mode between the delegate.Drop()
  // call and |new_window|'s destruction. So we define it here before
  // creating it.
  std::unique_ptr<aura::Window> new_window;

  // Emulate a drag to the right edge of the screen.
  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  const gfx::Point drag_end_location =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get())
          .right_center();

  TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                               source_window.get(), drag_start_location);
  delegate.DragUpdate(drag_start_location);
  delegate.DragUpdate(drag_end_location);

  new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_shell_delegate(),
              CreateBrowserForTabDrop(source_window.get(), _))
      .Times(1)
      .WillOnce(Return(new_window.get()));

  delegate.Drop(drag_end_location, ui::OSExchangeData());

  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  EXPECT_EQ(new_window.get(), split_view_controller->GetSnappedWindow(
                                  SplitViewController::SnapPosition::RIGHT));
  EXPECT_EQ(source_window.get(), split_view_controller->GetSnappedWindow(
                                     SplitViewController::SnapPosition::LEFT));
}

TEST_F(TabDragDropDelegateTest, SourceWindowBoundsUpdatedWhileDragging) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  const gfx::Rect original_bounds = source_window->bounds();

  // Drag a few pixels away to trigger window scaling, then to the
  // screen edge to visually snap the source window.
  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  const gfx::Point drag_mid_location =
      drag_start_location + gfx::Vector2d(10, 0);
  const gfx::Point drag_end_location =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get())
          .left_center();

  {
    TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                                 source_window.get(), drag_start_location);
    delegate.DragUpdate(drag_start_location);
    delegate.DragUpdate(drag_mid_location);

    // |source_window| should be shrunk in all directions
    EXPECT_GT(source_window->bounds().x(), original_bounds.x());
    EXPECT_GT(source_window->bounds().y(), original_bounds.y());
    EXPECT_LT(source_window->bounds().right(), original_bounds.right());
    EXPECT_LT(source_window->bounds().bottom(), original_bounds.bottom());

    delegate.DragUpdate(drag_end_location);

    // |source_window| should appear in the snapped position, but not
    // actually be snapped.
    SplitViewController* const split_view_controller =
        SplitViewController::Get(source_window.get());
    EXPECT_EQ(
        source_window->bounds(),
        split_view_controller->GetSnappedWindowBoundsInParent(
            SplitViewController::SnapPosition::RIGHT, source_window.get()));
    EXPECT_FALSE(split_view_controller->InSplitViewMode());
  }

  // The original bounds should be restored.
  EXPECT_EQ(source_window->bounds(), original_bounds);
}

TEST_F(TabDragDropDelegateTest, SnappedSourceWindowNotMoved) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  SplitViewController::SnapPosition const snap_position =
      SplitViewController::SnapPosition::LEFT;
  split_view_controller->SnapWindow(source_window.get(), snap_position);
  const gfx::Rect original_bounds = source_window->bounds();

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  const gfx::Point drag_end_location =
      drag_start_location + gfx::Vector2d(10, 0);

  {
    TabDragDropDelegate delegate(Shell::GetPrimaryRootWindow(),
                                 source_window.get(), drag_start_location);
    delegate.DragUpdate(drag_start_location);
    delegate.DragUpdate(drag_end_location);

    // |source_window| should remain snapped and it's bounds should not change.
    EXPECT_EQ(source_window.get(),
              split_view_controller->GetSnappedWindow(snap_position));
    EXPECT_EQ(original_bounds, source_window->bounds());
  }

  // Everything should still be the same after the drag ends.
  EXPECT_EQ(source_window.get(),
            split_view_controller->GetSnappedWindow(snap_position));
  EXPECT_EQ(original_bounds, source_window->bounds());
}

}  // namespace ash
