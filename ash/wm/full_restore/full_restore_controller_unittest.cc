// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/screen_util.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "base/cancelable_callback.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "components/full_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

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

class FullRestoreControllerTest : public AshTestBase, public aura::EnvObserver {
 public:
  // Struct which is the data in our fake full restore file.
  struct WindowInfo {
    int call_count = 0;
    std::unique_ptr<full_restore::WindowInfo> info;
  };

  FullRestoreControllerTest() = default;
  FullRestoreControllerTest(const FullRestoreControllerTest&) = delete;
  FullRestoreControllerTest& operator=(const FullRestoreControllerTest&) =
      delete;
  ~FullRestoreControllerTest() override = default;

  // Returns the number of times |window| has been saved to file since the last
  // ResetSaveWindowsCount call.
  int GetSaveWindowsCount(aura::Window* window) const {
    const int32_t restore_window_id =
        window->GetProperty(full_restore::kRestoreWindowIdKey);
    if (!base::Contains(fake_full_restore_file_, restore_window_id))
      return 0;
    return fake_full_restore_file_.at(restore_window_id).call_count;
  }

  // Returns the total number of saves since the last ResetSaveWindowsCount
  // call.
  int GetTotalSaveWindowsCount() const {
    int count = 0;
    for (const std::pair<int32_t, WindowInfo>& member :
         fake_full_restore_file_) {
      count += member.second.call_count;
    }
    return count;
  }

  void ResetSaveWindowsCount() {
    for (std::pair<int32_t, WindowInfo>& member : fake_full_restore_file_)
      member.second.call_count = 0;
  }

  // Returns window info for `window`.
  full_restore::WindowInfo* GetWindowInfo(aura::Window* window) const {
    const int32_t restore_window_id =
        window->GetProperty(full_restore::kRestoreWindowIdKey);
    if (!base::Contains(fake_full_restore_file_, restore_window_id))
      return nullptr;
    return fake_full_restore_file_.at(restore_window_id).info.get();
  }

  // Returns the stored activation index for |window|.
  int GetActivationIndex(aura::Window* window) const {
    full_restore::WindowInfo* window_info = GetWindowInfo(window);
    if (!window_info)
      return -1;
    absl::optional<int32_t> activation_index = window_info->activation_index;
    return activation_index.value_or(-1);
  }

  // Returns the restore property clear callbacks.
  const std::map<aura::Window*, base::CancelableOnceClosure>&
  GetRestorePropertyClearCallbacks() {
    return FullRestoreController::Get()->restore_property_clear_callbacks_;
  }

  // Mocks creating a widget that is launched from full restore service.
  views::Widget* CreateTestFullRestoredWidget(
      int32_t activation_index,
      const gfx::Rect& bounds = gfx::Rect(200, 200),
      aura::Window* root_window = Shell::GetPrimaryRootWindow(),
      absl::optional<int32_t> restore_window_id = absl::nullopt,
      chromeos::WindowStateType window_state_type =
          chromeos::WindowStateType::kNormal,
      bool is_taskless_arc_app = false) {
    // Full restore widgets are inactive when created as we do not want to take
    // activation from a possible activated window, and we want to stack them in
    // a certain order.
    DCHECK(root_window->IsRootWindow());
    TestWidgetBuilder widget_builder;
    widget_builder.SetWidgetType(views::Widget::InitParams::TYPE_WINDOW)
        .SetBounds(bounds)
        .SetShow(false)
        .SetContext(root_window)
        .SetShowState(chromeos::ToWindowShowState(window_state_type))
        .SetWindowProperty(full_restore::kActivationIndexKey,
                           new int32_t(activation_index))
        .SetWindowProperty(full_restore::kLaunchedFromFullRestoreKey, true);
    // If this is not given, the window will get assigned an id in
    // `OnWindowInitialized()`.
    if (restore_window_id) {
      widget_builder.SetWindowProperty(full_restore::kRestoreWindowIdKey,
                                       *restore_window_id);
    }

    if (is_taskless_arc_app) {
      widget_builder
          .SetWindowProperty(full_restore::kParentToHiddenContainerKey, true)
          .SetWindowProperty(aura::client::kAppType,
                             static_cast<int>(AppType::ARC_APP));
    }

    views::Widget* widget = widget_builder.BuildOwnedByNativeWidget();
    SetResizable(widget);
    if (!is_taskless_arc_app)
      FullRestoreController::Get()->OnWidgetInitialized(widget);
    if (window_state_type != chromeos::WindowStateType::kMinimized)
      widget->Show();
    return widget;
  }

