// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"

#include <string>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using ::chromeos::WindowStateType;

// A helper function to set the shelf auto-hide preference. This has the same
// effect as the user toggling the shelf context menu option.
void SetShelfAutoHideBehaviorPref(int64_t display_id,
                                  ShelfAutoHideBehavior behavior) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return;
  SetShelfAutoHideBehaviorPref(prefs, display_id, behavior);
}

class TabletModeWindowManagerTest : public AshTestBase {
 public:
  TabletModeWindowManagerTest() = default;

  TabletModeWindowManagerTest(const TabletModeWindowManagerTest&) = delete;
  TabletModeWindowManagerTest& operator=(const TabletModeWindowManagerTest&) =
      delete;

  ~TabletModeWindowManagerTest() override = default;

  // Initialize parameters for test windows.  If |can_maximize| is not
  // set, |max_size| is the upper limiting size for the window,
  // whereas an empty size means that there is no limit.
  struct InitParams {
    explicit InitParams(aura::client::WindowType t) : type(t) {}

    aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL;
    gfx::Rect bounds;
    gfx::Size max_size;
    bool can_maximize = true;
    bool can_resize = true;
    bool show_on_creation = true;
  };

  // Creates a window which has a fixed size.
  aura::Window* CreateFixedSizeNonMaximizableWindow(
      aura::client::WindowType type,
      const gfx::Rect& bounds) {
    InitParams params(type);
    params.bounds = bounds;
    params.can_maximize = false;
    params.can_resize = false;
    return CreateWindowInWatchedContainer(params);
  }

  // Creates a window which can not be maximized, but resized. |max_size|
  // denotes the maximal possible size, if the size is empty, the window has no
  // upper limit. Note: This function will only work with a single root window.
  aura::Window* CreateNonMaximizableWindow(aura::client::WindowType type,
                                           const gfx::Rect& bounds,
                                           const gfx::Size& max_size) {
    InitParams params(type);
    params.bounds = bounds;
    params.max_size = max_size;
    params.can_maximize = false;
    return CreateWindowInWatchedContainer(params);
  }

  // Creates a maximizable and resizable window.
  aura::Window* CreateWindow(aura::client::WindowType type,
                             const gfx::Rect bounds) {
    InitParams params(type);
    params.bounds = bounds;
    return CreateWindowInWatchedContainer(params);
  }

  // Creates a window which also has a widget.
  aura::Window* CreateWindowWithWidget(const gfx::Rect& bounds) {
    views::Widget* widget =
        views::Widget::CreateWindowWithContext(nullptr, GetContext(), bounds);
    widget->Show();
    // Note: The widget will get deleted with the window.
    return widget->GetNativeWindow();
  }

  // Create the tablet mode window manager.
  TabletModeWindowManager* CreateTabletModeWindowManager() {
    EXPECT_FALSE(TabletModeControllerTestApi().tablet_mode_window_manager());
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    return TabletModeControllerTestApi().tablet_mode_window_manager();
  }

  // Destroy the tablet mode window manager.
  void DestroyTabletModeWindowManager() {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
    EXPECT_FALSE(TabletModeControllerTestApi().tablet_mode_window_manager());
  }

  // Resize our desktop.
  void ResizeDesktop(int width_delta) {
    gfx::Size size =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow())
            .size();
    size.Enlarge(0, width_delta);
    UpdateDisplay(size.ToString());
  }

  // Create a window in one of the containers which are watched by the
  // TabletModeWindowManager. Note that this only works with one root window.
  aura::Window* CreateWindowInWatchedContainer(const InitParams& params) {
    aura::test::TestWindowDelegate* delegate = NULL;
    if (!params.can_maximize) {
      delegate = aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
      delegate->set_window_component(HTCAPTION);
      if (!params.max_size.IsEmpty())
        delegate->set_maximum_size(params.max_size);
    }
    aura::Window* window = aura::test::CreateTestWindowWithDelegateAndType(
        delegate, params.type, 0, params.bounds, NULL, params.show_on_creation);
    int32_t behavior = aura::client::kResizeBehaviorNone |
                       aura::client::kResizeBehaviorCanFullscreen;
    behavior |= params.can_resize ? aura::client::kResizeBehaviorCanResize : 0;
    behavior |=
        params.can_maximize ? aura::client::kResizeBehaviorCanMaximize : 0;
    window->SetProperty(aura::client::kResizeBehaviorKey, behavior);
    aura::Window* container =
        GetSwitchableContainersForRoot(Shell::GetPrimaryRootWindow(),
                                       /*active_desk_only=*/true)[0];
    container->AddChild(window);
    return window;
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }
};

// Test that creating the object and destroying it without any windows should
// not cause any problems.
TEST_F(TabletModeWindowManagerTest, SimpleStart) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyTabletModeWindowManager();
}

// Test that existing windows will handled properly when going into tablet
// mode.
TEST_F(TabletModeWindowManagerTest, PreCreateWindows) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  gfx::Rect rect3(20, 140, 100, 100);
  // Bounds for anything else.
  gfx::Rect rect(80, 90, 100, 110);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
  std::unique_ptr<aura::Window> w2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect2));
  std::unique_ptr<aura::Window> w3(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect3));
  std::unique_ptr<aura::Window> w4(
      CreateWindow(aura::client::WINDOW_TYPE_POPUP, rect));
  std::unique_ptr<aura::Window> w5(
      CreateWindow(aura::client::WINDOW_TYPE_MENU, rect));
  std::unique_ptr<aura::Window> w6(
      CreateWindow(aura::client::WINDOW_TYPE_TOOLTIP, rect));
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());

  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_NE(rect3.origin().ToString(), w3->bounds().origin().ToString());
  EXPECT_EQ(rect3.size().ToString(), w3->bounds().size().ToString());

  // All other windows should not have been touched.
  EXPECT_FALSE(WindowState::Get(w4.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w5.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w6.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());

  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
}

