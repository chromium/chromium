// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/window_restore_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "base/cancelable_callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using testing::ElementsAre;

void PerformAcceleratorAction(AcceleratorAction action,
                              const ui::Accelerator& accelerator) {
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(action,
                                                                 accelerator);
}

void SetResizable(views::Widget* widget) {
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanResize |
          aura::client::kResizeBehaviorCanMaximize);
}

}  // namespace

class WindowRestoreControllerTest : public AshTestBase,
                                    public aura::EnvObserver {
 public:
  // Struct which is the data in our fake window restore file.
  struct WindowInfo {
    int call_count = 0;
    app_restore::WindowInfo info;
  };

  WindowRestoreControllerTest() = default;
  WindowRestoreControllerTest(const WindowRestoreControllerTest&) = delete;
  WindowRestoreControllerTest& operator=(const WindowRestoreControllerTest&) =
      delete;
  ~WindowRestoreControllerTest() override = default;

  // Returns the number of times |window| has been saved to file since the last
  // ResetSaveWindowsCount call.
  int GetSaveWindowsCount(aura::Window* window) const {
    const int32_t restore_window_id =
        window->GetProperty(app_restore::kRestoreWindowIdKey);
    if (!base::Contains(fake_window_restore_file_, restore_window_id))
      return 0;
    return fake_window_restore_file_.at(restore_window_id).call_count;
  }

  // Returns the total number of saves since the last ResetSaveWindowsCount
  // call.
  int GetTotalSaveWindowsCount() const {
    int count = 0;
    for (const std::pair<int32_t, WindowInfo>& member :
         fake_window_restore_file_) {
      count += member.second.call_count;
    }
    return count;
  }

  void ResetSaveWindowsCount() {
    for (std::pair<int32_t, WindowInfo>& member : fake_window_restore_file_)
      member.second.call_count = 0;
  }

  // Returns window info for `window`.
  std::optional<app_restore::WindowInfo> GetWindowInfo(
      aura::Window* window) const {
    const int32_t restore_window_id =
        window->GetProperty(app_restore::kRestoreWindowIdKey);
    if (!base::Contains(fake_window_restore_file_, restore_window_id)) {
      return std::nullopt;
    }
    return fake_window_restore_file_.at(restore_window_id).info;
  }

  // Returns the stored activation index for `window`.
  int GetActivationIndex(aura::Window* window) const {
    std::optional<app_restore::WindowInfo> window_info = GetWindowInfo(window);
    return window_info ? window_info->activation_index.value_or(-1) : -1;
  }

  // Returns the restore property clear callbacks.
  const std::map<aura::Window*, base::CancelableOnceClosure>&
  GetRestorePropertyClearCallbacks() {
    return WindowRestoreController::Get()->restore_property_clear_callbacks_;
  }

  // Mocks creating a widget that is launched from window restore service.
  views::Widget* CreateTestWindowRestoredWidget(
      int32_t activation_index,
      const gfx::Rect& bounds = gfx::Rect(200, 200),
      aura::Window* root_window = Shell::GetPrimaryRootWindow()) {
    // If this is a new window, finds and sets a new restore window id.
    int32_t restore_window_id = 1;
    while (fake_window_restore_file_.contains(restore_window_id))
      ++restore_window_id;

    AddEntryToFakeFile(restore_window_id, bounds,
                       chromeos::WindowStateType::kNormal, activation_index,
                       WindowTreeHostManager::GetPrimaryDisplayId(),
                       /*desk_id=*/1);
    return CreateTestWindowRestoredWidgetFromRestoreId(
        restore_window_id, chromeos::AppType::BROWSER,
        /*is_taskless_arc_app=*/false);
  }

  // Mocks creating a widget based on the window info in
  // `fake_window_restore_file_`. Returns nullptr if there is not an entry that
  // matches `restore_window_id`.
  views::Widget* CreateTestWindowRestoredWidgetFromRestoreId(
      int32_t restore_window_id,
      chromeos::AppType app_type,
      bool is_taskless_arc_app) {
    if (!fake_window_restore_file_.contains(restore_window_id))
      return nullptr;

    app_restore::WindowInfo info =
        fake_window_restore_file_[restore_window_id].info;
    DCHECK(info.current_bounds);
    DCHECK(info.window_state_type);
    DCHECK(info.activation_index);
    DCHECK(info.display_id);

    aura::Window* context = Shell::GetRootWindowForDisplayId(*info.display_id);
    // The display may have been disconnected.
    if (!context)
      context = Shell::GetPrimaryRootWindow();
    DCHECK(context->IsRootWindow());

    // Window restore widgets are inactive when created as we do not want to
    // take activation from a possible activated window, and we want to stack
    // them in a certain order.
    TestWidgetBuilder widget_builder;
    widget_builder.SetWidgetType(views::Widget::InitParams::TYPE_WINDOW)
        .SetBounds(*info.current_bounds)
        .SetShow(false)
        .SetContext(context)
        .SetShowState(chromeos::ToWindowShowState(*info.window_state_type))
        .SetWindowProperty(app_restore::kWindowInfoKey,
                           new app_restore::WindowInfo(info))
        .SetWindowProperty(app_restore::kActivationIndexKey,
                           new int32_t(*info.activation_index))
        .SetWindowProperty(app_restore::kLaunchedFromAppRestoreKey, true)
        .SetWindowProperty(app_restore::kRestoreWindowIdKey, restore_window_id)
        .SetWindowProperty(chromeos::kAppTypeKey, app_type)
        .SetWindowProperty(app_restore::kParentToHiddenContainerKey,
                           is_taskless_arc_app);

    views::Widget* widget = widget_builder.BuildOwnedByNativeWidget();
    SetResizable(widget);
    if (!is_taskless_arc_app)
      WindowRestoreController::Get()->OnWidgetInitialized(widget);
    if (*info.window_state_type != chromeos::WindowStateType::kMinimized) {
      widget->Show();
    }
    return widget;
  }

  views::Widget* CreateTestWindowRestoredWidgetFromRestoreId(
      int32_t restore_window_id) {
    return CreateTestWindowRestoredWidgetFromRestoreId(
        restore_window_id, chromeos::AppType::BROWSER,
        /*is_taskless_arc_app=*/false);
  }

  // Adds an entry to the fake window restore file. If
  // `CreateTestWindowRestoredWidget` is called with a matching
  // `restore_window_id`, it will read and set the values set here.
  void AddEntryToFakeFile(int restore_window_id,
                          const gfx::Rect& bounds,
                          chromeos::WindowStateType window_state_type,
                          int32_t activation_index,
                          int64_t display_id,
                          int32_t desk_id) {
    DCHECK(!fake_window_restore_file_.contains(restore_window_id));
    WindowInfo window_info;
    window_info.info.current_bounds = bounds;
    window_info.info.window_state_type = window_state_type;
    window_info.info.activation_index = activation_index;
    window_info.info.display_id = display_id;
    window_info.info.desk_id = desk_id;
    fake_window_restore_file_[restore_window_id] = std::move(window_info);
  }

  void AddEntryToFakeFile(int restore_window_id,
                          const gfx::Rect& bounds,
                          chromeos::WindowStateType window_state_type,
                          int32_t activation_index,
                          int64_t display_id) {
    AddEntryToFakeFile(restore_window_id, bounds, window_state_type,
                       activation_index, display_id, /*desk_id=*/1);
  }

  void AddEntryToFakeFile(int restore_window_id,
                          const gfx::Rect& bounds,
                          chromeos::WindowStateType window_state_type) {
    AddEntryToFakeFile(
        restore_window_id, bounds, window_state_type, /*activation_index=*/-1,
        WindowTreeHostManager::GetPrimaryDisplayId(), /*desk_id=*/1);
  }

  void AddEntryToFakeFile(int restore_window_id,
                          const gfx::Rect& bounds,
                          chromeos::WindowStateType window_state_type,
                          int32_t desk_id) {
    AddEntryToFakeFile(restore_window_id, bounds, window_state_type,
                       /*activation_index=*/-1,
                       WindowTreeHostManager::GetPrimaryDisplayId(), desk_id);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    WindowRestoreController::Get()->SetSaveWindowCallbackForTesting(
        base::BindRepeating(&WindowRestoreControllerTest::OnSaveWindow,
                            base::Unretained(this)));
    env_observation_.Observe(aura::Env::GetInstance());

    // Turn on the user preference by default, so do not need to set
    // for all test cases all the time.
    app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(
        Shell::Get()->session_controller()->GetActiveAccountId(), true);
  }

  void TearDown() override {
    env_observation_.Reset();
    WindowRestoreController::Get()->SetSaveWindowCallbackForTesting({});
    AshTestBase::TearDown();
  }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    std::vector<chromeos::AppType> kSupportedAppTypes = {
        chromeos::AppType::BROWSER, chromeos::AppType::CHROME_APP,
        chromeos::AppType::ARC_APP};
    if (!base::Contains(kSupportedAppTypes,
                        window->GetProperty(chromeos::kAppTypeKey))) {
      return;
    }

    // If this is a new window, finds and sets a new restore window id.
    if (window->GetProperty(app_restore::kRestoreWindowIdKey) == 0) {
      int32_t restore_window_id = 1;
      while (fake_window_restore_file_.contains(restore_window_id))
        ++restore_window_id;
      window->SetProperty(app_restore::kRestoreWindowIdKey, restore_window_id);
    }
  }

 private:
  // Called when WindowRestoreController saves a window to the file. Immediately
  // writes to our fake file `fake_window_restore_file_`.
  void OnSaveWindow(const app_restore::WindowInfo& window_info) {
    aura::Window* window = window_info.window;
    DCHECK(window);

    const int32_t restore_window_id =
        window->GetProperty(app_restore::kRestoreWindowIdKey);
    if (fake_window_restore_file_.contains(restore_window_id)) {
      fake_window_restore_file_[restore_window_id].call_count++;
    } else {
      fake_window_restore_file_[restore_window_id] = WindowInfo();
    }

    fake_window_restore_file_[restore_window_id].info = window_info;
  }

  // A map which is a fake representation of the window restore file.
  base::flat_map<int32_t, WindowInfo> fake_window_restore_file_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};
};