  // Mocks creating a widget based on the window info in
  // `fake_full_restore_file_`. Returns nullptr if there is not an entry that
  // matches `restore_window_id`.
  views::Widget* CreateTestFullRestoredWidgetFromRestoreId(
      int32_t restore_window_id,
      bool is_taskless_arc_app) {
    if (!fake_full_restore_file_.contains(restore_window_id))
      return nullptr;

    full_restore::WindowInfo* info =
        fake_full_restore_file_[restore_window_id].info.get();
    const gfx::Rect bounds = info->current_bounds.value_or(gfx::Rect(200, 200));
    const int32_t activation_index = info->activation_index.value_or(-1);
    const auto window_state_type =
        info->window_state_type.value_or(chromeos::WindowStateType::kNormal);
    return CreateTestFullRestoredWidget(
        activation_index, bounds, Shell::GetPrimaryRootWindow(),
        restore_window_id, window_state_type, is_taskless_arc_app);
  }

  views::Widget* CreateTestFullRestoredWidgetFromRestoreId(
      int32_t restore_window_id) {
    return CreateTestFullRestoredWidgetFromRestoreId(
        restore_window_id,
        /*is_taskless_arc_app=*/false);
  }

  void VerifyStackingOrder(aura::Window* parent,
                           const std::vector<aura::Window*>& expected_windows) {
    auto children = parent->children();
    EXPECT_EQ(children.size(), expected_windows.size());

    for (size_t i = 0; i < children.size(); ++i)
      EXPECT_EQ(children[i], expected_windows[i]);
  }

  // Adds an entry to the fake full restore file. Calling
  // If `CreateTestFullRestoreWidget` is called with a matching
  // `restore_window_id`, it will read and set the values set here.
  void AddEntryToFakeFile(int restore_window_id,
                          const gfx::Rect& bounds,
                          chromeos::WindowStateType window_state_type,
                          int32_t activation_index,
                          int64_t display_id,
                          int32_t desk_id) {
    DCHECK(!fake_full_restore_file_.contains(restore_window_id));
    auto window_info = std::make_unique<full_restore::WindowInfo>();
    window_info->current_bounds = bounds;
    window_info->window_state_type = window_state_type;
    window_info->activation_index = activation_index;
    window_info->display_id = display_id;
    window_info->desk_id = desk_id;
    fake_full_restore_file_[restore_window_id].info = std::move(window_info);
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
    scoped_feature_list_.InitAndEnableFeature(features::kFullRestore);

    AshTestBase::SetUp();

    FullRestoreController::Get()->SetReadWindowCallbackForTesting(
        base::BindRepeating(&FullRestoreControllerTest::OnGetWindowInfo,
                            base::Unretained(this)));
    FullRestoreController::Get()->SetSaveWindowCallbackForTesting(
        base::BindRepeating(&FullRestoreControllerTest::OnSaveWindow,
                            base::Unretained(this)));
    env_observation_.Observe(aura::Env::GetInstance());
  }

  void TearDown() override {
    env_observation_.Reset();

    AshTestBase::TearDown();
  }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {
    std::vector<AppType> kSupportedAppTypes = {
        AppType::BROWSER, AppType::CHROME_APP, AppType::ARC_APP};
    if (!base::Contains(kSupportedAppTypes,
                        static_cast<AppType>(
                            window->GetProperty(aura::client::kAppType)))) {
      return;
    }

    // If this is a new window, finds and sets a new restore window id.
    if (window->GetProperty(full_restore::kRestoreWindowIdKey) == 0) {
      int32_t restore_window_id = 1;
      while (fake_full_restore_file_.contains(restore_window_id))
        ++restore_window_id;
      window->SetProperty(full_restore::kRestoreWindowIdKey, restore_window_id);
    }

    // FullRestoreController relies on getting OnWindowInitialized events from
    // aura::Env via full_restore::FullRestoreInfo. That object does not exist
    // for ash unit tests, so we will observe aura::Env ourselves and forward
    // the event to FullRestoreController.
    FullRestoreController::Get()->OnWindowInitialized(window);
  }