// The same test as the above but while a system modal dialog is shown.
TEST_F(TabletModeWindowManagerTest, GoingToMaximizedWithModalDialogPresent) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  gfx::Rect rect3(20, 140, 100, 100);
  // Bounds for anything else.
  gfx::Rect rect(80, 90, 100, 110);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
  std::unique_ptr<aura::Window> w2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect2));
  std::unique_ptr<aura::Window> w3(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect3));
  std::unique_ptr<aura::Window> w4(
      CreateWindow(aura::client::WINDOW_TYPE_POPUP, rect));
  std::unique_ptr<aura::Window> w5(
      CreateWindow(aura::client::WINDOW_TYPE_MENU, rect));
  std::unique_ptr<aura::Window> w6(
      CreateWindow(aura::client::WINDOW_TYPE_TOOLTIP, rect));
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());

  // Enable system modal dialog, and make sure both shelves are still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_NE(rect3.origin().ToString(), w3->bounds().origin().ToString());
  EXPECT_EQ(rect3.size().ToString(), w3->bounds().size().ToString());

  // All other windows should not have been touched.
  EXPECT_FALSE(WindowState::Get(w4.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w5.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w6.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());

  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
}

// Test that non-maximizable windows get properly handled when going into
// tablet mode.
TEST_F(TabletModeWindowManagerTest,
       PreCreateNonMaximizableButResizableWindows) {
  // The window bounds.
  gfx::Rect rect(10, 10, 200, 50);
  gfx::Size max_size(300, 200);
  gfx::Size empty_size;
  std::unique_ptr<aura::Window> unlimited_window(CreateNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect, empty_size));
  std::unique_ptr<aura::Window> limited_window(CreateNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect, max_size));
  std::unique_ptr<aura::Window> fixed_window(
      CreateFixedSizeNonMaximizableWindow(aura::client::WINDOW_TYPE_NORMAL,
                                          rect));
  EXPECT_FALSE(WindowState::Get(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());

  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  // The unlimited window should have the size of the workspace / parent window.
  EXPECT_FALSE(WindowState::Get(unlimited_window.get())->IsMaximized());
  EXPECT_EQ("0,0", unlimited_window->bounds().origin().ToString());
  const gfx::Size workspace_size_tablet_mode =
      screen_util::GetMaximizedWindowBoundsInParent(unlimited_window.get())
          .size();
  EXPECT_EQ(workspace_size_tablet_mode.ToString(),
            unlimited_window->bounds().size().ToString());
  // The limited window should have the size of the upper possible bounds.
  EXPECT_FALSE(WindowState::Get(limited_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            limited_window->bounds().origin().ToString());
  EXPECT_EQ(max_size.ToString(), limited_window->bounds().size().ToString());
  // The fixed size window should have the size of the original window.
  EXPECT_FALSE(WindowState::Get(fixed_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            fixed_window->bounds().origin().ToString());
  EXPECT_EQ(rect.size().ToString(), fixed_window->bounds().size().ToString());

  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());
}

// Test that creating windows while a maximizer exists picks them properly up.
TEST_F(TabletModeWindowManagerTest, CreateWindows) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());

  // Create the windows and see that the window manager picks them up.
  // Rects for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  gfx::Rect rect3(20, 140, 100, 100);
  // One rect for anything else.
  gfx::Rect rect(80, 90, 100, 110);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
  std::unique_ptr<aura::Window> w2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect2));
  std::unique_ptr<aura::Window> w3(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect3));
  std::unique_ptr<aura::Window> w4(
      CreateWindow(aura::client::WINDOW_TYPE_POPUP, rect));
  std::unique_ptr<aura::Window> w5(
      CreateWindow(aura::client::WINDOW_TYPE_MENU, rect));
  std::unique_ptr<aura::Window> w6(
      CreateWindow(aura::client::WINDOW_TYPE_TOOLTIP, rect));
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());

  // Make sure that the position of the unresizable window is in the middle of
  // the screen.
  gfx::Size work_area_size =
      screen_util::GetDisplayWorkAreaBoundsInParent(w3.get()).size();
  gfx::Point center =
      gfx::Point((work_area_size.width() - rect3.size().width()) / 2,
                 (work_area_size.height() - rect3.size().height()) / 2);
  gfx::Rect centered_window_bounds = gfx::Rect(center, rect3.size());
  EXPECT_EQ(centered_window_bounds.ToString(), w3->bounds().ToString());

  // All other windows should not have been touched.
  EXPECT_FALSE(WindowState::Get(w4.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w5.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w6.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());

  // After the tablet mode was disabled all windows fall back into the mode
  // they were created for.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
}

// Test that a window which got created while the tablet mode window manager
// is active gets restored to a usable (non tiny) size upon switching back.
TEST_F(TabletModeWindowManagerTest,
       CreateWindowInTabletModeRestoresToUsefulSize) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());

  // We pass in an empty rectangle to simulate a window creation with no
  // particular size.
  gfx::Rect empty_rect(0, 0, 0, 0);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, empty_rect));
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
  EXPECT_NE(empty_rect.ToString(), window->bounds().ToString());
  gfx::Rect maximized_size = window->bounds();
  const gfx::Insets tablet_insets =
      WorkAreaInsets::ForWindow(window.get())->user_work_area_insets();

  // Destroy the tablet mode and check that the resulting size of the window
  // is remaining as it is (but not maximized).
  DestroyTabletModeWindowManager();

  // Account for work-area updates when leaving tablet mode.
  const gfx::Insets clamshell_insets =
      WorkAreaInsets::ForWindow(window.get())->user_work_area_insets();
  const gfx::Insets offset_difference = clamshell_insets - tablet_insets;
  maximized_size.Inset(offset_difference);

  EXPECT_FALSE(WindowState::Get(window.get())->IsMaximized());
  EXPECT_EQ(maximized_size.ToString(), window->bounds().ToString());
}

