// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/window_util.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace ash {

namespace {

constexpr int kWebUITabStripHeight = 100;

class MockShellDelegate : public TestShellDelegate {
 public:
  MockShellDelegate() = default;
  ~MockShellDelegate() override = default;

  MOCK_METHOD(bool, IsTabDrag, (const ui::OSExchangeData&), (override));

  int GetBrowserWebUITabStripHeight() override { return kWebUITabStripHeight; }
};

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MockNewWindowDelegate() = default;
  ~MockNewWindowDelegate() override = default;

  MOCK_METHOD(void,
              NewWindowForDetachingTab,
              (aura::Window*,
               const ui::OSExchangeData&,
               NewWindowForDetachingTabCallback),
              (override));
};

}  // namespace

class TabDragDropDelegateTest : public AshTestBase {
 public:
  TabDragDropDelegateTest() = default;

  // AshTestBase:
  void SetUp() override {
    mock_new_window_delegate_ =
        std::make_unique<NiceMock<MockNewWindowDelegate>>();

    auto mock_shell_delegate = std::make_unique<NiceMock<MockShellDelegate>>();
    mock_shell_delegate_ = mock_shell_delegate.get();
    AshTestBase::SetUp(std::move(mock_shell_delegate));
    ash::TabletModeControllerTestApi().EnterTabletMode();

    // Create a dummy window and exit overview mode since drags can't be
    // initiated from overview mode.
    dummy_window_ = CreateToplevelTestWindow();
    ASSERT_TRUE(ExitOverview());
  }

  void TearDown() override {
    // Must be deleted before AshTestBase's tear down.
    dummy_window_.reset();

    // Clear our pointer before the object is destroyed.
    mock_shell_delegate_ = nullptr;

    mock_new_window_delegate_.reset();

    AshTestBase::TearDown();
  }

  MockShellDelegate* mock_shell_delegate() { return mock_shell_delegate_; }

  MockNewWindowDelegate* mock_new_window_delegate() {
    return static_cast<MockNewWindowDelegate*>(
        NewWindowDelegate::GetInstance());
  }

 private:
  std::unique_ptr<NiceMock<MockNewWindowDelegate>> mock_new_window_delegate_;
  raw_ptr<NiceMock<MockShellDelegate>> mock_shell_delegate_ = nullptr;
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
  EXPECT_CALL(*mock_new_window_delegate(), NewWindowForDetachingTab(_, _, _))
      .Times(0);

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
  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate->DragUpdate(drag_start_location);
  delegate->DragUpdate(drag_start_location + gfx::Vector2d(1, 0));
  delegate->DragUpdate(drag_start_location + gfx::Vector2d(2, 0));

  // Check that a new window is requested. Assume the correct drop data
  // is passed. Return the new window.
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));

  delegate.release()->DropAndDeleteSelf(
      drag_start_location + gfx::Vector2d(2, 0), ui::OSExchangeData());

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());
}

// When a tab is dragged to the left/right side of the Web Contents. It should
// enter split view.
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

  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate->DragUpdate(drag_start_location);
  delegate->DragUpdate(drag_end_location);

  new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));

  delegate.release()->DropAndDeleteSelf(drag_end_location,
                                        ui::OSExchangeData());

  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  EXPECT_EQ(new_window.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kSecondary));
  EXPECT_EQ(source_window.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kPrimary));
}

// When a tab is dragged to the left/right edge of the tab strip. It should not
// enter split view.
// https://crbug.com/1316070
TEST_F(TabDragDropDelegateTest, DropOnEdgeShouldNotEnterSplitView) {
  // Create the source window. This should automatically fill the work area
  // since we're in tablet mode.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // We want to avoid entering overview mode between the delegate.Drop()
  // call and |new_window|'s destruction. So we define it here before
  // creating it.
  std::unique_ptr<aura::Window> new_window;

  // Emulate a drag to the right edge of the tab strip. It should not enter
  // split view.
  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  const gfx::Point drag_end_location =
      gfx::Point(source_window->bounds().right(), kWebUITabStripHeight * 0.5);

  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate->DragUpdate(drag_start_location);
  delegate->DragUpdate(drag_end_location);

  new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));

  delegate.release()->DropAndDeleteSelf(drag_end_location,
                                        ui::OSExchangeData());

  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  EXPECT_FALSE(split_view_controller->InTabletSplitViewMode());
}