 private:
  // Called when FullRestoreController saves a window to the file. Immediately
  // writes to our fake file |fake_full_restore_file_|.
  void OnSaveWindow(const full_restore::WindowInfo& window_info) {
    aura::Window* window = window_info.window;
    DCHECK(window);

    const int32_t restore_window_id =
        window->GetProperty(full_restore::kRestoreWindowIdKey);
    if (fake_full_restore_file_.contains(restore_window_id)) {
      fake_full_restore_file_[restore_window_id].call_count++;
    } else {
      fake_full_restore_file_[restore_window_id].info =
          std::make_unique<full_restore::WindowInfo>();
    }

    CopyWindowInfo(window_info,
                   fake_full_restore_file_[restore_window_id].info.get());
  }

  // Callback function that is run when FullRestoreController tries to read
  // window data from the file. Immediately reads from our fake file
  // `fake_full_restore_file_`.
  std::unique_ptr<full_restore::WindowInfo> OnGetWindowInfo(
      aura::Window* window) {
    DCHECK(window);
    const int32_t restore_window_id =
        window->GetProperty(full_restore::kRestoreWindowIdKey);
    if (!fake_full_restore_file_.contains(restore_window_id))
      return nullptr;

    auto window_info = std::make_unique<full_restore::WindowInfo>();
    CopyWindowInfo(*fake_full_restore_file_[restore_window_id].info,
                   window_info.get());
    return window_info;
  }

  // Copies the info from `src` to `out_dst` since `fullrestore::WindowInfo`
  // copy constructor is deleted.
  void CopyWindowInfo(const full_restore::WindowInfo& src,
                      full_restore::WindowInfo* out_dst) {
    out_dst->window = src.window;
    out_dst->activation_index = src.activation_index;
    out_dst->desk_id = src.desk_id;
    out_dst->visible_on_all_workspaces = src.visible_on_all_workspaces;
    out_dst->restore_bounds = src.restore_bounds;
    out_dst->current_bounds = src.current_bounds;
    out_dst->window_state_type = src.window_state_type;
    out_dst->display_id = src.display_id;
  }