// Test that non-maximizable windows get properly handled when created in
// tablet mode.
TEST_F(TabletModeWindowManagerTest, CreateNonMaximizableButResizableWindows) {
  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);

  gfx::Rect rect(10, 10, 200, 50);
  gfx::Size max_size(300, 200);
  gfx::Size empty_size;
  std::unique_ptr<aura::Window> unlimited_window(CreateNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect, empty_size));
  std::unique_ptr<aura::Window> limited_window(CreateNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect, max_size));
  std::unique_ptr<aura::Window> fixed_window(
      CreateFixedSizeNonMaximizableWindow(aura::client::WINDOW_TYPE_NORMAL,
                                          rect));

  gfx::Size workspace_size =
      screen_util::GetMaximizedWindowBoundsInParent(unlimited_window.get())
          .size();

  // All windows should be sized now as big as possible and be centered.
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  // The unlimited window should have the size of the workspace / parent window.
  EXPECT_FALSE(WindowState::Get(unlimited_window.get())->IsMaximized());
  EXPECT_EQ("0,0", unlimited_window->bounds().origin().ToString());
  EXPECT_EQ(workspace_size.ToString(),
            unlimited_window->bounds().size().ToString());
  // The limited window should have the size of the upper possible bounds.
  EXPECT_FALSE(WindowState::Get(limited_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            limited_window->bounds().origin().ToString());
  EXPECT_EQ(max_size.ToString(), limited_window->bounds().size().ToString());
  // The fixed size window should have the size of the original window.
  EXPECT_FALSE(WindowState::Get(fixed_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            fixed_window->bounds().origin().ToString());
  EXPECT_EQ(rect.size().ToString(), fixed_window->bounds().size().ToString());

  // Destroy the manager again and check that the windows return to their
  // creation state.
  DestroyTabletModeWindowManager();

  EXPECT_FALSE(WindowState::Get(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(WindowState::Get(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());
}

// Create a string which consists of the bounds and the state for comparison.
std::string GetPlacementString(const gfx::Rect& bounds,
                               ui::mojom::WindowShowState state) {
  return bounds.ToString() + ' ' +
         base::NumberToString(static_cast<int>(state));
}

// Retrieves the window's restore state override - if any - and returns it as a
// string.
std::string GetPlacementOverride(aura::Window* window) {
  gfx::Rect* bounds = window->GetProperty(kRestoreBoundsOverrideKey);
  if (!bounds)
    return std::string();
  const auto type = window->GetProperty(kRestoreWindowStateTypeOverrideKey);
  return GetPlacementString(*bounds, ToWindowShowState(type));
}

// Test that the restore state will be kept at its original value for
// session restoration purposes.
TEST_F(TabletModeWindowManagerTest, TestRestoreIntegrety) {
  gfx::Rect bounds(10, 10, 200, 50);
  std::unique_ptr<aura::Window> normal_window(CreateWindowWithWidget(bounds));

  std::unique_ptr<aura::Window> maximized_window(
      CreateWindowWithWidget(bounds));
  WindowState::Get(maximized_window.get())->Maximize();

  EXPECT_EQ(std::string(), GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));

  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);

  // With the maximization the override states should be returned in its
  // pre-maximized state.
  EXPECT_EQ(GetPlacementString(bounds, ui::mojom::WindowShowState::kDefault),
            GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(GetPlacementString(bounds, ui::mojom::WindowShowState::kMaximized),
            GetPlacementOverride(maximized_window.get()));

  // Changing a window's state now does not change the returned result.
  WindowState::Get(maximized_window.get())->Minimize();
  EXPECT_EQ(GetPlacementString(bounds, ui::mojom::WindowShowState::kMaximized),
            GetPlacementOverride(maximized_window.get()));

  // Destroy the manager again and check that the overrides get reset.
  DestroyTabletModeWindowManager();
  EXPECT_EQ(std::string(), GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));

  // Changing a window's state now does not bring the overrides back.
  WindowState::Get(maximized_window.get())->Restore();
  gfx::Rect new_bounds(10, 10, 200, 50);
  maximized_window->SetBounds(new_bounds);

  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));
}

// Test that windows which got created before the maximizer was created can be
// destroyed while the maximizer is still running.
TEST_F(TabletModeWindowManagerTest, PreCreateWindowsDeleteWhileActive) {
  TabletModeWindowManager* manager = NULL;
  {
    // Bounds for windows we know can be controlled.
    gfx::Rect rect1(10, 10, 200, 50);
    gfx::Rect rect2(10, 60, 200, 50);
    gfx::Rect rect3(20, 140, 100, 100);
    // Bounds for anything else.
    std::unique_ptr<aura::Window> w1(
        CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
    std::unique_ptr<aura::Window> w2(
        CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect2));
    std::unique_ptr<aura::Window> w3(CreateFixedSizeNonMaximizableWindow(
        aura::client::WINDOW_TYPE_NORMAL, rect3));

    // Create the manager and make sure that all qualifying windows were
    // detected and changed.
    manager = CreateTabletModeWindowManager();
    ASSERT_TRUE(manager);
    EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  }
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyTabletModeWindowManager();
}

// Test that windows which got created while the maximizer was running can get
// destroyed before the maximizer gets destroyed.
TEST_F(TabletModeWindowManagerTest, CreateWindowsAndDeleteWhileActive) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  {
    // Bounds for windows we know can be controlled.
    gfx::Rect rect1(10, 10, 200, 50);
    gfx::Rect rect2(10, 60, 200, 50);
    gfx::Rect rect3(20, 140, 100, 100);
    std::unique_ptr<aura::Window> w1(
        CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
    std::unique_ptr<aura::Window> w2(
        CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect2));
    std::unique_ptr<aura::Window> w3(CreateFixedSizeNonMaximizableWindow(
        aura::client::WINDOW_TYPE_NORMAL, rect3));
    // Check that the windows got automatically maximized as well.
    EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
    EXPECT_TRUE(WindowState::Get(w1.get())->IsMaximized());
    EXPECT_TRUE(WindowState::Get(w2.get())->IsMaximized());
    EXPECT_FALSE(WindowState::Get(w3.get())->IsMaximized());
  }
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyTabletModeWindowManager();
}

// Test that windows which were maximized stay maximized.
TEST_F(TabletModeWindowManagerTest, MaximizedShouldRemainMaximized) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState::Get(window.get())->Maximize();

  // Create the manager and make sure that the window gets detected.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(1, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Destroy the manager again and check that the window will remain maximized.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
  WindowState::Get(window.get())->Restore();
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
}

// Test that minimized windows do neither get maximized nor restored upon
// entering tablet mode and get restored to their previous state after
// leaving.
TEST_F(TabletModeWindowManagerTest, MinimizedWindowBehavior) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> initially_minimized_window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> initially_normal_window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> initially_maximized_window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState::Get(initially_minimized_window.get())->Minimize();
  WindowState::Get(initially_maximized_window.get())->Maximize();

  // Create the manager and make sure that the window gets detected.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(
      WindowState::Get(initially_minimized_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(initially_normal_window.get())->IsMaximized());
  EXPECT_TRUE(
      WindowState::Get(initially_maximized_window.get())->IsMaximized());
  // Now minimize the second window to check that upon leaving the window
  // will get restored to its minimized state.
  WindowState::Get(initially_normal_window.get())->Minimize();
  WindowState::Get(initially_maximized_window.get())->Minimize();
  EXPECT_TRUE(
      WindowState::Get(initially_minimized_window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(initially_normal_window.get())->IsMinimized());
  EXPECT_TRUE(
      WindowState::Get(initially_maximized_window.get())->IsMinimized());

  // Destroy the manager again and check that the window will get minimized.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(
      WindowState::Get(initially_minimized_window.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(initially_normal_window.get())->IsMinimized());
  EXPECT_TRUE(
      WindowState::Get(initially_maximized_window.get())->IsMaximized());
}