// Tests window save with setting on or off.
TEST_F(WindowRestoreControllerTest, WindowSaveDisabled) {
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  auto window1 =
      CreateAppWindow(gfx::Rect(600, 600), chromeos::AppType::BROWSER);
  auto window2 =
      CreateAppWindow(gfx::Rect(600, 600), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Disable window restore.
  app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(account_id, false);

  auto* window1_state = WindowState::Get(window1.get());
  auto* window2_state = WindowState::Get(window2.get());

  // Window minimization should not trigger window save with the
  // user preference off.
  window1_state->Minimize();
  window2_state->Minimize();
  EXPECT_EQ(0, GetSaveWindowsCount(window1.get()));
  EXPECT_EQ(0, GetSaveWindowsCount(window2.get()));

  // Enable window restore.
  app_restore::AppRestoreInfo::GetInstance()->SetRestorePref(account_id, true);

  // Setting the user preference to true should trigger window save
  // immediately.
  EXPECT_EQ(1, GetSaveWindowsCount(window1.get()));
  EXPECT_EQ(1, GetSaveWindowsCount(window2.get()));
}

// Tests that data gets saved when changing a window's window state.
TEST_F(WindowRestoreControllerTest, WindowStateChanged) {
  auto window =
      CreateAppWindow(gfx::Rect(600, 600), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  auto* window_state = WindowState::Get(window.get());
  window_state->Minimize();
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));

  window_state->Unminimize();
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  window_state->Activate();
  EXPECT_EQ(3, GetSaveWindowsCount(window.get()));

  // Maximize and restore will invoke two calls to SaveWindows because
  // their animations also change the bounds of the window. The actual writing
  // to the database is throttled, so this is ok.
  window_state->Maximize();
  EXPECT_EQ(5, GetSaveWindowsCount(window.get()));

  window_state->Restore();
  EXPECT_EQ(7, GetSaveWindowsCount(window.get()));

  PerformAcceleratorAction(AcceleratorAction::kWindowCycleSnapLeft, {});
  EXPECT_EQ(8, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when moving a window to another desk.
TEST_F(WindowRestoreControllerTest, WindowMovedDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window =
      CreateAppWindow(gfx::Rect(200, 200), chromeos::AppType::BROWSER);
  aura::Window* previous_parent = window->parent();
  ResetSaveWindowsCount();

  // Move the window to the desk on the right. Test that we save the window in
  // the database.
  PerformAcceleratorAction(
      AcceleratorAction::kDesksMoveActiveItemRight,
      {ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN});
  ASSERT_NE(previous_parent, window->parent());
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved correctly when assigning a window to all desks.
TEST_F(WindowRestoreControllerTest, AssignToAllDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window =
      CreateAppWindow(gfx::Rect(100, 100), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Assign |window| to all desks. This should trigger a save.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceVisibleOnAllWorkspaces);
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));

  // An all desks window should have a populated `desk_id` but not `desk_guid`.
  std::optional<app_restore::WindowInfo> window_info =
      GetWindowInfo(window.get());
  ASSERT_TRUE(window_info);
  EXPECT_EQ(aura::client::kWindowWorkspaceVisibleOnAllWorkspaces,
            window_info->desk_id);
  EXPECT_FALSE(window_info->desk_guid.is_valid());

  // Unassign |window| from all desks. This should trigger a save.
  window->SetProperty(aura::client::kWindowWorkspaceKey,
                      aura::client::kWindowWorkspaceUnassignedWorkspace);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  // A non-all desks window should not have a populated `desk_id`.
  std::optional<app_restore::WindowInfo> window_info2 =
      GetWindowInfo(window.get());
  ASSERT_TRUE(window_info2);
  EXPECT_FALSE(window_info2->desk_id);
}

// Tests that data gets saved when moving a window to another display using the
// accelerator.
TEST_F(WindowRestoreControllerTest, WindowMovedDisplay) {
  UpdateDisplay("800x700,801+0-800x700");

  auto window =
      CreateAppWindow(gfx::Rect(50, 50, 200, 200), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Move the window to the next display. Test that we save the window in
  // the database.
  PerformAcceleratorAction(AcceleratorAction::kMoveActiveWindowBetweenDisplays,
                           {});
  ASSERT_TRUE(
      gfx::Rect(801, 0, 800, 800).Contains(window->GetBoundsInScreen()));
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when dragging a window.
TEST_F(WindowRestoreControllerTest, WindowDragged) {
  auto window =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Test that even if we move n times, we will only save to file once.
  const gfx::Point point_on_frame(200, 16);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(point_on_frame);
  event_generator->PressLeftButton();
  for (int i = 0; i < 5; ++i)
    event_generator->MoveMouseBy(15, 15);
  event_generator->ReleaseLeftButton();

  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

TEST_F(WindowRestoreControllerTest, TabletModeChange) {
  // Tests that with no windows, nothing gets save when entering or exiting
  // tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  auto window1 =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  auto window2 =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Tests that we save each window when entering or exiting tablet mode. Due to
  // many possible things changing during a tablet switch (window state, bounds,
  // etc.), we cannot determine exactly how many saves there will be, but there
  // should be more than one per window.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_GT(GetSaveWindowsCount(window1.get()), 1);
  EXPECT_GT(GetSaveWindowsCount(window2.get()), 1);

  ResetSaveWindowsCount();
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_GT(GetSaveWindowsCount(window1.get()), 1);
  EXPECT_GT(GetSaveWindowsCount(window2.get()), 1);
}

TEST_F(WindowRestoreControllerTest, DisplayAddRemove) {
  UpdateDisplay("800x700, 800x700");

  auto window =
      CreateAppWindow(gfx::Rect(800, 0, 400, 400), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);

  // Remove the secondary display. Doing so will change both the bounds of the
  // window and activate it, resulting in a double save.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  EXPECT_EQ(0, GetSaveWindowsCount(window.get()));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  // Reconnect the secondary display. PersistentWindowController will move the
  // window back to the secondary display, so a save should be triggered.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(3, GetSaveWindowsCount(window.get()));
}

TEST_F(WindowRestoreControllerTest, Activation) {
  auto window1 =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  auto window2 =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  auto window3 =
      CreateAppWindow(gfx::Rect(400, 400), chromeos::AppType::BROWSER);
  ResetSaveWindowsCount();

  // Tests that an activation will save once for each window.
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(3, GetTotalSaveWindowsCount());

  // Tests that most recently used windows have the lowest activation index.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());

  EXPECT_EQ(2, GetActivationIndex(window1.get()));
  EXPECT_EQ(1, GetActivationIndex(window2.get()));
  EXPECT_EQ(0, GetActivationIndex(window3.get()));
}

// Tests that the mock widget created from window restore we will use in other
// tests works as expected.
TEST_F(WindowRestoreControllerTest, TestWindowRestoredWidget) {
  const int32_t kActivationIndex = 2;
  views::Widget* widget = CreateTestWindowRestoredWidget(kActivationIndex);
  int32_t* activation_index =
      widget->GetNativeWindow()->GetProperty(app_restore::kActivationIndexKey);
  ASSERT_TRUE(activation_index);
  EXPECT_EQ(kActivationIndex, *activation_index);

  // Since `widget` is the topmost window, it can be activated.
  EXPECT_TRUE(wm::CanActivateWindow(widget->GetNativeWindow()));
  EXPECT_TRUE(widget->IsActive());

  // Activation permissions are restored in a post task. Spin the run loop and
  // verify.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm::CanActivateWindow(widget->GetNativeWindow()));
}

// Tests that widgets are restored to their proper stacking order, even if they
// are restored out-of-order.
TEST_F(WindowRestoreControllerTest, Stacking) {
  // Create a window that is a child of the active desk's container.
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  auto non_restored_sibling = CreateTestWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(non_restored_sibling.get()));

  // Simulate restoring windows out-of-order, starting with `window_4`. Restored
  // windows should be placed below non-restored windows so `window_4` should be
  // placed at the bottom.
  auto* window_4 = CreateTestWindowRestoredWidget(4)->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, non_restored_sibling.get()));

  // Restore `window_2` now. It should be stacked above `window_4`.
  auto* window_2 = CreateTestWindowRestoredWidget(2)->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, window_2, non_restored_sibling.get()));

  // Restore `window_3` now. It should be stacked above `window_4`.
  auto* window_3 = CreateTestWindowRestoredWidget(3)->GetNativeWindow();
  EXPECT_THAT(
      desk_container->children(),
      ElementsAre(window_4, window_3, window_2, non_restored_sibling.get()));

  // Restore `window_1` now. It should be stacked above `window_2`.
  auto* window_1 = CreateTestWindowRestoredWidget(1)->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, window_3, window_2, window_1,
                          non_restored_sibling.get()));

  // Restore `window_5` now.
  auto* window_5 = CreateTestWindowRestoredWidget(5)->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_5, window_4, window_3, window_2, window_1,
                          non_restored_sibling.get()));
}