  // A map which is a fake representation of the full restore file.
  base::flat_map<int32_t, WindowInfo> fake_full_restore_file_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that data gets saved when changing a window's window state.
TEST_F(FullRestoreControllerTest, WindowStateChanged) {
  auto window = CreateAppWindow(gfx::Rect(600, 600), AppType::BROWSER);
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

  PerformAcceleratorAction(WINDOW_CYCLE_SNAP_LEFT, {});
  EXPECT_EQ(8, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when moving a window to another desk.
TEST_F(FullRestoreControllerTest, WindowMovedDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window = CreateAppWindow(gfx::Rect(100, 100), AppType::BROWSER);
  aura::Window* previous_parent = window->parent();
  ResetSaveWindowsCount();

  // Move the window to the desk on the right. Test that we save the window in
  // the database.
  PerformAcceleratorAction(
      DESKS_MOVE_ACTIVE_ITEM_RIGHT,
      {ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN});
  ASSERT_NE(previous_parent, window->parent());
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when assigning a window to all desks.
TEST_F(FullRestoreControllerTest, AssignToAllDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(0, desks_controller->GetDeskIndex(
                   desks_controller->GetTargetActiveDesk()));

  auto window = CreateAppWindow(gfx::Rect(100, 100), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Assign |window| to all desks. This should trigger a save.
  window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, true);
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));

  // Unassign |window| from all desks. This should trigger a save.
  window->SetProperty(aura::client::kVisibleOnAllWorkspacesKey, false);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when moving a window to another display using the
// accelerator.
TEST_F(FullRestoreControllerTest, WindowMovedDisplay) {
  UpdateDisplay("800x800,801+0-800x800");

  auto window = CreateAppWindow(gfx::Rect(50, 50, 100, 100), AppType::BROWSER);
  ResetSaveWindowsCount();

  // Move the window to the next display. Test that we save the window in
  // the database.
  PerformAcceleratorAction(MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS, {});
  ASSERT_TRUE(
      gfx::Rect(801, 0, 800, 800).Contains(window->GetBoundsInScreen()));
  EXPECT_EQ(1, GetSaveWindowsCount(window.get()));
}

// Tests that data gets saved when dragging a window.
TEST_F(FullRestoreControllerTest, WindowDragged) {
  auto window = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
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

TEST_F(FullRestoreControllerTest, TabletModeChange) {
  // Tests that with no windows, nothing gets save when entering or exiting
  // tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(0, GetTotalSaveWindowsCount());

  auto window1 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window2 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
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

TEST_F(FullRestoreControllerTest, DisplayAddRemove) {
  UpdateDisplay("800x800,801+0-800x800");

  auto window = CreateAppWindow(gfx::Rect(800, 0, 400, 400), AppType::BROWSER);
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
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetSaveWindowsCount(window.get()));

  // Reconnect the secondary display. PersistentWindowController will move the
  // window back to the secondary display, so a save should be triggered.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(3, GetSaveWindowsCount(window.get()));
}

TEST_F(FullRestoreControllerTest, Activation) {
  auto window1 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window2 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
  auto window3 = CreateAppWindow(gfx::Rect(400, 400), AppType::BROWSER);
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

// Tests that the mock widget created from full restore we will use in other
// tests works as expected.
TEST_F(FullRestoreControllerTest, TestFullRestoredWidget) {
  const int32_t kActivationIndex = 2;
  views::Widget* widget = CreateTestFullRestoredWidget(kActivationIndex);
  int32_t* activation_index =
      widget->GetNativeWindow()->GetProperty(full_restore::kActivationIndexKey);
  ASSERT_TRUE(activation_index);
  EXPECT_EQ(kActivationIndex, *activation_index);

  // Widget cannot be activated and is not active after it is created from full
  // restore.
  EXPECT_FALSE(wm::CanActivateWindow(widget->GetNativeWindow()));
  EXPECT_FALSE(widget->IsActive());

  // Activation permissions are restored in a post task. Spin the run loop and
  // verify.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(wm::CanActivateWindow(widget->GetNativeWindow()));
}

// Tests that widgets are restored to their proper stacking order, even if they
// are restored out-of-order.
TEST_F(FullRestoreControllerTest, Stacking) {
  // Create a window that is a child of the active desk's container.
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  auto non_restored_sibling = CreateTestWindow();
  VerifyStackingOrder(desk_container, {non_restored_sibling.get()});

  // Simulate restoring windows out-of-order, starting with `window_4`. Restored
  // windows should be placed below non-restored windows so `window_4` should be
  // placed at the bottom.
  auto* window_4 = CreateTestFullRestoredWidget(4)->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, non_restored_sibling.get()});

  // Restore `window_2` now. It should be stacked above `window_4`.
  auto* window_2 = CreateTestFullRestoredWidget(2)->GetNativeWindow();
  VerifyStackingOrder(desk_container,
                      {window_4, window_2, non_restored_sibling.get()});

  // Restore `window_3` now. It should be stacked above `window_4`.
  auto* window_3 = CreateTestFullRestoredWidget(3)->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, window_3, window_2,
                                       non_restored_sibling.get()});

  // Restore `window_1` now. It should be stacked above `window_2`.
  auto* window_1 = CreateTestFullRestoredWidget(1)->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, window_3, window_2, window_1,
                                       non_restored_sibling.get()});

  // Restore `window_5` now.
  auto* window_5 = CreateTestFullRestoredWidget(5)->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_5, window_4, window_3, window_2,
                                       window_1, non_restored_sibling.get()});
}

// Tests that widgets are restored to their proper stacking order in a
// multi-display scenario.
TEST_F(FullRestoreControllerTest, StackingMultiDisplay) {
  UpdateDisplay("800x800,801+0-800x800,1602+0-800x800");

  auto root_windows = Shell::GetAllRootWindows();
  auto* root_1 = root_windows[0];
  auto* root_2 = root_windows[1];
  auto* root_3 = root_windows[2];

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
  auto* window_3_3 = CreateTestFullRestoredWidget(8, display_3_bounds, root_3)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_3, {window_3_3});

  // Restore `window_2_1`.
  auto* window_2_1 = CreateTestFullRestoredWidget(2, display_2_bounds, root_2)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_2, {window_2_1});

  // Restore `window_1_2`.
  auto* window_1_2 = CreateTestFullRestoredWidget(4, display_1_bounds, root_1)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_1, {window_1_2});

  // Restore `window_3_2`.
  auto* window_3_2 = CreateTestFullRestoredWidget(6, display_3_bounds, root_3)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_3, {window_3_3, window_3_2});

  // Restore `window_1_3`.
  auto* window_1_3 = CreateTestFullRestoredWidget(7, display_1_bounds, root_1)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_1, {window_1_3, window_1_2});

  // Restore `window_2_2`.
  auto* window_2_2 = CreateTestFullRestoredWidget(5, display_2_bounds, root_2)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_2, {window_2_2, window_2_1});

  // Restore `window_1_1`.
  auto* window_1_1 = CreateTestFullRestoredWidget(1, display_1_bounds, root_1)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_1,
                      {window_1_3, window_1_2, window_1_1});

  // Restore `window_3_1`.
  auto* window_3_1 = CreateTestFullRestoredWidget(3, display_3_bounds, root_3)
                         ->GetNativeWindow();
  VerifyStackingOrder(desk_container_display_3,
                      {window_3_3, window_3_2, window_3_1});
}