// Check that resizing the desktop does reposition unmaximizable, unresizable &
// managed windows.
TEST_F(TabletModeWindowManagerTest, DesktopSizeChangeMovesUnmaximizable) {
  UpdateDisplay("500x400");
  // This window will move because it does not fit the new bounds.
  gfx::Rect rect(20, 300, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));
  EXPECT_EQ(rect.ToString(), window1->bounds().ToString());

  // This window will not move because it does fit the new bounds.
  gfx::Rect rect2(20, 140, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect2));

  // Turning on the manager will reposition (but not resize) the window.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(2, manager->GetNumberOfManagedWindows());
  gfx::Rect moved_bounds(window1->bounds());
  EXPECT_NE(rect.origin().ToString(), moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), moved_bounds.size().ToString());

  // Simulating a desktop resize should move the window again.
  UpdateDisplay("400x300");
  gfx::Rect new_moved_bounds(window1->bounds());
  EXPECT_NE(rect.origin().ToString(), new_moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), new_moved_bounds.size().ToString());
  EXPECT_NE(moved_bounds.origin().ToString(), new_moved_bounds.ToString());

  // Turning off the mode should not restore to the initial coordinates since
  // the new resolution is smaller and the window was on the edge.
  DestroyTabletModeWindowManager();
  EXPECT_NE(rect.ToString(), window1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), window2->bounds().ToString());
}

// Check that windows return to original location if desktop size changes to
// something else and back while in tablet mode.
TEST_F(TabletModeWindowManagerTest, SizeChangeReturnWindowToOriginalPos) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> window(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));

  // Turning on the manager will reposition (but not resize) the window.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(1, manager->GetNumberOfManagedWindows());
  gfx::Rect moved_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), moved_bounds.size().ToString());

  // Simulating a desktop resize should move the window again.
  ResizeDesktop(-10);
  gfx::Rect new_moved_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), new_moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), new_moved_bounds.size().ToString());
  EXPECT_NE(moved_bounds.origin().ToString(), new_moved_bounds.ToString());

  // Then resize back to the original desktop size which should move windows
  // to their original location after leaving the tablet mode.
  ResizeDesktop(10);
  DestroyTabletModeWindowManager();
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
}

// Check that enabling of the tablet mode does not have an impact on the MRU
// order of windows.
TEST_F(TabletModeWindowManagerTest, ModeChangeKeepsMRUOrder) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> w2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> w3(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> w4(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> w5(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  // The windows should be in the reverse order of creation in the MRU list.
  {
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);

    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }

  // Activating the window manager should keep the order.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(5, manager->GetNumberOfManagedWindows());
  {
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
    // We do not test maximization here again since that was done already.
    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }

  // Destroying should still keep the order.
  DestroyTabletModeWindowManager();
  {
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
    // We do not test maximization here again since that was done already.
    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }
}

// Check that a restore state change does always restore to maximized.
TEST_F(TabletModeWindowManagerTest, IgnoreRestoreStateChages) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  CreateTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());
  DestroyTabletModeWindowManager();
}

// Check that minimize and restore do the right thing.
TEST_F(TabletModeWindowManagerTest, TestMinimize) {
  gfx::Rect rect(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());

  window_state->Minimize();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_FALSE(window->IsVisible());

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
}

// Tests that minimized window can restore to pre-minimized show state after
// entering and leaving tablet mode (https://crbug.com/783310).
TEST_F(TabletModeWindowManagerTest, MinimizedEnterAndLeaveTabletMode) {
  gfx::Rect rect(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(window_state->IsMinimized());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_FALSE(window_state->IsMinimized());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
}

// Tests that pre-minimized window show state is persistent after entering and
// leaving tablet mode, that is not cleared in tablet mode.
TEST_F(TabletModeWindowManagerTest, PersistPreMinimizedShowState) {
  gfx::Rect rect(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  window_state->Minimize();
  EXPECT_EQ(ui::mojom::WindowShowState::kMaximized,
            window->GetProperty(aura::client::kRestoreShowStateKey));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  window_state->Unminimize();
  // Check that pre-minimized window show state is not cleared due to
  // unminimizing in tablet mode.
  EXPECT_EQ(ui::mojom::WindowShowState::kMaximized,
            window->GetProperty(aura::client::kRestoreShowStateKey));
  window_state->Minimize();
  EXPECT_EQ(ui::mojom::WindowShowState::kMaximized,
            window->GetProperty(aura::client::kRestoreShowStateKey));

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());
}

// Tests unminimizing in tablet mode and then exiting tablet mode should have
// pre-minimized window show state.
TEST_F(TabletModeWindowManagerTest, UnminimizeInTabletMode) {
  // Tests restoring to maximized show state.
  gfx::Rect rect(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  window_state->Minimize();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  window_state->Unminimize();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_TRUE(window_state->IsMaximized());

  // Tests restoring to normal show state.
  window_state->Restore();
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->GetBoundsInScreen());
  window_state->Minimize();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  window_state->Unminimize();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->GetBoundsInScreen());
}

// Tests that if we minimize a snapped window, it is snapped upon unminimizing.
TEST_F(TabletModeWindowManagerTest, UnminimizeSnapInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  auto* window_state = WindowState::Get(window.get());
  WindowSnapWMEvent event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&event);
  ASSERT_TRUE(window_state->IsSnapped());

  window_state->Minimize();
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsSnapped());
}

// Check that a full screen window remains full screen upon entering maximize
// mode. Furthermore, checks that this window is not full screen upon exiting
// tablet mode if it was un-full-screened while in tablet mode.
TEST_F(TabletModeWindowManagerTest, KeepFullScreenModeOn) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());

  Shelf* shelf = GetPrimaryShelf();

  // Allow the shelf to hide and set the pref.
  SetShelfAutoHideBehaviorPref(GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);

  // With full screen, the shelf should get hidden.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  CreateTabletModeWindowManager();

  // The Full screen mode should continue to be on.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // When exiting fullscreen, tablet mode should still be enabled, and the shelf
  // state should return to SHELF_AUTO_HIDE.
  window_state->OnWMEvent(&event);
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // The shelf auto-hide preference should be restored when exiting tablet mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
}