// Tests that widgets are restored to their proper stacking order in a
// multi-display scenario.
TEST_F(WindowRestoreControllerTest, StackingMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");

  auto root_windows = Shell::GetAllRootWindows();
  auto* root_1 = root_windows[0].get();
  auto* root_2 = root_windows[1].get();
  auto* root_3 = root_windows[2].get();

  auto* desk_container_display_1 =
      desks_util::GetActiveDeskContainerForRoot(root_1);
  auto* desk_container_display_2 =
      desks_util::GetActiveDeskContainerForRoot(root_2);
  auto* desk_container_display_3 =
      desks_util::GetActiveDeskContainerForRoot(root_3);

  const gfx::Rect display_1_bounds(0, 0, 200, 200);
  const gfx::Rect display_2_bounds(801, 0, 200, 200);
  const gfx::Rect display_3_bounds(1602, 0, 200, 200);

  // Start simulating restoring windows out-of-order with the following naming
  // convention `window_<display>_<relative_stacking_order>`. Restore
  // `window_3_3` first.
  auto* window_3_3 = CreateTestWindowRestoredWidget(8, display_3_bounds, root_3)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_3->children(), ElementsAre(window_3_3));

  // Restore `window_2_1`.
  auto* window_2_1 = CreateTestWindowRestoredWidget(2, display_2_bounds, root_2)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_2->children(), ElementsAre(window_2_1));

  // Restore `window_1_2`.
  auto* window_1_2 = CreateTestWindowRestoredWidget(4, display_1_bounds, root_1)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_1->children(), ElementsAre(window_1_2));

  // Restore `window_3_2`.
  auto* window_3_2 = CreateTestWindowRestoredWidget(6, display_3_bounds, root_3)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_3->children(),
              ElementsAre(window_3_3, window_3_2));

  // Restore `window_1_3`.
  auto* window_1_3 = CreateTestWindowRestoredWidget(7, display_1_bounds, root_1)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_1->children(),
              ElementsAre(window_1_3, window_1_2));

  // Restore `window_2_2`.
  auto* window_2_2 = CreateTestWindowRestoredWidget(5, display_2_bounds, root_2)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_2->children(),
              ElementsAre(window_2_2, window_2_1));

  // Restore `window_1_1`.
  auto* window_1_1 = CreateTestWindowRestoredWidget(1, display_1_bounds, root_1)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_1->children(),
              ElementsAre(window_1_3, window_1_2, window_1_1));

  // Restore `window_3_1`.
  auto* window_3_1 = CreateTestWindowRestoredWidget(3, display_3_bounds, root_3)
                         ->GetNativeWindow();
  EXPECT_THAT(desk_container_display_3->children(),
              ElementsAre(window_3_3, window_3_2, window_3_1));
}