// Tests clamshell snapped window functionality when creating a window from full
// restore.
TEST_F(FullRestoreControllerTest, ClamshellSnapWindow) {
  UpdateDisplay("800x800");

  // Add two entries to our fake full restore file, one snapped left and the
  // other snapped right.
  const gfx::Rect restored_bounds(200, 200);
  AddEntryToFakeFile(/*restore_window_id=*/2, restored_bounds,
                     chromeos::WindowStateType::kLeftSnapped);
  AddEntryToFakeFile(/*restore_window_id=*/3, restored_bounds,
                     chromeos::WindowStateType::kRightSnapped);

  // Create two full restore windows with the same restore window ids as the
  // entries we added. Test they are snapped and have snapped bounds.
  aura::Window* left_window =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  aura::Window* right_window =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  auto* left_window_state = WindowState::Get(left_window);
  auto* right_window_state = WindowState::Get(right_window);
  EXPECT_TRUE(left_window_state->IsSnapped());
  EXPECT_TRUE(right_window_state->IsSnapped());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, nullptr),
            left_window->GetBoundsInScreen());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SplitViewController::RIGHT, nullptr),
            right_window->GetBoundsInScreen());

  // Test that after restoring the snapped windows, they have the bounds we
  // saved into the fake file.
  left_window_state->Restore();
  right_window_state->Restore();
  EXPECT_EQ(restored_bounds, left_window->GetBoundsInScreen());
  EXPECT_EQ(restored_bounds, right_window->GetBoundsInScreen());
}

// Tests full restore behavior when a display is disconnected before
// restoration.
TEST_F(FullRestoreControllerTest, DisconnectedDisplay) {
  UpdateDisplay("800x800");
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
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_3});

  // Restore `window_4`.
  auto* window_4 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/4)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, window_3});

  // Restore `window_2`.
  auto* window_2 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, window_3, window_2});

  // Restore `window_1`.
  auto* window_1 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/1)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_4, window_3, window_2, window_1});

  // Restore `window_6`.
  auto* window_6 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/6)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container,
                      {window_6, window_4, window_3, window_2, window_1});

  // Restore `window_5`.
  auto* window_5 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/5)
          ->GetNativeWindow();
  VerifyStackingOrder(desk_container, {window_6, window_5, window_4, window_3,
                                       window_2, window_1});
}

// Tests that the splitview data in tablet is saved properly.
TEST_F(FullRestoreControllerTest, TabletSplitviewWindow) {
  TabletModeControllerTestApi().EnterTabletMode();

  const gfx::Rect bounds(300, 300);
  auto window1 = CreateAppWindow(gfx::Rect(300, 300), AppType::BROWSER);
  auto window2 = CreateAppWindow(gfx::Rect(300, 300), AppType::BROWSER);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller->SnapWindow(window2.get(), SplitViewController::RIGHT);

  full_restore::WindowInfo* window1_info = GetWindowInfo(window1.get());
  full_restore::WindowInfo* window2_info = GetWindowInfo(window2.get());
  ASSERT_TRUE(window1_info);
  ASSERT_TRUE(window2_info);
  ASSERT_TRUE(window1_info->window_state_type);
  ASSERT_TRUE(window2_info->window_state_type);

  EXPECT_EQ(chromeos::WindowStateType::kLeftSnapped,
            *window1_info->window_state_type);
  EXPECT_EQ(chromeos::WindowStateType::kRightSnapped,
            *window2_info->window_state_type);
  EXPECT_EQ(bounds, *window1_info->current_bounds);
  EXPECT_EQ(bounds, *window2_info->current_bounds);
}