// Similar to the fullscreen mode, the pinned mode should be kept as well.
TEST_F(TabletModeWindowManagerTest, KeepPinnedModeOn_Case1) {
  // Scenario: in the default state, pin a window, enter to the tablet mode,
  // then unpin.
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_FALSE(window_state->IsPinned());

  // Pin the window.
  {
    WMEvent event(WM_EVENT_PIN);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Enter tablet mode. The pinned mode should continue to be on.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Then unpin.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsPinned());

  // Exit tablet mode. The window should not be back to the pinned mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());
}

TEST_F(TabletModeWindowManagerTest, KeepPinnedModeOn_Case2) {
  // Scenario: in the tablet mode, pin a window, exit tablet mode, then unpin.
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_FALSE(window_state->IsPinned());

  // Enter tablet mode.
  CreateTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());

  // Pin the window.
  {
    WMEvent event(WM_EVENT_PIN);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Exit tablet mode. The pinned mode should continue to be on.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Then unpin.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsPinned());

  // Enter tablet mode again for verification. The window should not be back to
  // the pinned mode.
  CreateTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());

  // Exit tablet mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());
}

TEST_F(TabletModeWindowManagerTest, KeepPinnedModeOn_Case3) {
  // Scenario: in the default state, pin a window, enter to the tablet mode,
  // exit from the tablet mode, then unpin.
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_FALSE(window_state->IsPinned());

  // Pin the window.
  {
    WMEvent event(WM_EVENT_PIN);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Enter tablet mode. The pinned mode should continue to be on.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Exit tablet mode. The pinned mode should continue to be on, too.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Then unpin.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsPinned());

  // Enter tablet mode again for verification. The window should not be back to
  // the pinned mode.
  CreateTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());

  // Exit tablet mode.
  DestroyTabletModeWindowManager();
}

TEST_F(TabletModeWindowManagerTest, KeepPinnedModeOn_Case4) {
  // Scenario: in tablet mode, pin a window, exit tablet mode, enter tablet mode
  // again, then unpin.
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_FALSE(window_state->IsPinned());

  // Enter tablet mode.
  CreateTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());

  // Pin the window.
  {
    WMEvent event(WM_EVENT_PIN);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Exit tablet mode. The pinned mode should continue to be on.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Enter tablet mode again. The pinned mode should continue to be on, too.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsPinned());

  // Then unpin.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsPinned());

  // Exit tablet mode. The window should not be back to the pinned mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());
}

TEST_F(TabletModeWindowManagerTest, KeepPinnedModeOn_Case5) {
  std::unique_ptr<aura::Window> w1(CreateWindow(
      aura::client::WINDOW_TYPE_NORMAL, gfx::Rect(20, 140, 100, 100)));
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_FALSE(window_state->IsPinned());

  CreateTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());

  // Pin the window.
  {
    WMEvent event(WM_EVENT_PIN);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Trigger ADDED_TO_WORKSPACE event.
  {
    WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
    window_state->OnWMEvent(&event);
  }
  EXPECT_TRUE(window_state->IsPinned());

  // Then unpin.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsPinned());

  // Exit tablet mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(window_state->IsPinned());
}

// Verifies that if a window is un-full-screened while in tablet mode,
// other changes to that window's state (such as minimizing it) are
// preserved upon exiting tablet mode.
TEST_F(TabletModeWindowManagerTest, MinimizePreservedAfterLeavingFullscreen) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());

  Shelf* shelf = GetPrimaryShelf();

  // Allow the shelf to hide and enter full screen.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  ASSERT_FALSE(window_state->IsMinimized());

  // Enter tablet mode, exit full screen, and then minimize the window.
  CreateTabletModeWindowManager();
  window_state->OnWMEvent(&event);
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  // The window should remain minimized when exiting tablet mode.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsMinimized());
}

// Tests that the auto-hide behavior is not affected when entering/exiting
// tablet mode.
TEST_F(TabletModeWindowManagerTest, DoNotDisableAutoHideBehaviorOnTabletMode) {
  Shelf* shelf = GetPrimaryShelf();
  SetShelfAutoHideBehaviorPref(GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  CreateTabletModeWindowManager();
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  DestroyTabletModeWindowManager();
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
}

// Check that full screen mode can be turned on in tablet mode and remains
// upon coming back.
TEST_F(TabletModeWindowManagerTest, AllowFullScreenMode) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());

  Shelf* shelf = GetPrimaryShelf();

  // Allow the shelf to hide and set the pref.
  SetShelfAutoHideBehaviorPref(GetPrimaryDisplay().id(),
                               ShelfAutoHideBehavior::kAlways);

  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  CreateTabletModeWindowManager();

  // Fullscreen should stay off, and the shelf behavior is unmodified.
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // After going into fullscreen mode, the shelf should be hidden.
  WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // With the destruction of the manager we should remain in full screen.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Check that the full screen mode will stay active when the tablet mode is
// ended.
TEST_F(TabletModeWindowManagerTest,
       FullScreenModeRemainsWhenCreatedInTabletMode) {
  CreateTabletModeWindowManager();

  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  WMEvent event_full_screen(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event_full_screen);
  EXPECT_TRUE(window_state->IsFullscreen());

  // After the tablet mode manager is ended, full screen will remain.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
}

// Check that the full screen mode will stay active throughout a maximzied mode
// session.
TEST_F(TabletModeWindowManagerTest,
       FullScreenModeRemainsThroughTabletModeSwitch) {
  gfx::Rect rect(20, 140, 100, 100);
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(w1.get());
  WMEvent event_full_screen(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event_full_screen);
  EXPECT_TRUE(window_state->IsFullscreen());

  CreateTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
}