TEST_F(TabDragDropDelegateTest, DropTabInSplitViewMode) {
  // Enter tablet split view mode by snap the source window to the left.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  split_view_controller->SnapWindow(source_window.get(),
                                    SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  // Snap another window to the right to make sure right split screen is not in
  // overview mode.
  std::unique_ptr<aura::Window> right_window = CreateToplevelTestWindow();
  split_view_controller->SnapWindow(right_window.get(),
                                    SnapPosition::kSecondary);

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  auto area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get());

  // Emulate a drag to the right side of the screen.
  // |new_window1| should snap to the right split view.
  gfx::Point drag_end_location_right(area.width() * 0.8, area.height() * 0.5);
  auto delegate1 = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate1->DragUpdate(drag_start_location);
  delegate1->DragUpdate(drag_end_location_right);
  std::unique_ptr<aura::Window> new_window1 = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window1.get()));
  delegate1.release()->DropAndDeleteSelf(drag_end_location_right,
                                         ui::OSExchangeData());

  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  EXPECT_EQ(new_window1.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kSecondary));
  EXPECT_EQ(source_window.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kPrimary));
  new_window1.reset();  // Close |new_window1|.

  // Emulate a drag to the left side of the screen.
  // |new_window2| should snap to the left split view.
  // |source_window| should go into overview mode.
  gfx::Point drag_end_location_left(area.width() * 0.2, area.height() * 0.5);
  auto delegate2 = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate2->DragUpdate(drag_start_location);
  delegate2->DragUpdate(drag_end_location_left);
  std::unique_ptr<aura::Window> new_window2 = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window2.get()));
  delegate2.release()->DropAndDeleteSelf(drag_end_location_left,
                                         ui::OSExchangeData());

  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  EXPECT_EQ(nullptr,
            split_view_controller->GetSnappedWindow(SnapPosition::kSecondary));
  EXPECT_EQ(new_window2.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kPrimary));
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(
      base::Contains(GetWindowsListInOverviewGrids(), source_window.get()));
}

TEST_F(TabDragDropDelegateTest, DropTabToOverviewMode) {
  // Enter tablet split view mode by snap the source window to the left.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  split_view_controller->SnapWindow(source_window.get(),
                                    SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  auto area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get());

  // Emulate a drag to the right side of the screen.
  // |new_window1| should snap to overview mode.
  gfx::Point drag_end_location_right(area.width() * 0.8, area.height() * 0.5);
  auto delegate1 = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate1->DragUpdate(drag_start_location);
  delegate1->DragUpdate(drag_end_location_right);
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));
  delegate1.release()->DropAndDeleteSelf(drag_end_location_right,
                                         ui::OSExchangeData());

  EXPECT_EQ(nullptr,
            split_view_controller->GetSnappedWindow(SnapPosition::kSecondary));
  EXPECT_TRUE(
      base::Contains(GetWindowsListInOverviewGrids(), new_window.get()));
}

TEST_F(TabDragDropDelegateTest, WillNotDropTabToOverviewModeInSnappingZone) {
  // Enter tablet split view mode by snap the source window to the left.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  split_view_controller->SnapWindow(source_window.get(),
                                    SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  auto area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get());

  // Emulate a drag to the right snapping zone of the screen.
  // |new_window1| should not snap to overview mode.
  gfx::Point drag_end_location_right(area.width() * 0.95, area.height() * 0.5);
  auto delegate1 = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate1->DragUpdate(drag_start_location);
  delegate1->DragUpdate(drag_end_location_right);
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));
  delegate1.release()->DropAndDeleteSelf(drag_end_location_right,
                                         ui::OSExchangeData());

  EXPECT_EQ(new_window.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kSecondary));
  ASSERT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(TabDragDropDelegateTest, WillNotDropTabToOverviewMode) {
  // Enter tablet split view mode by snap the source window to the left.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  SplitViewController* const split_view_controller =
      SplitViewController::Get(source_window.get());
  split_view_controller->SnapWindow(source_window.get(),
                                    SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InTabletSplitViewMode());
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();
  auto area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          source_window.get());

  // Emulate a drag to the left side of the screen.
  // |new_window1| should not snap to overview mode.
  gfx::Point drag_end_location_right(area.width() * 0.2, area.height() * 0.5);
  auto delegate1 = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate1->DragUpdate(drag_start_location);
  delegate1->DragUpdate(drag_end_location_right);
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));
  delegate1.release()->DropAndDeleteSelf(drag_end_location_right,
                                         ui::OSExchangeData());

  EXPECT_EQ(new_window.get(),
            split_view_controller->GetSnappedWindow(SnapPosition::kPrimary));
  EXPECT_FALSE(
      base::Contains(GetWindowsListInOverviewGrids(), new_window.get()));
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
    EXPECT_EQ(source_window->bounds(),
              split_view_controller->GetSnappedWindowBoundsInParent(
                  SnapPosition::kSecondary, source_window.get(),
                  chromeos::kDefaultSnapRatio));
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
  SnapPosition const snap_position = SnapPosition::kPrimary;
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