// Tests tablet snapped window functionality when creating a window from full
// restore.
TEST_F(FullRestoreControllerTest, TabletSnapWindow) {
  // Add two entries to our fake full restore file, one snapped left and the
  // other snapped right.
  const gfx::Rect restored_bounds(200, 200);
  AddEntryToFakeFile(/*restore_window_id=*/2, restored_bounds,
                     chromeos::WindowStateType::kLeftSnapped);
  AddEntryToFakeFile(/*restore_window_id=*/3, restored_bounds,
                     chromeos::WindowStateType::kRightSnapped);

  TabletModeControllerTestApi().EnterTabletMode();

  // Create two full restore windows with the same restore window ids as the
  // entries we added. Test they are snapped and have snapped bounds.
  aura::Window* left_window =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/2)
          ->GetNativeWindow();
  aura::Window* right_window =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/3)
          ->GetNativeWindow();
  auto* left_window_state = WindowState::Get(left_window);
  auto* right_window_state = WindowState::Get(right_window);
  EXPECT_TRUE(left_window_state->IsSnapped());
  EXPECT_TRUE(right_window_state->IsSnapped());
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, nullptr),
            left_window->GetBoundsInScreen());
  EXPECT_EQ(split_view_controller->GetSnappedWindowBoundsInScreen(
                SplitViewController::RIGHT, nullptr),
            right_window->GetBoundsInScreen());
  EXPECT_EQ(left_window, split_view_controller->left_window());
  EXPECT_EQ(right_window, split_view_controller->right_window());

  TabletModeControllerTestApi().LeaveTabletMode();

  // Test that after restoring the snapped windows, they have the bounds we
  // saved into the fake file.
  left_window_state->Restore();
  right_window_state->Restore();
  EXPECT_EQ(restored_bounds, left_window->GetBoundsInScreen());
  EXPECT_EQ(restored_bounds, right_window->GetBoundsInScreen());
}

// Tests full restore behavior when a display size changes.
TEST_F(FullRestoreControllerTest, DisplaySizeChange) {
  constexpr int kRestoreId = 1;
  UpdateDisplay("400x400");

  // Add an entry for a window that is larger than the current display size.
  // This simulates a user using a larger display than the one they are
  // restoring to.
  AddEntryToFakeFile(kRestoreId, gfx::Rect(0, 0, 800, 800),
                     chromeos::WindowStateType::kNormal);

  // Restore the window. Its bounds should be within the current display and its
  // window state should be unaffected.
  auto* restored_window =
      CreateTestFullRestoredWidgetFromRestoreId(kRestoreId)->GetNativeWindow();
  auto restored_bounds = restored_window->GetBoundsInScreen();
  EXPECT_LE(restored_bounds.width(), 400);
  EXPECT_LE(restored_bounds.height(), 400);
  EXPECT_EQ(gfx::Point(0, 0), restored_bounds.origin());
  EXPECT_TRUE(WindowState::Get(restored_window)->IsNormalStateType());
}

// Tests full restore behavior for when a window saved in clamshell mode is
// restored as expected in tablet mode.
TEST_F(FullRestoreControllerTest, ClamshellToTablet) {
  constexpr int kRestoreId = 1;

  // Add a normal window to the fake file.
  const gfx::Rect clamshell_bounds(400, 400);
  AddEntryToFakeFile(kRestoreId, clamshell_bounds,
                     chromeos::WindowStateType::kNormal);

  // Restore the window after entering tablet mode, it should be maximized.
  TabletModeControllerTestApi().EnterTabletMode();
  auto* restored_window =
      CreateTestFullRestoredWidgetFromRestoreId(kRestoreId)->GetNativeWindow();
  EXPECT_TRUE(WindowState::Get(restored_window)->IsMaximized());
  EXPECT_EQ(screen_util::GetMaximizedWindowBoundsInParent(restored_window),
            restored_window->GetBoundsInScreen());

  // Leave tablet mode. The window should have the saved bounds.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(WindowState::Get(restored_window)->IsMaximized());
  EXPECT_EQ(clamshell_bounds, restored_window->GetBoundsInScreen());
}