// Tests clamshell snapped window functionality when creating a window from
// window restore.
TEST_F(WindowRestoreControllerTest, ClamshellSnapWindow) {
  UpdateDisplay("800x700");

  // Add two entries to our fake window restore file, one snapped left and the
  // other snapped right.
  const gfx::Rect restored_bounds(200, 200);
  AddEntryToFakeFile(/*restore_window_id=*/2, restored_bounds,
                     chromeos::WindowStateType::kPrimarySnapped);
  AddEntryToFakeFile(/*restore_window_id=*/3, restored_bounds,
                     chromeos::WindowStateType::kSecondarySnapped);

  // Create two window restore windows with the same restore window ids as the
  // entries we added. Test they are snapped and have snapped bounds.
  aura::Window* left_window =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  aura::Window* right_window =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  auto* left_window_state = WindowState::Get(left_window);
  auto* right_window_state = WindowState::Get(right_window);
  EXPECT_TRUE(left_window_state->IsSnapped());
  EXPECT_TRUE(right_window_state->IsSnapped());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SnapPosition::kPrimary, nullptr, chromeos::kDefaultSnapRatio,
                /*account_for_divider_width=*/false),
            left_window->GetBoundsInScreen());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SnapPosition::kSecondary, nullptr, chromeos::kDefaultSnapRatio,
                /*account_for_divider_width=*/false),
            right_window->GetBoundsInScreen());

  // Test that after restoring the snapped windows, they have the bounds we
  // saved into the fake file.
  left_window_state->Restore();
  right_window_state->Restore();
  EXPECT_EQ(restored_bounds, left_window->GetBoundsInScreen());
  EXPECT_EQ(restored_bounds, right_window->GetBoundsInScreen());
}

// Tests clamshell Floated window functionality when creating a window from
// window restore.
TEST_F(WindowRestoreControllerTest, ClamshellFloatWindow) {
  // Add one floated window entry to our fake window restore file.
  const gfx::Rect restored_bounds(200, 200);
  AddEntryToFakeFile(/*restore_window_id=*/1, restored_bounds,
                     chromeos::WindowStateType::kFloated);

  // Create one window restore window with the same restore window id as the
  // entries we added. Test they are floated and have moved floated bounds.
  aura::Window* floated_window =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1)
          ->GetNativeWindow();
  auto* floated_window_state = WindowState::Get(floated_window);
  EXPECT_TRUE(floated_window_state->IsFloated());

  auto* float_controller = Shell::Get()->float_controller();
  EXPECT_EQ(float_controller->GetFloatWindowClamshellBounds(
                floated_window, chromeos::FloatStartLocation::kBottomRight),
            floated_window->GetBoundsInScreen());

  // Test that after restoring the floated windows, they have the bounds we
  // saved into the fake file.
  floated_window_state->Restore();
  EXPECT_EQ(restored_bounds, floated_window->GetBoundsInScreen());
}

// Tests that windows floated in tablet mode get restored properly.
TEST_F(WindowRestoreControllerTest, TabletFloatWindow) {
  TabletModeControllerTestApi().EnterTabletMode();

  auto floated_window =
      CreateAppWindow(gfx::Rect(600, 600), chromeos::AppType::BROWSER);
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Close the window and fake launching a window from full restore. Verify that
  // is is floated.
  const int32_t restore_window_id =
      floated_window->GetProperty(app_restore::kRestoreWindowIdKey);
  floated_window.reset();
  aura::Window* restored_floated_window =
      CreateTestWindowRestoredWidgetFromRestoreId(restore_window_id)
          ->GetNativeWindow();
  EXPECT_TRUE(WindowState::Get(restored_floated_window)->IsFloated());
}

// Tests window restore behavior when a display is disconnected before
// restoration.
TEST_F(WindowRestoreControllerTest, DisconnectedDisplay) {
  UpdateDisplay("800x700");
  const gfx::Rect display_1_bounds(0, 0, 200, 200);
  const gfx::Rect display_2_bounds(801, 0, 200, 200);
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t disconnected_id = primary_id + 1;
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  const chromeos::WindowStateType window_state =
      chromeos::WindowStateType::kNormal;

  // Add three entries to our fake restore file. They will all be on the primary
  // display.
  AddEntryToFakeFile(/*restore_window_id=*/1, display_1_bounds, window_state,
                     /*activation_index=*/1, primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/3, display_1_bounds, window_state,
                     /*activation_index=*/3, primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/5, display_1_bounds, window_state,
                     /*activation_index=*/5, primary_id);

  // Add three entries to our fake restore file. They will all be on the
  // disconnected display.
  AddEntryToFakeFile(/*restore_window_id=*/2, display_2_bounds, window_state,
                     /*activation_index=*/2, disconnected_id);
  AddEntryToFakeFile(/*restore_window_id=*/4, display_2_bounds, window_state,
                     /*activation_index=*/4, disconnected_id);
  AddEntryToFakeFile(/*restore_window_id=*/6, display_2_bounds, window_state,
                     /*activation_index=*/6, disconnected_id);

  // Simulate restoring windows out-of-order, starting with `window_3`.
  ASSERT_EQ(0u, desk_container->children().size());
  auto* window_3 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(), ElementsAre(window_3));

  // Restore `window_4`.
  auto* window_4 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/4)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(), ElementsAre(window_4, window_3));

  // Restore `window_2`.
  auto* window_2 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, window_3, window_2));

  // Restore `window_1`.
  auto* window_1 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, window_3, window_2, window_1));

  // Restore `window_6`.
  auto* window_6 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/6)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_6, window_4, window_3, window_2, window_1));

  // Restore `window_5`.
  auto* window_5 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/5)
          ->GetNativeWindow();
  EXPECT_THAT(
      desk_container->children(),
      ElementsAre(window_6, window_5, window_4, window_3, window_2, window_1));
}