// Make sure metrics is recorded during tab dragging in tablet mode with
// webui tab strip enable.
TEST_F(TabDragDropDelegateTest, TabDraggingHistogram) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());

  const gfx::Point drag_start_location = source_window->bounds().CenterPoint();

  // Emulate a drag session ending in a drop to a new window. This should
  // generate a histogram.
  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_start_location);
  delegate->DragUpdate(drag_start_location + gfx::Vector2d(1, 0));
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(
      source_window->layer()->GetCompositor()));

  // Check that a new window is requested. Assume the correct drop data
  // is passed. Return the new window.
  std::unique_ptr<aura::Window> new_window = CreateToplevelTestWindow();
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(RunOnceCallback<2>(new_window.get()));
  delegate.release()->DropAndDeleteSelf(
      drag_start_location + gfx::Vector2d(1, 0), ui::OSExchangeData());
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(
      source_window->layer()->GetCompositor()));

  EXPECT_FALSE(
      SplitViewController::Get(source_window.get())->InTabletSplitViewMode());
  histogram_tester.ExpectTotalCount("Ash.TabDrag.PresentationTime.TabletMode",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Ash.TabDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

// There are edge cases where a dragging tab closes itself before being dropped.
// In these cases new window will be nullptr and it
// should be handled gracefully. https://crbug.com/1286203
TEST_F(TabDragDropDelegateTest, DropWithoutNewWindow) {
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  const gfx::Point drag_location = source_window->bounds().CenterPoint();
  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(), drag_location);
  delegate->OnNewBrowserWindowCreated(drag_location, /*new_window=*/nullptr);
}

// Tests that if tab dragging is started on a floated window and then canceled,
// the float window returns to its original bounds.
TEST_F(TabDragDropDelegateTest, CancelTabDragWithFloatedWindow) {
  // Create a floated window.
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();
  source_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  wm::ActivateWindow(source_window.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(source_window.get())->IsFloated());
  const gfx::Rect original_bounds = source_window->GetBoundsInScreen();

  // Simulate tab dragging from the floated source window.
  auto delegate = std::make_unique<TabDragDropDelegate>(
      Shell::GetPrimaryRootWindow(), source_window.get(),
      source_window->bounds().CenterPoint());
  delegate.reset();
  EXPECT_EQ(original_bounds, source_window->GetBoundsInScreen());
}

TEST_F(TabDragDropDelegateTest, CaptureShouldBeReleasedAfterDrop) {
  std::unique_ptr<aura::Window> source_window =
      CreateToplevelTestWindow(gfx::Rect(0, 0, 10, 10));

  constexpr gfx::Point kDragStartLocation(5, 5);

  // Emulate a drag session ending in a drop to a new window.
  auto* delegate = new TabDragDropDelegate(
      Shell::GetPrimaryRootWindow(), source_window.get(), kDragStartLocation);

  delegate->TakeCapture(Shell::GetPrimaryRootWindow(), source_window.get(),
                        base::BindLambdaForTesting([]() {}),
                        ui::TransferTouchesBehavior::kCancel);

  delegate->DragUpdate(kDragStartLocation);
  delegate->DragUpdate(kDragStartLocation + gfx::Vector2d(10, 0));

  // Input capture should still be active.
  EXPECT_TRUE(ash::window_util::GetCaptureWindow());

  NewWindowDelegate::NewWindowForDetachingTabCallback new_window_callback;
  EXPECT_CALL(*mock_new_window_delegate(),
              NewWindowForDetachingTab(source_window.get(), _, _))
      .Times(1)
      .WillOnce(
          [&](aura::Window* source_window, const ui::OSExchangeData& drop_data,
              NewWindowDelegate::NewWindowForDetachingTabCallback callback) {
            new_window_callback = std::move(callback);
          });

  delegate->DropAndDeleteSelf(kDragStartLocation + gfx::Vector2d(10, 0),
                              ui::OSExchangeData());

  // Input capture should have been released.
  EXPECT_FALSE(ash::window_util::GetCaptureWindow());

  std::move(new_window_callback).Run(source_window.get());
}

}  // namespace ash