// Tests full restore behavior for when a window saved in tablet mode is
// restored as expected in clamshell mode.
TEST_F(FullRestoreControllerTest, TabletToClamshell) {
  TabletModeControllerTestApi().EnterTabletMode();

  // The tablet mode window manager watches windows when they are added, then
  // tracks them when the window is shown. They must be resizable when tracked,
  // so we use a TestWidgetBuilder instead of `CreateTestWindow()`, which would
  // show the window before we can make it resizable.
  const gfx::Rect expected_bounds(300, 300);
  TestWidgetBuilder builder;
  views::Widget* widget =
      builder.SetTestWidgetDelegate()
          .SetBounds(expected_bounds)
          .SetContext(Shell::GetPrimaryRootWindow())
          .SetShow(false)
          .SetWindowProperty(aura::client::kAppType,
                             static_cast<int>(AppType::CHROME_APP))
          .BuildOwnedByNativeWidget();
  SetResizable(widget);
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  ASSERT_TRUE(WindowState::Get(window)->IsMaximized());
  ASSERT_EQ(screen_util::GetMaximizedWindowBoundsInParent(window),
            window->GetBoundsInScreen());

  // Check that the values in the fake file can be restored in clamshell mode.
  full_restore::WindowInfo* window_info = GetWindowInfo(window);
  ASSERT_TRUE(window_info);
  ASSERT_TRUE(window_info->current_bounds);
  ASSERT_TRUE(window_info->window_state_type);
  EXPECT_EQ(expected_bounds, *window_info->current_bounds);
  EXPECT_EQ(chromeos::WindowStateType::kDefault,
            *window_info->window_state_type);

  const int restore_id = window->GetProperty(full_restore::kRestoreWindowIdKey);

  // Leave tablet mode, and then mock creating the window from full restore
  // file. Test that the state and bounds are as expected in clamshell mode.
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  TabletModeControllerTestApi().LeaveTabletMode();
  auto* restored_window =
      CreateTestFullRestoredWidgetFromRestoreId(restore_id)->GetNativeWindow();
  EXPECT_TRUE(WindowState::Get(restored_window)->IsNormalStateType());
  EXPECT_EQ(expected_bounds, window->GetBoundsInScreen());
}

// Tests that when windows are restored in tablet mode the hotseat is hidden.
// See crbug.com/1202923.
TEST_F(FullRestoreControllerTest, HotseatIsHiddenOnRestoration) {
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
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  views::Widget* restored_widget_2 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/2);
  EXPECT_FALSE(restored_widget_1->IsVisible());
  EXPECT_TRUE(restored_widget_2->IsVisible());
  EXPECT_FALSE(restored_widget_1->IsActive());
  EXPECT_FALSE(restored_widget_2->IsActive());
  EXPECT_EQ(HotseatState::kHidden, hotseat_widget->state());
  EXPECT_FALSE(app_list_widget->IsActive());
}

// Tests that the app list isn't deactivated when all restored windows are
// minimized.
TEST_F(FullRestoreControllerTest,
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
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  views::Widget* restored_widget_2 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/2);
  EXPECT_FALSE(restored_widget_1->IsVisible());
  EXPECT_FALSE(restored_widget_2->IsVisible());
  EXPECT_FALSE(restored_widget_1->IsActive());
  EXPECT_FALSE(restored_widget_2->IsActive());
  EXPECT_TRUE(app_list_widget->IsActive());
}

// Tests that the posted task for clearing a window's
// `full_restore::kLaunchedFromFullRestoreKey` is cancelled when that window is
// destroyed.
TEST_F(FullRestoreControllerTest, RestorePropertyClearCallback) {
  // Restore a window.
  AddEntryToFakeFile(/*restore_window_id=*/1, gfx::Rect(200, 200),
                     chromeos::WindowStateType::kNormal);
  views::Widget* restored_widget =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_window_id=*/1);
  ASSERT_TRUE(restored_widget->GetNativeWindow()->GetProperty(
      full_restore::kLaunchedFromFullRestoreKey));

  // Destroy the window immediately. There should be no restore property clear
  // callbacks left.
  ASSERT_EQ(1u, GetRestorePropertyClearCallbacks().size());
  restored_widget->CloseNow();
  EXPECT_EQ(0u, GetRestorePropertyClearCallbacks().size());
}

// Tests full restore behavior for when a ARC window is created without an
// associated task.
TEST_F(FullRestoreControllerTest, ArcAppWindowCreatedWithoutTask) {
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
  auto* restored_window = CreateTestFullRestoredWidgetFromRestoreId(
                              kRestoreId, /*is_taskless_arc_app=*/true)
                              ->GetNativeWindow();
  EXPECT_EQ(
      Shell::GetContainer(root_window, kShellWindowId_UnparentedContainer),
      restored_window->parent());

  // Simulate having the task ready. Our `restored_window` should now be
  // parented to the desk associated with desk 3, which is desk D.
  FullRestoreController::Get()->OnARCTaskReadyForUnparentedWindow(
      restored_window);
  EXPECT_EQ(Shell::GetContainer(root_window, kShellWindowId_DeskContainerD),
            restored_window->parent());
}