// Tests that the splitview data in tablet is saved properly.
TEST_F(WindowRestoreControllerTest, TabletSplitviewWindow) {
  TabletModeControllerTestApi().EnterTabletMode();

  const gfx::Rect bounds(300, 300);
  auto window1 =
      CreateAppWindow(gfx::Rect(300, 300), chromeos::AppType::BROWSER);
  auto window2 =
      CreateAppWindow(gfx::Rect(300, 300), chromeos::AppType::BROWSER);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);

  std::optional<app_restore::WindowInfo> window1_info =
      GetWindowInfo(window1.get());
  std::optional<app_restore::WindowInfo> window2_info =
      GetWindowInfo(window2.get());
  ASSERT_TRUE(window1_info);
  ASSERT_TRUE(window2_info);
  ASSERT_TRUE(window1_info->window_state_type);
  ASSERT_TRUE(window2_info->window_state_type);

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            *window1_info->window_state_type);
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            *window2_info->window_state_type);
  EXPECT_EQ(bounds, *window1_info->current_bounds);
  EXPECT_EQ(bounds, *window2_info->current_bounds);
}

// Tests tablet snapped window functionality when creating a window from window
// restore.
TEST_F(WindowRestoreControllerTest, TabletSnapWindow) {
  // Add two entries to our fake window restore file, one snapped left and the
  // other snapped right.
  const gfx::Rect restored_bounds(200, 200);
  AddEntryToFakeFile(/*restore_window_id=*/2, restored_bounds,
                     chromeos::WindowStateType::kPrimarySnapped);
  AddEntryToFakeFile(/*restore_window_id=*/3, restored_bounds,
                     chromeos::WindowStateType::kSecondarySnapped);

  TabletModeControllerTestApi().EnterTabletMode();

  // Create two window restore windows with the same restore window ids as the
  // entries we added. Test they are snapped and have snapped bounds.
  aura::Window* left_window =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  aura::Window* right_window =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  auto* left_window_state = WindowState::Get(left_window);
  auto* right_window_state = WindowState::Get(right_window);
  EXPECT_TRUE(left_window_state->IsSnapped());
  EXPECT_TRUE(right_window_state->IsSnapped());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SnapPosition::kPrimary, nullptr, chromeos::kDefaultSnapRatio,
                /*account_for_divider_width=*/true),
            left_window->GetBoundsInScreen());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SnapPosition::kSecondary, nullptr, chromeos::kDefaultSnapRatio,
                /*account_for_divider_width=*/true),
            right_window->GetBoundsInScreen());
  EXPECT_EQ(left_window, split_view_controller->primary_window());
  EXPECT_EQ(right_window, split_view_controller->secondary_window());

  TabletModeControllerTestApi().LeaveTabletMode();

  // Test that after restoring the snapped windows, they have the bounds we
  // saved into the fake file.
  left_window_state->Restore();
  right_window_state->Restore();
  EXPECT_EQ(restored_bounds, left_window->GetBoundsInScreen());
  EXPECT_EQ(restored_bounds, right_window->GetBoundsInScreen());
}

// Tests window restore behavior when a display size changes.
TEST_F(WindowRestoreControllerTest, DisplaySizeChange) {
  constexpr int kRestoreId = 1;
  UpdateDisplay("500x400");

  // Add an entry for a window that is larger than the current display size.
  // This simulates a user using a larger display than the one they are
  // restoring to.
  AddEntryToFakeFile(kRestoreId, gfx::Rect(0, 0, 800, 800),
                     chromeos::WindowStateType::kNormal);

  // Restore the window. Its bounds should be within the current display and its
  // window state should be unaffected.
  auto* restored_window =
      CreateTestWindowRestoredWidgetFromRestoreId(kRestoreId)
          ->GetNativeWindow();
  auto restored_bounds = restored_window->GetBoundsInScreen();
  EXPECT_LE(restored_bounds.width(), 500);
  EXPECT_LE(restored_bounds.height(), 400);
  EXPECT_EQ(gfx::Point(0, 0), restored_bounds.origin());
  EXPECT_TRUE(WindowState::Get(restored_window)->IsNormalStateType());
}

// Tests window restore behavior for when a window saved in clamshell mode is
// restored as expected in tablet mode.
TEST_F(WindowRestoreControllerTest, ClamshellToTablet) {
  constexpr int kRestoreId = 1;

  // Add a normal window to the fake file.
  const gfx::Rect clamshell_bounds(400, 400);
  AddEntryToFakeFile(kRestoreId, clamshell_bounds,
                     chromeos::WindowStateType::kNormal);

  // Restore the window after entering tablet mode, it should be maximized.
  TabletModeControllerTestApi().EnterTabletMode();
  auto* restored_window =
      CreateTestWindowRestoredWidgetFromRestoreId(kRestoreId)
          ->GetNativeWindow();
  EXPECT_TRUE(WindowState::Get(restored_window)->IsMaximized());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(restored_window),
            restored_window->GetBoundsInScreen());

  // Leave tablet mode. The window should have the saved bounds.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(WindowState::Get(restored_window)->IsMaximized());
  EXPECT_EQ(clamshell_bounds, restored_window->GetBoundsInScreen());
}