// Check that an empty window does not get restored to a tiny size.
TEST_F(TabletModeWindowManagerTest,
       CreateAndMaximizeInTabletModeShouldRetoreToGoodSizeGoingToDefault) {
  CreateTabletModeWindowManager();
  gfx::Rect rect;
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  w1->Show();
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_TRUE(window_state->IsMaximized());

  // There is a calling order in which the restore bounds can get set to an
  // empty rectangle. We simulate this here.
  window_state->SetRestoreBoundsInScreen(rect);
  EXPECT_TRUE(window_state->GetRestoreBoundsInScreen().IsEmpty());

  // Setting the window to a new size will physically not change the window,
  // but the restore size should get updated so that a restore later on will
  // return to this size.
  gfx::Rect requested_bounds(10, 20, 50, 70);
  w1->SetBounds(requested_bounds);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(requested_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  DestroyTabletModeWindowManager();

  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(w1->bounds().ToString(), requested_bounds.ToString());
}

// Check that non maximizable windows cannot be dragged by the user.
TEST_F(TabletModeWindowManagerTest, TryToDesktopSizeDragUnmaximizable) {
  gfx::Rect rect(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());

  // 1. Move the mouse over the caption and check that dragging the window does
  // change the location.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(gfx::Point(rect.x() + 2, rect.y() + 2));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  base::RunLoop().RunUntilIdle();
  generator.ReleaseLeftButton();
  gfx::Point first_dragged_origin = window->bounds().origin();
  EXPECT_EQ(rect.x() + 10, first_dragged_origin.x());
  EXPECT_EQ(rect.y() + 5, first_dragged_origin.y());

  // 2. Check that turning on the manager will stop allowing the window from
  // dragging.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  gfx::Rect center_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), center_bounds.origin().ToString());
  generator.MoveMouseTo(
      gfx::Point(center_bounds.x() + 1, center_bounds.y() + 1));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  base::RunLoop().RunUntilIdle();
  generator.ReleaseLeftButton();
  EXPECT_EQ(center_bounds.x(), window->bounds().x());
  EXPECT_EQ(center_bounds.y(), window->bounds().y());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // 3. Releasing the mazimize manager again will restore the window to its
  // previous bounds and
  generator.MoveMouseTo(
      gfx::Point(first_dragged_origin.x() + 1, first_dragged_origin.y() + 1));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  base::RunLoop().RunUntilIdle();
  generator.ReleaseLeftButton();
  EXPECT_EQ(first_dragged_origin.x() + 10, window->bounds().x());
  EXPECT_EQ(first_dragged_origin.y() + 5, window->bounds().y());
}

// Tests that windows with the always-on-top property are not managed by
// the TabletModeWindowManager while tablet mode is engaged (i.e.,
// they remain free-floating).
TEST_F(TabletModeWindowManagerTest, AlwaysOnTopWindows) {
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(20, 140, 100, 100);

  // Create two windows with the always-on-top property.
  std::unique_ptr<aura::Window> w1(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect1));
  std::unique_ptr<aura::Window> w2(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect2));
  w1->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  w2->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());

  // Enter tablet mode. Neither window should be managed because they have
  // the always-on-top property set, which means that none of their properties
  // should change.
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());

  // Remove the always-on-top property from both windows while in maximize
  // mode. The windows should become managed, which means they should be
  // maximized/centered and no longer be draggable.
  w1->SetProperty(aura::client::kZOrderingKey, ui::ZOrderLevel::kNormal);
  w2->SetProperty(aura::client::kZOrderingKey, ui::ZOrderLevel::kNormal);
  EXPECT_EQ(2, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_NE(rect1.origin().ToString(), w1->bounds().origin().ToString());
  EXPECT_NE(rect1.size().ToString(), w1->bounds().size().ToString());
  EXPECT_NE(rect2.origin().ToString(), w2->bounds().origin().ToString());
  EXPECT_EQ(rect2.size().ToString(), w2->bounds().size().ToString());

  // Applying the always-on-top property to both windows while in maximize
  // mode should cause both windows to return to their original size,
  // position, and state.
  w1->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  w2->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());

  // The always-on-top windows should not change when leaving tablet mode.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(w1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(w2.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
}

// Tests that windows that can control maximized bounds are not maximized
// and not tracked.
TEST_F(TabletModeWindowManagerTest, DontMaximizeClientManagedWindows) {
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  WindowState::Get(window.get())->set_allow_set_bounds_direct(true);

  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(window.get())->IsMaximized());
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
}

// Verify that if tablet mode is started in the lock screen, windows will still
// be maximized after leaving the lock screen.
TEST_F(TabletModeWindowManagerTest, CreateManagerInLockScreen) {
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  ASSERT_FALSE(WindowState::Get(window.get())->IsMaximized());

  // Create the tablet mode window manager while inside the lock screen.
  GetSessionControllerClient()->RequestLockScreen();
  CreateTabletModeWindowManager();
  GetSessionControllerClient()->UnlockScreen();

  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  DestroyTabletModeWindowManager();
  EXPECT_FALSE(WindowState::Get(window.get())->IsMaximized());
}

namespace {

class TestObserver : public WindowStateObserver {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  WindowStateType old_type) override {
    pre_count_++;
    last_old_state_ = old_type;
  }

  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   WindowStateType old_type) override {
    post_count_++;
    post_layer_visibility_ = window_state->window()->layer()->visible();
    EXPECT_EQ(last_old_state_, old_type);
  }

  int GetPreCountAndReset() {
    int r = pre_count_;
    pre_count_ = 0;
    return r;
  }

  int GetPostCountAndReset() {
    int r = post_count_;
    post_count_ = 0;
    return r;
  }

  bool GetPostLayerVisibilityAndReset() {
    bool r = post_layer_visibility_;
    post_layer_visibility_ = false;
    return r;
  }

  WindowStateType GetLastOldStateAndReset() {
    WindowStateType r = last_old_state_;
    last_old_state_ = WindowStateType::kDefault;
    return r;
  }

 private:
  int pre_count_ = 0;
  int post_count_ = 0;
  bool post_layer_visibility_ = false;
  WindowStateType last_old_state_ = WindowStateType::kDefault;
};

}  // namespace

TEST_F(TabletModeWindowManagerTest, StateTypeChange) {
  TestObserver observer;
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  CreateTabletModeWindowManager();

  WindowState* window_state = WindowState::Get(window.get());
  window_state->AddObserver(&observer);

  window->Show();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(0, observer.GetPreCountAndReset());
  EXPECT_EQ(0, observer.GetPostCountAndReset());

  // Window is already in tablet mode.
  WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(0, observer.GetPreCountAndReset());
  EXPECT_EQ(0, observer.GetPostCountAndReset());

  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(1, observer.GetPreCountAndReset());
  EXPECT_EQ(1, observer.GetPostCountAndReset());
  EXPECT_EQ(WindowStateType::kMaximized, observer.GetLastOldStateAndReset());

  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(1, observer.GetPreCountAndReset());
  EXPECT_EQ(1, observer.GetPostCountAndReset());
  EXPECT_EQ(WindowStateType::kFullscreen, observer.GetLastOldStateAndReset());

  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  EXPECT_EQ(1, observer.GetPreCountAndReset());
  EXPECT_EQ(1, observer.GetPostCountAndReset());
  EXPECT_EQ(WindowStateType::kMaximized, observer.GetLastOldStateAndReset());

  WMEvent restore_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&restore_event);
  EXPECT_EQ(1, observer.GetPreCountAndReset());
  EXPECT_EQ(1, observer.GetPostCountAndReset());
  EXPECT_EQ(WindowStateType::kMinimized, observer.GetLastOldStateAndReset());
  EXPECT_EQ(true, observer.GetPostLayerVisibilityAndReset());

  window_state->RemoveObserver(&observer);

  DestroyTabletModeWindowManager();
}