// Tests that parenting ARC windows to hidden container works in the multi
// display scenario, including if a display gets disconnected partway through.
TEST_F(FullRestoreControllerTest, ArcAppWindowCreatedWithoutTaskMultiDisplay) {
  UpdateDisplay("800x800,801+0-800x800");

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
  auto* restored_window1 = CreateTestFullRestoredWidgetFromRestoreId(
                               kRestoreId1, /*is_taskless_arc_app=*/true)
                               ->GetNativeWindow();
  EXPECT_EQ(Shell::GetContainer(secondary_root_window,
                                kShellWindowId_UnparentedContainer),
            restored_window1->parent());
  FullRestoreController::Get()->OnARCTaskReadyForUnparentedWindow(
      restored_window1);
  EXPECT_EQ(
      Shell::GetContainer(secondary_root_window, kShellWindowId_DeskContainerD),
      restored_window1->parent());

  // Restore the second window, it should also go to the invisible unparented
  // container for the secondary display.
  auto* restored_window2 = CreateTestFullRestoredWidgetFromRestoreId(
                               kRestoreId2, /*is_taskless_arc_app=*/true)
                               ->GetNativeWindow();
  EXPECT_EQ(Shell::GetContainer(secondary_root_window,
                                kShellWindowId_UnparentedContainer),
            restored_window2->parent());

  // Remove the secondary display. When the ARC task is ready, it should go to
  // container associated with desk 3 on the primary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  FullRestoreController::Get()->OnARCTaskReadyForUnparentedWindow(
      restored_window2);
  EXPECT_EQ(
      Shell::GetContainer(primary_root_window, kShellWindowId_DeskContainerD),
      restored_window2->parent());
}

// Tests that windows that are out-of-bounds of the display they're being
// restored to are properly restored.
TEST_F(FullRestoreControllerTest, OutOfBoundsWindows) {
  UpdateDisplay("800x800");
  const gfx::Rect kScreenBounds(0, 0, 800, 800);
  const gfx::Rect kPartialBounds(-100, 100, 200, 200);
  const gfx::Rect kFullBounds(801, 801, 400, 200);

  // Add an entry that is partially out-of-bounds, one that is completely
  // out-of-bounds, and one that is completely out-of-bounds and snapped.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  AddEntryToFakeFile(/*restore_id=*/1, kPartialBounds,
                     chromeos::WindowStateType::kNormal, /*activation_index=*/1,
                     /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_id=*/2, kFullBounds,
                     chromeos::WindowStateType::kNormal, /*activation_index=*/2,
                     /*display_id=*/primary_id);
  AddEntryToFakeFile(/*restore_id=*/3, kFullBounds,
                     chromeos::WindowStateType::kLeftSnapped,
                     /*activation_index=*/3, /*display_id=*/primary_id);

  // Restore the first window. The window should have the exact same bounds.
  const gfx::Rect& window_bounds_1 =
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_id=*/1)
          ->GetNativeWindow()
          ->GetBoundsInScreen();
  EXPECT_EQ(kPartialBounds, window_bounds_1);

  // Restore the second window. The window should be moved such that part of it
  // is within the display.
  gfx::Rect window_bounds_2(
      CreateTestFullRestoredWidgetFromRestoreId(/*restore_id=*/2)
          ->GetNativeWindow()
          ->GetBoundsInScreen());
  EXPECT_TRUE(window_bounds_2.Intersects(kScreenBounds));
  EXPECT_LT(0, IntersectRects(kScreenBounds, window_bounds_2).size().GetArea());

  // Restore the third window. The window's restore bounds should be moved such
  // that part of it is within the display.
  const gfx::Rect& window_bounds_3 =
      WindowState::Get(
          CreateTestFullRestoredWidgetFromRestoreId(/*restore_id=*/3)
              ->GetNativeWindow())
          ->GetRestoreBoundsInScreen();
  EXPECT_TRUE(window_bounds_3.Intersects(kScreenBounds));
  EXPECT_LT(0, IntersectRects(kScreenBounds, window_bounds_3).size().GetArea());
}

}  // namespace ash