// Tests window restore behavior for when a window saved in tablet mode is
// restored as expected in clamshell mode.
TEST_F(WindowRestoreControllerTest, TabletToClamshell) {
  TabletModeControllerTestApi().EnterTabletMode();

  // The tablet mode window manager watches windows when they are added, then
  // tracks them when the window is shown. They must be resizable when tracked,
  // so we use a TestWidgetBuilder instead of `CreateTestWindow()`, which would
  // show the window before we can make it resizable.
  const gfx::Rect expected_bounds(300, 300);
  TestWidgetBuilder builder;
  views::Widget* widget = builder.SetTestWidgetDelegate()
                              .SetBounds(expected_bounds)
                              .SetContext(Shell::GetPrimaryRootWindow())
                              .SetShow(false)
                              .SetWindowProperty(chromeos::kAppTypeKey,
                                                 chromeos::AppType::CHROME_APP)
                              .BuildOwnedByNativeWidget();
  SetResizable(widget);
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  ASSERT_TRUE(WindowState::Get(window)->IsMaximized());
  ASSERT_EQ(screen_util::GetMaximizedWindowBoundsInParent(window),
            window->GetBoundsInScreen());

  // Check that the values in the fake file can be restored in clamshell mode.
  std::optional<app_restore::WindowInfo> window_info = GetWindowInfo(window);
  ASSERT_TRUE(window_info);
  ASSERT_TRUE(window_info->activation_index);
  ASSERT_TRUE(window_info->current_bounds);
  ASSERT_TRUE(window_info->window_state_type);
  EXPECT_EQ(expected_bounds, *window_info->current_bounds);
  EXPECT_EQ(chromeos::WindowStateType::kDefault,
            *window_info->window_state_type);

  const int restore_id = window->GetProperty(app_restore::kRestoreWindowIdKey);

  // Leave tablet mode, and then mock creating the window from window restore
  // file. Test that the state and bounds are as expected in clamshell mode.
  TabletModeControllerTestApi().LeaveTabletMode();
  auto* restored_window =
      CreateTestWindowRestoredWidgetFromRestoreId(restore_id)
          ->GetNativeWindow();
  EXPECT_TRUE(WindowState::Get(restored_window)->IsNormalStateType());
  EXPECT_EQ(expected_bounds, window->GetBoundsInScreen());
}

// Tests that when windows are restored in tablet mode the hotseat is hidden.
// See crbug.com/1202923.
TEST_F(WindowRestoreControllerTest, HotseatIsHiddenOnRestoration) {
  // Enter tablet mode and check that the hotseat is not hidden.
  TabletModeControllerTestApi().EnterTabletMode();
  HotseatWidget* hotseat_widget = GetPrimaryShelf()->hotseat_widget();
  EXPECT_EQ(HotseatState::kShownHomeLauncher, hotseat_widget->state());
  auto* app_list_widget = views::Widget::GetWidgetForNativeWindow(
      Shell::Get()->app_list_controller()->GetWindow());
  ASSERT_TRUE(app_list_widget->IsActive());

  // Add two entries, where the window highest on the z-order is minimized.
  // Restore both entries. The hotseat should now be hidden.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  AddEntryToFakeFile(/*restore_window_id=*/1, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kMinimized,
                     /*activation_index=*/1, /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/2, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kNormal,
                     /*activation_index=*/2, /*display_id=*/primary_id);
  views::Widget* restored_widget_1 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  views::Widget* restored_widget_2 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2);

  EXPECT_FALSE(restored_widget_1->IsVisible());
  EXPECT_FALSE(restored_widget_1->IsActive());

  EXPECT_TRUE(restored_widget_2->IsActive());
  EXPECT_TRUE(restored_widget_2->IsVisible());

  EXPECT_EQ(HotseatState::kHidden, hotseat_widget->state());
  EXPECT_FALSE(app_list_widget->IsActive());
}

// Tests that the app list isn't deactivated when all restored windows are
// minimized.
TEST_F(WindowRestoreControllerTest,
       AppListNotDeactivatedWhenAllWindowsMinimized) {
  // Enter tablet mode and ensure the app list is active.
  TabletModeControllerTestApi().EnterTabletMode();
  auto* app_list_widget = views::Widget::GetWidgetForNativeWindow(
      Shell::Get()->app_list_controller()->GetWindow());
  ASSERT_TRUE(app_list_widget->IsActive());

  // Create multiple minimized entries and restore them. The app list should
  // still be active.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  AddEntryToFakeFile(/*restore_window_id=*/1, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kMinimized,
                     /*activation_index=*/1, /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/2, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kMinimized,
                     /*activation_index=*/2, /*display_id=*/primary_id);
  views::Widget* restored_widget_1 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  views::Widget* restored_widget_2 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2);
  EXPECT_FALSE(restored_widget_1->IsVisible());
  EXPECT_FALSE(restored_widget_2->IsVisible());
  EXPECT_FALSE(restored_widget_1->IsActive());
  EXPECT_FALSE(restored_widget_2->IsActive());
  EXPECT_TRUE(app_list_widget->IsActive());
}

// Tests that the posted task for clearing a window's
// `app_restore::kLaunchedFromAppRestoreKey` is cancelled when that window is
// destroyed.
TEST_F(WindowRestoreControllerTest, RestorePropertyClearCallback) {
  // Restore a window.
  AddEntryToFakeFile(/*restore_window_id=*/1, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kNormal);
  views::Widget* restored_widget =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  ASSERT_TRUE(restored_widget->GetNativeWindow()->GetProperty(
      app_restore::kLaunchedFromAppRestoreKey));

  // Destroy the window immediately. There should be no restore property clear
  // callbacks left.
  ASSERT_EQ(1u, GetRestorePropertyClearCallbacks().size());
  restored_widget->CloseNow();
  EXPECT_EQ(0u, GetRestorePropertyClearCallbacks().size());
}

// Tests window restore behavior for when a ARC window is created without an
// associated task.
TEST_F(WindowRestoreControllerTest, ArcAppWindowCreatedWithoutTask) {
  constexpr int kRestoreId = 1;

  // Create enough desks so that we can parent to the expected desk.
  auto* desks_controller = DesksController::Get();
  for (int i = 0; i < 4; ++i)
    desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  // Add a normal window to the fake file. The target desk is desk 3.
  AddEntryToFakeFile(kRestoreId, gfx::Rect(400, 400),
                     chromeos::WindowStateType::kNormal, 3);

  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Restore the window, it should go to the invisible unparented container for
  // now.
  auto* restored_window = CreateTestWindowRestoredWidgetFromRestoreId(
                              kRestoreId, chromeos::AppType::ARC_APP,
                              /*is_taskless_arc_app=*/true)
                              ->GetNativeWindow();
  EXPECT_EQ(
      Shell::GetContainer(root_window, kShellWindowId_UnparentedContainer),
      restored_window->parent());

  // Simulate having the task ready. Our `restored_window` should now be
  // parented to the desk associated with desk 3, which is desk D.
  WindowRestoreController::Get()->OnParentWindowToValidContainer(
      restored_window);
  EXPECT_EQ(Shell::GetContainer(root_window, kShellWindowId_DeskContainerD),
            restored_window->parent());
}