// Test that the restore state will be kept at its original value for
// session restoration purposes.
TEST_F(TabletModeWindowManagerTest, SetPropertyOnUnmanagedWindow) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  InitParams params(aura::client::WINDOW_TYPE_NORMAL);
  params.bounds = gfx::Rect(10, 10, 100, 100);
  params.show_on_creation = false;
  std::unique_ptr<aura::Window> window(CreateWindowInWatchedContainer(params));
  WindowState::Get(window.get())->set_allow_set_bounds_direct(true);
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  window->Show();
}

// Test that the minimized window bounds doesn't change until it's unminimized.
TEST_F(TabletModeWindowManagerTest, DontChangeBoundsForMinimizedWindow) {
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(1, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_EQ(window->bounds(), rect);

  EnterOverview();
  EXPECT_EQ(window->bounds(), rect);

  // Exit overview mode will update all windows' bounds. However, if the window
  // is minimized, the bounds will not be updated.
  ExitOverview();
  EXPECT_EQ(window->bounds(), rect);
}

// Make sure that transient children should not be maximized.
TEST_F(TabletModeWindowManagerTest, DontMaximizeTransientChild) {
  gfx::Rect rect(0, 0, 200, 200);
  std::unique_ptr<aura::Window> parent(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  std::unique_ptr<aura::Window> child(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  ::wm::TransientWindowManager::GetOrCreate(parent.get())
      ->AddTransientChild(child.get());

  ASSERT_TRUE(CreateTabletModeWindowManager());
  EXPECT_TRUE(WindowState::Get(parent.get())->IsMaximized());
  EXPECT_NE(rect.size(), parent->bounds().size());
  EXPECT_FALSE(WindowState::Get(child.get())->IsMaximized());
  EXPECT_EQ(rect.size(), child->bounds().size());
}

TEST_F(TabletModeWindowManagerTest, AllowNormalWindowBoundsChangeByVK) {
  UpdateDisplay("1200x800");
  gfx::Rect rect(0, 0, 1200, 600);
  std::unique_ptr<aura::Window> window(CreateFixedSizeNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect));
  ASSERT_TRUE(CreateTabletModeWindowManager());

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  gfx::Rect window_bounds = window->bounds();

  // Simulate VK up.
  wm::EnsureWindowNotInRect(window.get(), gfx::Rect(0, 600, 1200, 200));
  EXPECT_NE(window->bounds(), window_bounds);

  // Simulate VK dismissal.
  wm::RestoreWindowBoundsOnClientFocusLost(window.get());
  EXPECT_EQ(window->bounds(), window_bounds);
}

// Test clamshell mode <-> tablet mode transition.
// TODO(b/327269057): Refactor this to `SplitViewController|SnapGroup`.
TEST_F(TabletModeWindowManagerTest, ClamshellTabletTransitionTest) {
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  // 1. Clamshell -> tablet. If overview is active, it should still be kept
  // active after transition.
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(EnterOverview());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  EXPECT_TRUE(manager);
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // 2. Tablet -> Clamshell. If overview is active, it should still be kept
  // active after transition.
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // 3. Clamshell -> tablet. If overview is inactive, it should still be kept
  // inactive after transition. All windows will be maximized.
  EXPECT_TRUE(ExitOverview());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  CreateTabletModeWindowManager();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // 4. Tablet -> Clamshell. The window should be restored to its old state.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(WindowState::Get(window.get())->IsMaximized());

  // 5. Clamshell -> Tablet. If the window is snapped, it will be carried over
  // to splitview in tablet mode.
  const WindowSnapWMEvent event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window.get())->OnWMEvent(&event);
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
  // After transition, we should be in single split screen.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());

  // 6. Tablet -> Clamshell. Since there is only 1 window, splitview and
  // overview will be both ended. The window will be kept snapped.
  DestroyTabletModeWindowManager();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());

  // Create another normal state window to test additional scenarios.
  std::unique_ptr<aura::Window> window2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  wm::ActivateWindow(window2.get());
  // 7. Clamshell -> Tablet. Since top window is not a snapped window, all
  // windows will be maximized.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // 8. Tablet -> Clamshell. If tablet splitscreen is active with two snapped
  // windows, the two windows will remain snapped in clamshell mode.
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsSnapped());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // 9. Clamshell -> Tablet. If two window are snapped to two sides of the
  // screen, they will carry over to splitscreen in tablet mode.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsSnapped());

  // 10. Tablet -> Clamshell. If overview and splitview are both active, after
  // transition, they will remain both active.
  EnterOverview();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DestroyTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // 11. Clamshell -> Tablet. The same as 10.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

// Test the divider position value during tablet <-> clamshell transition.
TEST_F(TabletModeWindowManagerTest,
       ClamshellTabletTransitionDividerPositionTest) {
  UpdateDisplay("1200x800");
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  OverviewController* overview_controller = OverviewController::Get();

  // First test 1 window case.
  const WindowSnapWMEvent left_snap_event(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(window.get())->OnWMEvent(&left_snap_event);
  const gfx::Rect left_snapped_bounds =
      gfx::Rect(1200 / 2, 800 - ShelfConfig::Get()->shelf_size());
  EXPECT_EQ(window->bounds().width(), left_snapped_bounds.width());
  // Change its bounds horizontally a bit and then enter tablet mode.
  window->SetBounds(gfx::Rect(400, left_snapped_bounds.height()));
  CreateTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window.get()));
  // Check the window is moved to 1/3 snapped position.
  EXPECT_EQ(window->bounds().width(),
            std::round(1200 * chromeos::kOneThirdSnapRatio) -
                kSplitviewDividerShortSideLength / 2);
  // Exit tablet mode and verify the window stays near the same position.
  DestroyTabletModeWindowManager();
  EXPECT_NEAR(window->bounds().width(),
              std::round(1200 * chromeos::kOneThirdSnapRatio),
              kSplitviewDividerShortSideLength / 2);

  // Now test the 2 windows case.
  std::unique_ptr<aura::Window> window2(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));
  WindowState::Get(window.get())->OnWMEvent(&left_snap_event);
  const WindowSnapWMEvent right_snap_event(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(window2.get())->OnWMEvent(&right_snap_event);
  // Change their bounds horizontally and then enter tablet mode.
  window->SetBounds(gfx::Rect(400, left_snapped_bounds.height()));
  window2->SetBounds(gfx::Rect(400, 0, 800, left_snapped_bounds.height()));
  CreateTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  // Check |window| and |window2| is moved to 1/3 snapped position.
  EXPECT_EQ(window->bounds().width(),
            std::round(1200 * chromeos::kOneThirdSnapRatio) -
                kSplitviewDividerShortSideLength / 2);
  EXPECT_EQ(window2->bounds().width(),
            1200 - window->bounds().width() - kSplitviewDividerShortSideLength);
  // Exit tablet mode and verify the windows stay near the same position.
  DestroyTabletModeWindowManager();
  EXPECT_NEAR(window->bounds().width(),
              std::round(1200 * chromeos::kOneThirdSnapRatio),
              kSplitviewDividerShortSideLength / 2);
  EXPECT_NEAR(window2->bounds().width(), 1200 - window->bounds().width(),
              kSplitviewDividerShortSideLength / 2);
}