// Tests that parenting ARC windows to hidden container works in the multi
// display scenario, including if a display gets disconnected partway through.
TEST_F(WindowRestoreControllerTest,
       ArcAppWindowCreatedWithoutTaskMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700");

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);

  aura::Window* primary_root_window = Shell::GetPrimaryRootWindow();
  aura::Window* secondary_root_window = Shell::GetAllRootWindows()[1];

  // Create enough desks so that we can parent to the expected desk.
  auto* desks_controller = DesksController::Get();
  for (int i = 0; i < 4; ++i)
    desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  // Add two normal windows to the fake file. The target desk is desk 3 and the
  // target display is the secondary one.
  constexpr int kRestoreId1 = 1;
  constexpr int kRestoreId2 = 2;
  AddEntryToFakeFile(kRestoreId1, gfx::Rect(900, 0, 400, 400),
                     chromeos::WindowStateType::kNormal, 3);
  AddEntryToFakeFile(kRestoreId2, gfx::Rect(900, 0, 400, 400),
                     chromeos::WindowStateType::kNormal, 3);

  // Restore the first window, it should go to the invisible unparented
  // container for the secondary display until the ARC task is ready.
  auto* restored_window1 = CreateTestWindowRestoredWidgetFromRestoreId(
                               kRestoreId1, chromeos::AppType::ARC_APP,
                               /*is_taskless_arc_app=*/true)
                               ->GetNativeWindow();
  EXPECT_EQ(Shell::GetContainer(secondary_root_window,
                                kShellWindowId_UnparentedContainer),
            restored_window1->parent());
  WindowRestoreController::Get()->OnParentWindowToValidContainer(
      restored_window1);
  EXPECT_EQ(
      Shell::GetContainer(secondary_root_window, kShellWindowId_DeskContainerD),
      restored_window1->parent());

  // Restore the second window, it should also go to the invisible unparented
  // container for the secondary display.
  auto* restored_window2 = CreateTestWindowRestoredWidgetFromRestoreId(
                               kRestoreId2, chromeos::AppType::ARC_APP,
                               /*is_taskless_arc_app=*/true)
                               ->GetNativeWindow();
  EXPECT_EQ(Shell::GetContainer(secondary_root_window,
                                kShellWindowId_UnparentedContainer),
            restored_window2->parent());

  // Remove the secondary display. When the ARC task is ready, it should go to
  // container associated with desk 3 on the primary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  WindowRestoreController::Get()->OnParentWindowToValidContainer(
      restored_window2);
  EXPECT_EQ(
      Shell::GetContainer(primary_root_window, kShellWindowId_DeskContainerD),
      restored_window2->parent());
}

// Tests that windows that are out-of-bounds of the display they're being
// restored to are properly restored.
TEST_F(WindowRestoreControllerTest, OutOfBoundsWindows) {
  UpdateDisplay("800x700");
  const gfx::Rect kScreenBounds(0, 0, 800, 800);
  const gfx::Rect kPartialBounds(-100, 100, 200, 200);
  const gfx::Rect kFullBounds(801, 801, 400, 200);

  // Add an entry that is partially out-of-bounds, one that is completely
  // out-of-bounds, and one that is completely out-of-bounds and snapped.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  AddEntryToFakeFile(/*restore_window_id=*/1, kPartialBounds,
                     chromeos::WindowStateType::kNormal, /*activation_index=*/1,
                     /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/2, kFullBounds,
                     chromeos::WindowStateType::kNormal, /*activation_index=*/2,
                     /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_window_id=*/3, kFullBounds,
                     chromeos::WindowStateType::kPrimarySnapped,
                     /*activation_index=*/3, /*display_id=*/primary_id);

  // Restore the first window. The window should have the exact same bounds.
  const gfx::Rect& window_bounds_1 =
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/1)
          ->GetNativeWindow()
          ->GetBoundsInScreen();
  EXPECT_EQ(kPartialBounds, window_bounds_1);

  // Restore the second window. The window should be moved such that part of it
  // is within the display.
  gfx::Rect window_bounds_2(
      CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow()
          ->GetBoundsInScreen());
  EXPECT_TRUE(window_bounds_2.Intersects(kScreenBounds));
  EXPECT_LT(0, IntersectRects(kScreenBounds, window_bounds_2).size().GetArea());

  // Restore the third window. The window's restore bounds should be moved such
  // that part of it is within the display.
  const gfx::Rect& window_bounds_3 =
      WindowState::Get(
          CreateTestWindowRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
              ->GetNativeWindow())
          ->GetRestoreBoundsInScreen();
  EXPECT_TRUE(window_bounds_3.Intersects(kScreenBounds));
  EXPECT_LT(0, IntersectRects(kScreenBounds, window_bounds_3).size().GetArea());
}

// Tests that when the topmost window is a Window Restore'd window, it is
// activatable. See crbug.com/1229928.
TEST_F(WindowRestoreControllerTest, TopmostWindowIsActivatable) {
  // Create a window that is not restored and activate it.
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  auto window =
      CreateAppWindow(gfx::Rect(100, 100), chromeos::AppType::BROWSER);
  wm::ActivateWindow(window.get());
  ASSERT_TRUE(wm::IsActiveWindow(window.get()));

  // Create a Window Restore'd Chrome app.
  AddEntryToFakeFile(
      /*restore_window_id=*/2, gfx::Rect(200, 200),
      chromeos::WindowStateType::kNormal,
      /*activation_index=*/1, WindowTreeHostManager::GetPrimaryDisplayId());
  auto* restored_window1 =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/2, chromeos::AppType::CHROME_APP,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(restored_window1, window.get()));

  // Create a Window Restore'd window.
  auto* window_4 = CreateTestWindowRestoredWidget(4)->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, restored_window1, window.get()));

  // Check the Window Restore'd windows' properties.
  EXPECT_TRUE(
      restored_window1->GetProperty(app_restore::kLaunchedFromAppRestoreKey));
  EXPECT_TRUE(window_4->GetProperty(app_restore::kLaunchedFromAppRestoreKey));

  // Both the Window Restore'd windows shouldn't be activatable.
  EXPECT_FALSE(wm::CanActivateWindow(restored_window1));
  EXPECT_FALSE(wm::CanActivateWindow(window_4));

  // Destroy the non-restored window. The new topmost window will be
  // `restored_window1` so it should be activatable.
  window.reset();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(window_4, restored_window1));
  EXPECT_TRUE(
      restored_window1->GetProperty(app_restore::kLaunchedFromAppRestoreKey));
  EXPECT_TRUE(wm::CanActivateWindow(restored_window1));
  EXPECT_FALSE(wm::CanActivateWindow(window_4));
}