// Tests partial split clamshell <-> tablet transition.
TEST_F(TabletModeWindowManagerTest, PartialClamshellTabletTransitionTest) {
  // 1. Create a window and snap to primary 2/3.
  auto window1 = CreateTestWindow();
  OverviewController* overview_controller = OverviewController::Get();
  const WindowSnapWMEvent snap_primary_two_third(WM_EVENT_SNAP_PRIMARY,
                                                 chromeos::kTwoThirdSnapRatio);
  WindowState::Get(window1.get())->OnWMEvent(&snap_primary_two_third);
  // Enter tablet mode and verify that overview opens and the window and
  // divider are at 2/3.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  int divider_origin_x = split_view_controller()
                             ->split_view_divider()
                             ->GetDividerBoundsInScreen(
                                 /*is_dragging=*/false)
                             .x();
  int divider_delta = kSplitviewDividerShortSideLength / 2;
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            divider_origin_x + divider_delta);
  // Exit tablet mode and verify the window stays in the same position.
  DestroyTabletModeWindowManager();
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width());

  // 2. Create another window and snap to secondary at 1/3.
  auto window2 = CreateTestWindow();
  const WindowSnapWMEvent snap_secondary_one_third(
      WM_EVENT_SNAP_SECONDARY, chromeos::kOneThirdSnapRatio);
  WindowState::Get(window2.get())->OnWMEvent(&snap_secondary_one_third);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width());
  // Enter tablet mode and verify the windows are in splitview and the window
  // bounds and divider are at 2/3.
  CreateTabletModeWindowManager();
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  divider_origin_x = split_view_controller()
                         ->split_view_divider()
                         ->GetDividerBoundsInScreen(
                             /*is_dragging=*/false)
                         .x();

  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
            window1->bounds().width() + divider_delta);
  EXPECT_EQ(std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
            window2->bounds().width() + divider_delta);
  EXPECT_EQ(
      std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio) -
          divider_delta,
      divider_origin_x);

  // Exit tablet mode and verify the windows are still at 2/3, with allowance
  // for the divider width since it is only there in tablet mode.
  DestroyTabletModeWindowManager();
  if (IsSnapGroupEnabledInClamshellMode()) {
    // TODO(b/5626469): Revisit the snapped bounds.
    EXPECT_NEAR(
        std::round(work_area_bounds.width() * chromeos::kTwoThirdSnapRatio),
        window1->bounds().width(), divider_delta);
    EXPECT_NEAR(
        std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
        window2->bounds().width(), divider_delta);
  } else {
    EXPECT_EQ(
        std::round(work_area_bounds.width() * chromeos::kOneThirdSnapRatio),
        window2->bounds().width() + divider_delta);
  }
}

// Test that when switching from clamshell mode to tablet mode, if overview mode
// is active, home launcher is hidden. And after overview mode is dismissed,
// home launcher will be shown again.
TEST_F(TabletModeWindowManagerTest, HomeLauncherVisibilityTest) {
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  // Clamshell -> Tablet mode transition. If overview is active, it will remain
  // in overview.
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(EnterOverview());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  EXPECT_TRUE(manager);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);

  aura::Window* home_screen_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  EXPECT_FALSE(home_screen_window->TargetVisibility());

  base::HistogramTester tester;
  tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf, 0);

  // Tap at window to leave the overview mode.
  GetEventGenerator()->GestureTapAt(window->GetBoundsInScreen().CenterPoint());
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);
  tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf, 0);

  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(home_screen_window->TargetVisibility());
}

// Test the basic restore behavior in tablet mode. Different with the restore
// behavior in clamshell mode, a window can not be restored to kNormal window
// state if it's maximizable.
TEST_F(TabletModeWindowManagerTest, BasicRestoreBehaviors) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  EXPECT_TRUE(manager);
  gfx::Rect rect(10, 10, 200, 50);
  std::unique_ptr<aura::Window> window(
      CreateWindow(aura::client::WINDOW_TYPE_NORMAL, rect));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsMaximized());

  // Restoring a maximized window in tablet mode will still keep it in maximized
  // state.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());

  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  // Restoring a snapped window in tablet mode will change the window back to
  // maximized window state.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());

  // Transition to kFullscreen window state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  // Restoring a fullscreen window in tablet mode will change the window back to
  // maximized window state.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());

  // Transition to kMinimized window state.
  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());

  // Transition to kPrimarySnapped first and then to kFullscreen and then try to
  // restore it.
  window_state->OnWMEvent(&snap_left);
  window_state->OnWMEvent(&fullscreen_event);
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());

  // Minimize and then restore it will still restore the window back to snapped
  // window state.
  window_state->OnWMEvent(&minimized_event);
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
}

TEST_F(TabletModeWindowManagerTest, NonMaximizableWindowRestore) {
  TabletModeWindowManager* manager = CreateTabletModeWindowManager();
  EXPECT_TRUE(manager);

  gfx::Rect rect(10, 10, 200, 50);
  gfx::Size max_size(300, 200);
  std::unique_ptr<aura::Window> window(CreateNonMaximizableWindow(
      aura::client::WINDOW_TYPE_NORMAL, rect, max_size));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);

  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kFullscreen);

  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);

  // Restoring a kNormal window will keep it in the same kNormal state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
}

}  // namespace ash