// Tests that when the topmost window is minimized, the next highest unminimized
// window is activated.
TEST_F(WindowRestoreControllerTest, NextTopmostWindowIsActivatable) {
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());

  // Create a minimized Window Restore'd browser. It should not be activatable.
  AddEntryToFakeFile(
      /*restore_window_id=*/2, gfx::Rect(200, 200),
      chromeos::WindowStateType::kMinimized,
      /*activation_index=*/2, WindowTreeHostManager::GetPrimaryDisplayId());
  auto* restored_window2 =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/2, chromeos::AppType::BROWSER,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(), ElementsAre(restored_window2));
  EXPECT_FALSE(wm::CanActivateWindow(restored_window2));

  // Create another minimized Window Restore'd browser which is below
  // `restored_window2` in the stacking order. Both restored windows should not
  // be activatable because they're minimized.
  AddEntryToFakeFile(
      /*restore_window_id=*/3, gfx::Rect(200, 200),
      chromeos::WindowStateType::kMinimized,
      /*activation_index=*/3, WindowTreeHostManager::GetPrimaryDisplayId());
  auto* restored_window3 =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/3, chromeos::AppType::BROWSER,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(restored_window3, restored_window2));
  EXPECT_FALSE(wm::CanActivateWindow(restored_window3));
  EXPECT_FALSE(wm::CanActivateWindow(restored_window2));

  // Create Window Restore'd browser which is below `restored_window3` in the
  // stacking order. Since it's the topmost unminimized window, it should be
  // activatable and will be activated so it will be added to the top of the
  // stacking order.
  AddEntryToFakeFile(
      /*restore_window_id=*/4, gfx::Rect(200, 200),
      chromeos::WindowStateType::kNormal,
      /*activation_index=*/4, WindowTreeHostManager::GetPrimaryDisplayId());
  auto* restored_window4 =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/4, chromeos::AppType::BROWSER,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  EXPECT_THAT(
      desk_container->children(),
      ElementsAre(restored_window3, restored_window2, restored_window4));
  EXPECT_TRUE(wm::CanActivateWindow(restored_window4));
  EXPECT_FALSE(wm::CanActivateWindow(restored_window3));
  EXPECT_FALSE(wm::CanActivateWindow(restored_window2));
  EXPECT_TRUE(wm::IsActiveWindow(restored_window4));
}

// Tests that when a window is restored to an inactive desk it is not
// activatable. See crbug.com/1237158.
TEST_F(WindowRestoreControllerTest, WindowsOnInactiveDeskAreNotActivatable) {
  // Create two desks and switch to the first desk. There should now be three in
  // total.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  const auto& desks = desks_controller->desks();
  desks_controller->ActivateDesk(desks[0].get(),
                                 DesksSwitchSource::kUserSwitch);
  ASSERT_EQ(3u, desks.size());

  // Create a Window Restore'd browser in the third desk. It should not be
  // activatable.
  AddEntryToFakeFile(
      /*restore_window_id=*/2, gfx::Rect(200, 200),
      chromeos::WindowStateType::kMinimized,
      /*activation_index=*/2, WindowTreeHostManager::GetPrimaryDisplayId(),
      /*desk_id=*/2);
  auto* restored_window2 =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/2, chromeos::AppType::BROWSER,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  EXPECT_FALSE(wm::CanActivateWindow(restored_window2));
}

// Tests that when a window is saved in overview, its pre-overview bounds are
// used. See https://crbug.com/1265750.
TEST_F(WindowRestoreControllerTest, WindowsSavedInOverview) {
  const gfx::Rect window_bounds(300, 200);
  auto browser_window =
      CreateAppWindow(window_bounds, chromeos::AppType::BROWSER);
  auto arc_window = CreateAppWindow(window_bounds, chromeos::AppType::ARC_APP);

  ToggleOverview();
  EXPECT_NE(window_bounds, browser_window->GetBoundsInScreen());
  EXPECT_NE(window_bounds, arc_window->GetBoundsInRootWindow());

  std::optional<app_restore::WindowInfo> browser_window_info =
      GetWindowInfo(browser_window.get());
  ASSERT_TRUE(browser_window_info);
  EXPECT_EQ(window_bounds, browser_window_info->current_bounds);

  std::optional<app_restore::WindowInfo> arc_window_info =
      GetWindowInfo(arc_window.get());
  ASSERT_TRUE(arc_window_info);
  EXPECT_EQ(window_bounds, arc_window_info->arc_extra_info->bounds_in_root);
}

// Tests that if overview is active, and a window gets launched because of full
// restore, we exit overview.
TEST_F(WindowRestoreControllerTest, WindowsRestoredWhileInOverview) {
  AddEntryToFakeFile(
      /*restore_window_id=*/1, gfx::Rect(900, 700, 300, 300),
      chromeos::WindowStateType::kNormal);

  ToggleOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Create a restored window. Test that we have exited overview.
  CreateTestWindowRestoredWidgetFromRestoreId(
      /*restore_window_id=*/1, chromeos::AppType::BROWSER,
      /*is_taskless_arc_app=*/false)
      ->GetNativeWindow();
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Tests that a window whose bounds are offscreen (were on a disconnected
// display), are restored such that at least 30% of the window is visible.
TEST_F(WindowRestoreControllerTest, WindowsMinimumVisibleArea) {
  UpdateDisplay("800x600");

  // Create a Window Restore'd browser that is was previously on a monitor at
  // the bottom right of the current display.
  const int window_length = 200;
  AddEntryToFakeFile(
      /*restore_window_id=*/1,
      gfx::Rect(900, 700, window_length, window_length),
      chromeos::WindowStateType::kNormal);
  auto* restored_window =
      CreateTestWindowRestoredWidgetFromRestoreId(
          /*restore_window_id=*/1, chromeos::AppType::BROWSER,
          /*is_taskless_arc_app=*/false)
          ->GetNativeWindow();
  const gfx::Rect& bounds_in_screen = restored_window->GetBoundsInScreen();

  // Check the intersection of the display bounds and the window bounds in
  // screen. The intersection should be non-empty (window is partially on the
  // display) and width and height should at least 60 (30% of the window is
  // visible).
  const gfx::Rect intersection =
      gfx::IntersectRects(gfx::Rect(0, 0, 800, 600), bounds_in_screen);
  const int minimum_length =
      std::round(kMinimumPercentOnScreenArea * window_length);
  EXPECT_FALSE(intersection.IsEmpty());
  EXPECT_GE(intersection.size().width(), minimum_length);
  EXPECT_GE(intersection.size().height(), minimum_length);
}

}  // namespace ash
