// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <array>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/test/test_desk_profiles_delegate.h"
#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_icon_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button_container.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class SavedDeskTest : public OverviewTestBase,
                      public MultiUserWindowManagerDelegate {
 public:
  SavedDeskTest() = default;
  SavedDeskTest(const SavedDeskTest&) = delete;
  SavedDeskTest& operator=(const SavedDeskTest&) = delete;
  ~SavedDeskTest() override = default;

  // Adds an entry to the desks model directly without capturing a desk. Allows
  // for testing the names and times of the UI directly.
  void AddEntry(const base::Uuid& uuid,
                const std::string& name,
                base::Time created_time,
                DeskTemplateType type) {
    AddSavedDeskEntry(ash_test_helper()->saved_desk_test_helper()->desk_model(),
                      uuid, name, created_time, type);
  }

  void AddEntry(const base::Uuid& uuid,
                const std::string& name,
                base::Time created_time,
                DeskTemplateSource source,
                DeskTemplateType type,
                std::unique_ptr<app_restore::RestoreData> restore_data) {
    AddSavedDeskEntry(ash_test_helper()->saved_desk_test_helper()->desk_model(),
                      uuid, name, created_time, source, type,
                      std::move(restore_data));
  }

  // Creates an app_restore::RestoreData object with `num_windows.size()` apps,
  // where the ith app has `num_windows[i]` windows. The windows
  // activation index is its creation order.
  std::unique_ptr<app_restore::RestoreData> CreateRestoreData(
      std::vector<int> num_windows) {
    auto restore_data = std::make_unique<app_restore::RestoreData>();
    int32_t activation_index_counter = 0;
    for (size_t i = 0; i < num_windows.size(); ++i) {
      const std::string app_id = base::NumberToString(i);

      // We need to add each `app_id` to app registry cache since our desk
      // template serialization requires an updated app cache to get the app
      // info.
      saved_desk_test_helper()->AddAppIdToAppRegistryCache(app_id);

      for (int32_t window_id = 0; window_id < num_windows[i]; ++window_id) {
        restore_data->AddAppLaunchInfo(
            std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id));

        app_restore::WindowInfo window_info;
        window_info.activation_index =
            std::make_optional<int32_t>(activation_index_counter++);

        restore_data->ModifyWindowInfo(app_id, window_id, window_info);
      }
    }
    return restore_data;
  }

  // Gets the current list of saved desk entries from the desk model directly
  // without updating the UI.
  const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>
  GetAllEntries() {
    auto result = desk_model()->GetAllEntries();
    EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk,
              result.status);
    return result.entries;
  }

  // Deletes an entry to the desks model directly without interacting with the
  // UI.
  void DeleteEntry(const base::Uuid& uuid) {
    base::RunLoop loop;
    desk_model()->DeleteEntry(
        uuid, base::BindLambdaForTesting(
                  [&](desks_storage::DeskModel::DeleteEntryStatus status) {
                    loop.Quit();
                  }));
    loop.Run();
  }

  // May return null in tablet mode.
  const OverviewDeskBarView* GetDesksBarViewForRoot(aura::Window* root_window) {
    if (auto* overview_session = GetOverviewSession()) {
      return overview_session->GetGridWithRootWindow(root_window)
          ->desks_bar_view();
    }
    return nullptr;
  }

  const DeskIconButton* GetLibraryButtonForRoot(aura::Window* root_window) {
    if (auto* desks_bar_view = GetDesksBarViewForRoot(root_window)) {
      return desks_bar_view->library_button();
    }
    return nullptr;
  }

  bool GetNewDeskButtonEnabledState(aura::Window* root_window) {
    auto* desks_bar_view = GetDesksBarViewForRoot(root_window);
    CHECK(desks_bar_view);
    return desks_bar_view->new_desk_button()->GetEnabled();
  }

  SavedDeskSaveDeskButton* GetSaveDeskAsTemplateButtonForRoot(
      aura::Window* root_window) {
    auto* overview_grid = GetOverviewGridForRoot(root_window);
    CHECK(overview_grid);
    return overview_grid->GetSaveDeskAsTemplateButton();
  }

  SavedDeskSaveDeskButton* GetSaveDeskForLaterButtonForRoot(
      aura::Window* root_window) {
    auto* overview_grid = GetOverviewGridForRoot(root_window);
    CHECK(overview_grid);
    return overview_grid->GetSaveDeskForLaterButton();
  }

  SavedDeskSaveDeskButtonContainer* GetSaveDeskButtonContainerForRoot(
      aura::Window* root_window) {
    auto* overview_grid = GetOverviewGridForRoot(root_window);
    CHECK(overview_grid);
    return overview_grid->GetSaveDeskButtonContainer();
  }

  SavedDeskRegularIconView* GetSavedDeskRegularIconView(
      SavedDeskIconView* icon_view) {
    DCHECK(!icon_view->IsOverflowIcon());
    return static_cast<SavedDeskRegularIconView*>(icon_view);
  }

  SavedDeskItemHoverState GetHoverState(const SavedDeskItemView* item_view) {
    return SavedDeskItemViewTestApi(item_view).GetHoverState();
  }

  // Shows the saved desk library by emulating a click on the library button. It
  // is required to have at least one entry in the desk model for the button to
  // be visible and clickable.
  void ShowSavedDeskLibrary() {
    auto* root_window = Shell::GetPrimaryRootWindow();
    auto* library_button = GetLibraryButtonForRoot(root_window);
    ASSERT_TRUE(library_button);
    ASSERT_TRUE(library_button->GetVisible());
    LeftClickOn(library_button);
  }

  // Helper function for attempting to delete a saved desk entry based on its
  // uuid. Also checks if the grid item count is as expected before deleting.
  // This function assumes we are already in overview mode and viewing the saved
  // desk grid.
  void DeleteSavedDeskItem(const base::Uuid uuid,
                           const size_t expected_current_item_count,
                           bool expect_saved_desk_item_exists = true) {
    const auto& grid_list = GetOverviewGridList();
    auto* saved_desk_library_view = grid_list[0]->GetSavedDeskLibraryView();
    ASSERT_TRUE(saved_desk_library_view);

    size_t total_item_count = 0;

    SavedDeskGridView* grid_view = nullptr;
    SavedDeskItemView* item_view = nullptr;
    for (SavedDeskGridView* grid : saved_desk_library_view->grid_views()) {
      for (SavedDeskItemView* item : grid->grid_items()) {
        if (SavedDeskItemViewTestApi(item).uuid() == uuid) {
          grid_view = grid;
          item_view = item;
        }
      }
      total_item_count += grid->grid_items().size();
    }

    // Check the current grid item count.
    ASSERT_EQ(expected_current_item_count, total_item_count);

    if (!expect_saved_desk_item_exists) {
      ASSERT_EQ(item_view, nullptr);
      return;
    }

    ASSERT_TRUE(item_view);
    ASSERT_TRUE(grid_view);

    LeftClickOn(SavedDeskItemViewTestApi(item_view).delete_button());

    // Clicking on the delete button should bring up the delete dialog.
    ASSERT_TRUE(Shell::IsSystemModalWindowOpen());

    // Click the delete button on the delete dialog. Show delete dialog and
    // select accept.
    const auto* dialog_accept_button = GetSavedDeskDialogAcceptButton();
    LeftClickOn(dialog_accept_button);

    // Wait for the dialog to close.
    base::RunLoop().RunUntilIdle();
    SavedDeskGridViewTestApi(grid_view).WaitForItemMoveAnimationDone();
    SavedDeskLibraryViewTestApi(saved_desk_library_view).WaitForAnimationDone();
    SavedDeskPresenterTestApi(
        GetOverviewGridList()[0]->overview_session()->saved_desk_presenter())
        .MaybeWaitForModel();
  }

  void WaitForSavedDeskLibrary() {
    SavedDeskLibraryView* library_view =
        GetOverviewGridList().front().get()->GetSavedDeskLibraryView();
    DCHECK(library_view);
    SavedDeskLibraryViewTestApi(library_view).WaitForAnimationDone();
  }

  // Open overview mode if we're not in overview mode yet, and then show the
  // saved desk grid.
  void OpenOverviewAndShowSavedDeskGrid() {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    ShowSavedDeskLibrary();
    WaitForSavedDeskLibrary();
  }

  void SpamLeftClickOn(const views::View* view) {
    DCHECK(view);

    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    DCHECK(view->GetVisible());
    for (size_t i = 0; i < 5; i++)
      event_generator->ClickLeftButton();
  }

  void LongPressAt(const gfx::Point& point) {
    ui::GestureEvent long_press(
        point.x(), point.y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureLongPress));
    GetEventGenerator()->Dispatch(&long_press);
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& GetOverviewGridList() {
    auto* overview_session = GetOverviewSession();
    DCHECK(overview_session);

    return overview_session->grid_list();
  }

  // Opens the context menu associated on the active desk. Then return the
  // `views::MenuItemView` associated with `command_id`. Expands the desk bar
  // from zero state if necessary.
  views::MenuItemView* GetActiveDeskActionContextMenuItem(
      aura::Window* root,
      DeskActionContextMenu::CommandId command_id) {
    return DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
        root, DeskBarViewBase::Type::kOverview,
        DesksController::Get()->GetActiveDeskIndex(), command_id);
  }

  // Opens overview mode and then clicks the save desk as template button. This
  // should save a new desk template and open the saved desk grid.
  void OpenOverviewAndSaveTemplate(aura::Window* root) {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    if (features::IsSavedDeskUiRevampEnabled()) {
      LeftClickOn(GetActiveDeskActionContextMenuItem(
          root, DeskActionContextMenu::kSaveAsTemplate));
    } else {
      ASSERT_TRUE(
          GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
      LeftClickOn(GetSaveDeskAsTemplateButtonForRoot(root));
    }

    WaitForSavedDeskUI();
    WaitForSavedDeskLibrary();
    // Clicking the save desk as template button selects the newly created saved
    // desk's name field. We can press enter or escape or click to select out of
    // it.
    PressAndReleaseKey(ui::VKEY_RETURN);
    for (auto& overview_grid : GetOverviewGridList())
      ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  }

  // Opens overview mode and then clicks the "save desk for later" button. This
  // should create a new saved desk and open the library page.
  void OpenOverviewAndSaveDeskForLater(aura::Window* root,
                                       bool observe_closing_windows = true) {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    if (features::IsSavedDeskUiRevampEnabled()) {
      LeftClickOn(GetActiveDeskActionContextMenuItem(
          root, DeskActionContextMenu::kSaveForLater));
    } else {
      ASSERT_TRUE(
          GetOverviewGridForRoot(root)->IsSaveDeskForLaterButtonVisible());
      LeftClickOn(GetSaveDeskForLaterButtonForRoot(root));
    }

    WaitForSavedDeskUI();

    // Wait for one more time only when we have closing windows.
    if (observe_closing_windows) {
      WaitForSavedDeskUI();
    }

    // Clicking the save desk button selects the newly saved desk's name
    // field. We can press enter or escape or click to select out of it.
    PressAndReleaseKey(ui::VKEY_RETURN);
    for (auto& overview_grid : GetOverviewGridList())
      ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  }

  SkBitmap GetBitmapWithInnerRoundedRect(gfx::Size size,
                                         int stroke_width,
                                         SkColor color) {
    gfx::Canvas canvas(size, /*image_scale=*/1.0f, /*is_opaque=*/false);
    gfx::RectF bounds(size.width(), size.height());

    cc::PaintFlags paint_flags;
    paint_flags.setAntiAlias(true);
    paint_flags.setColor(color);
    paint_flags.setStrokeWidth(stroke_width);
    paint_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
    bounds.Inset(gfx::InsetsF(stroke_width / 2.0f));
    canvas.DrawRoundRect(bounds, /*radius=*/bounds.height() / 2.0f,
                         paint_flags);
    return canvas.GetBitmap();
  }

  TestSavedDeskDelegate* GetSavedDeskDelegate() {
    return static_cast<TestSavedDeskDelegate*>(
        Shell::Get()->saved_desk_delegate());
  }

  MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesksTemplates,
         features::kDeskBarWindowOcclusionOptimization,
         chromeos::features::kOverviewSessionInitOptimizations},
        {});
    OverviewTestBase::SetUp();

    // The `FullRestoreSaveHandler` isn't setup during tests so every window we
    // create in tests doesn't have an app id associated with it. Since these
    // windows don't have app ids, `OverviewGrid` won't consider them supported
    // windows so we need to disable the app id check during tests.
    disable_app_id_check_ =
        OverviewController::Get()->SetDisableAppIdCheckForTests();

    // Wait for the desk model to have completed its initialization. Not doing
    // this would lead to flaky tests.
    saved_desk_test_helper()->WaitForDeskModels();
    account_id_test_ = AccountId::FromUserEmail("test_user");
    multi_user_window_manager_ =
        MultiUserWindowManager::Create(this, account_id_test_);
  }

  void TearDown() override {
    disable_app_id_check_.reset();
    multi_user_window_manager_.reset();
    OverviewTestBase::TearDown();
  }

  // MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override {}
  void OnTransitionUserShelfToNewAccount() override {}

 protected:
  std::optional<base::AutoReset<bool>> disable_app_id_check_;

  // Tests should normally create a local `ScopedAnimationDurationScaleMode`.
  // Create this object if a non zero scale mode needs to be used during test
  // tear down.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_scale_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<MultiUserWindowManager> multi_user_window_manager_;
  AccountId account_id_test_;
};

// Tests the helpers `AddEntry()` and `DeleteEntry()`, which will be used in
// different tests.
TEST_F(SavedDeskTest, AddDeleteEntry) {
  const base::Uuid expected_uuid = base::Uuid::GenerateRandomV4();
  const std::string expected_name = "desk name";
  base::Time expected_time = base::Time::Now();
  AddEntry(expected_uuid, expected_name, expected_time,
           DeskTemplateType::kTemplate);

  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1ul, entries.size());
  EXPECT_EQ(expected_uuid, entries[0]->uuid());
  EXPECT_EQ(base::UTF8ToUTF16(expected_name), entries[0]->template_name());
  EXPECT_EQ(expected_time, entries[0]->created_time());

  DeleteEntry(expected_uuid);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());
}

// Tests the library buttons visibility in clamshell mode.
TEST_F(SavedDeskTest, LibraryButtonsVisibilityClamshell) {
  // Helper function to verify which of the library buttons are currently shown.
  auto verify_button_visibilities =
      [this](bool zero_state_shown, bool expanded_state_shown,
             bool active_state_shown, const std::string& trace_string) {
        SCOPED_TRACE(trace_string);
        for (aura::Window* root_window : Shell::GetAllRootWindows()) {
          // There is just one library button with a state that is either zero,
          // expanded, or active.
          auto* desks_bar_view = GetDesksBarViewForRoot(root_window);
          ASSERT_TRUE(desks_bar_view);
          const DeskIconButton* library_button =
              desks_bar_view->library_button();
          EXPECT_EQ(zero_state_shown, library_button &&
                                          library_button->GetVisible() &&
                                          library_button->state() ==
                                              DeskIconButton::State::kZero);
          EXPECT_EQ(
              expanded_state_shown,
              IsLazyInitViewVisible(library_button) &&
                  library_button->state() == DeskIconButton::State::kExpanded);
          EXPECT_EQ(active_state_shown, library_button &&
                                            library_button->GetVisible() &&
                                            library_button->state() ==
                                                DeskIconButton::State::kActive);
        }
      };

  // The library button should appear on all root windows.
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  // There are no entries initially, so the none of the library buttons are
  // visible.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*active_state_shown=*/false,
                             /*trace_string=*/"one-desk-zero-entries");

  // Exit overview and add an entry.
  ToggleOverview();
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);

  // Reenter overview and verify the zero state library button is visible since
  // there is one entry to view.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/true,
                             /*expanded_state_shown=*/false,
                             /*active_state_shown=*/false,
                             /*trace_string=*/"one-desk-one-entry");

  // Click on the library button. It should expand the desks bar and the desk
  // bar should be active.
  LeftClickOn(GetLibraryButtonForRoot(Shell::GetPrimaryRootWindow()));
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*active_state_shown=*/true,
                             /*trace_string=*/"expand-from-zero-state");

  // Exit overview and create a new desk.
  ToggleOverview();
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Reenter overview and verify the expanded state library button is visible
  // since there is one entry to view.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/true,
                             /*active_state_shown=*/false,
                             /*trace_string=*/"two-desk-one-entry");

  // Exit overview and delete the entry.
  ToggleOverview();
  DeleteEntry(uuid);

  // Reenter overview and verify none of the buttons are shown.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*active_state_shown=*/false,
                             /*trace_string=*/"two-desk-zero-entries");
}

// Tests that the no windows widget is hidden when the saved desk grid is shown.
TEST_F(SavedDeskTest, NoWindowsLabelOnSavedDeskGridShow) {
  UpdateDisplay("400x300,400x300");

  // At least one entry is required for the saved desk grid to be shown.
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Start overview mode. The no windows widget should be visible.
  ToggleOverview();
  auto& grid_list = GetOverviewGridList();
  ASSERT_EQ(2u, grid_list.size());
  EXPECT_TRUE(grid_list[0]->no_windows_widget());
  EXPECT_TRUE(grid_list[1]->no_windows_widget());

  // Open the saved desk grid. The no windows widget should now be hidden.
  ShowSavedDeskLibrary();
  EXPECT_FALSE(grid_list[0]->no_windows_widget());
  EXPECT_FALSE(grid_list[1]->no_windows_widget());
}

// Tests that we stay in the saved desk library when deleting the last entry.
TEST_F(SavedDeskTest, NoItemsLabelOnDeletingLastSavedDesk) {
  UpdateDisplay("800x600,800x600");
  // Create a test window.
  auto test_window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1ul, desk_model()->GetEntryCount());
  // Exit overview mode.
  ToggleOverview();

  // Close the window and enter overview mode. The no windows widget should be
  // shown.
  test_window.reset();
  ToggleOverview();
  EXPECT_TRUE(GetOverviewGridList()[0]->no_windows_widget());

  // Open the saved desk grid. The no windows widget should now be hidden and
  // the no items label in the library UI should also not be visible.
  ShowSavedDeskLibrary();
  EXPECT_FALSE(GetOverviewGridList()[0]->no_windows_widget());
  EXPECT_FALSE(SavedDeskLibraryViewTestApi(
                   GetOverviewGridList()[0]->GetSavedDeskLibraryView())
                   .no_items_label()
                   ->GetVisible());

  // Delete the one and only template, which should hide the saved desk grid but
  // remain in overview. Check that the no windows widget is now back.
  OpenOverviewAndShowSavedDeskGrid();
  DeleteSavedDeskItem(entries[0]->uuid(), /*expected_current_item_count=*/1);

  ASSERT_TRUE(InOverviewSession());
  // We should be in the saved desk UI and the no items label should now be
  // visible.
  EXPECT_TRUE(SavedDeskLibraryViewTestApi(
                  GetOverviewGridList()[0]->GetSavedDeskLibraryView())
                  .no_items_label()
                  ->GetVisible());
}

// Tests when user enter saved desk, a11y alert being sent.
TEST_F(SavedDeskTest, InvokeAccessibilityAlertOnEnterDeskTemplates) {
  TestAccessibilityControllerClient client;

  // At least one entry is required for the saved desk grid to be shown.
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Start overview mode.
  ToggleOverview();

  // Alert for entering overview mode should be sent.
  EXPECT_EQ(AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());

  // Enter saved desk grid.
  ShowSavedDeskLibrary();

  // Alert for entering saved desk grid should be sent.
  EXPECT_EQ(AccessibilityAlert::SAVED_DESKS_MODE_ENTERED,
            client.last_a11y_alert());
}

// Tests that overview items are hidden when the saved desk grid is shown.
TEST_F(SavedDeskTest, HideOverviewItemsOnSavedDeskGridShow) {
  UpdateDisplay("800x600,800x600");

  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto test_window = CreateAppWindow();

  // Start overview mode. The window is visible in the overview mode.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_EQ(1.0f, test_window->layer()->opacity());

  // Open the saved desk grid. This should hide the window.
  ShowSavedDeskLibrary();
  EXPECT_EQ(0.0f, test_window->layer()->opacity());

  // Exit overview mode. The window is restored and visible again.
  ToggleOverview();
  EXPECT_EQ(1.0f, test_window->layer()->opacity());
}

// Verifies that we don't get a crash when: creating a window, minimizing it,
// entering the saved desk library and finally exiting overview. Regression test
// for http://b/260001863.
TEST_F(SavedDeskTest, HideMinimizedWindowOverviewItemsOnSavedDeskGridShow) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto test_window = CreateAppWindow();
  WindowState::Get(test_window.get())->Minimize();

  // Enter overview mode and the saved desk library. Entering the library will
  // hide the overview item.
  OpenOverviewAndShowSavedDeskGrid();

  // Exit overview mode. This needs to be done with a non-zero duration so that
  // the fade-out animation happens.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview();
}

// Tests that when the saved desk grid is shown and the active desk is closed,
// overview items stay hidden.
TEST_F(SavedDeskTest, OverviewItemsStayHiddenInSavedDeskGridOnDeskClose) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create a test window in the current desk.
  DesksController* desks_controller = DesksController::Get();
  auto test_window_1 = CreateAppWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(1ul, desks_controller->active_desk()->windows().size());

  // Create and activate a new desk, and create a test window there.
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk = desks_controller->desks().back().get();
  ActivateDesk(desk);
  auto test_window_2 = CreateAppWindow();
  // Check that the active desk is the second desk, and that it contains one
  // window.
  ASSERT_EQ(1, desks_controller->GetActiveDeskIndex());
  auto active_desk_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(1ul, active_desk_windows.size());
  auto all_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  EXPECT_EQ(2ul, all_windows.size());

  // Start overview mode. `test_window_2` should be visible in overview mode.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_EQ(1.0f, test_window_2->layer()->opacity());
  auto& overview_grid = GetOverviewGridList()[0];
  EXPECT_FALSE(overview_grid->GetOverviewItemContaining(test_window_1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(test_window_2.get()));

  // Open the saved desk grid. This should hide `test_window_2`.
  ShowSavedDeskLibrary();
  EXPECT_EQ(0.0f, test_window_2->layer()->opacity());

  // While in the saved desk grid, delete the active desk by clicking on the
  // combine desk option.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view =
      desks_bar_view->FindMiniViewForDesk(desks_controller->active_desk());
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        Shell::GetPrimaryRootWindow(),
        DeskActionContextMenu::CommandId::kCombineDesks));
  } else {
    LeftClickOn(mini_view->desk_action_view()->combine_desks_button());
  }

  // Expect we stay in the saved desk grid.
  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());

  // Expect that the active desk is now the first desk.
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Expect both windows are not visible.
  EXPECT_EQ(0.0f, test_window_1->layer()->opacity());
  EXPECT_EQ(0.0f, test_window_2->layer()->opacity());

  // Exit overview mode.
  ToggleOverview();
  // Expect both windows are visible.
  EXPECT_EQ(1.0f, test_window_1->layer()->opacity());
  EXPECT_EQ(1.0f, test_window_2->layer()->opacity());
}

// Tests the modality of the dialogs shown in saved desks.
TEST_F(SavedDeskTest, DialogSystemModal) {
  UpdateDisplay("800x600,800x600");

  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  // Show one of the dialogs. Activating the dialog keeps us in overview mode.
  auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
  dialog_controller->ShowReplaceDialog(Shell::GetPrimaryRootWindow(), u"Bento",
                                       DeskTemplateType::kTemplate,
                                       base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  ASSERT_TRUE(GetOverviewSession());

  // Fetch the widget for the dialog and test that it appears on the primary
  // root window.
  const views::Widget* dialog_widget = dialog_controller->dialog_widget();
  ASSERT_TRUE(dialog_widget);
  EXPECT_EQ(Shell::GetPrimaryRootWindow(),
            dialog_widget->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(dialog_widget->GetNativeWindow(), window_util::GetActiveWindow());

  // Hit escape to delete the dialog. Tests that there are no more system modal
  // windows open, and that we are still in overview because the dialog takes
  // the escape event, not the overview session.
  // TOOD(b/292156927): Use esc key to dismiss the dialog when this is fixed.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());
}

// Tests that the saved desk grid and item views are populated correctly.
TEST_F(SavedDeskTest, SavedDeskGridItems) {
  UpdateDisplay("800x600,800x600");

  const base::Uuid uuid_1 = base::Uuid::GenerateRandomV4();
  const std::string name_1 = "template_1";
  base::Time time_1 = base::Time::Now();
  AddEntry(uuid_1, name_1, time_1, DeskTemplateType::kTemplate);

  const base::Uuid uuid_2 = base::Uuid::GenerateRandomV4();
  const std::string name_2 = "template_2";
  base::Time time_2 = time_1 + base::Hours(13);
  AddEntry(uuid_2, name_2, time_2, DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  // Check that the grid is populated with the correct number of items, as
  // well as with the correct name and timestamp.
  for (const auto& overview_grid : GetOverviewGridList()) {
    std::vector<SavedDeskItemView*> grid_items =
        GetItemViewsFromDeskLibrary(overview_grid.get());

    ASSERT_EQ(2ul, grid_items.size());

    // The grid item order is currently not guaranteed, so need to
    // verify that each item exists by looking them up by their
    // UUID.
    auto verify_saved_desk_grid_item = [&grid_items](const base::Uuid& uuid,
                                                     const std::string& name) {
      auto iter =
          base::ranges::find(grid_items, uuid, [](const SavedDeskItemView* v) {
            return SavedDeskItemViewTestApi(v).uuid();
          });
      ASSERT_NE(grid_items.end(), iter);

      SavedDeskItemView* item_view = *iter;
      EXPECT_EQ(base::UTF8ToUTF16(name), item_view->name_view()->GetText());
      EXPECT_FALSE(
          SavedDeskItemViewTestApi(item_view).time_view()->GetText().empty());
    };

    verify_saved_desk_grid_item(uuid_1, name_1);
    verify_saved_desk_grid_item(uuid_2, name_2);
  }
}

// Tests that deleting templates in the saved desk grid functions correctly.
TEST_F(SavedDeskTest, DeleteTemplate) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::Uuid uuid_1 = base::Uuid::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  const base::Uuid uuid_2 = base::Uuid::GenerateRandomV4();
  AddEntry(uuid_2, "template_2", base::Time::Now() + base::Hours(13),
           DeskTemplateType::kTemplate);

  // This window should be hidden whenever the saved desk grid is open.
  auto test_window = CreateAppWindow();

  OpenOverviewAndShowSavedDeskGrid();

  // The window is hidden because the saved desk grid is open.
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(2ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_1`.
  DeleteSavedDeskItem(uuid_1, /*expected_current_item_count=*/2);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Verifies that the template with `uuid_1`, doesn't exist anymore.
  DeleteSavedDeskItem(uuid_1, /*expected_current_item_count=*/1,
                      /*expect_saved_desk_item_exists=*/false);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_2`.
  DeleteSavedDeskItem(uuid_2, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());
}

// Tests that the save desk button container is aligned with the first
// overview item. Regression test for https://crbug.com/1285491.
TEST_F(SavedDeskTest, SaveDeskButtonContainerAligned) {
  // The save desk button container is removed as part of the UI revamp.
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP();
  }

  // Create a test window in the current desk.
  auto test_window1 = CreateAppWindow();
  auto test_window2 = CreateAppWindow();
  // A widget is needed to close.
  auto test_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ToggleOverview();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root_window);
  SavedDeskSaveDeskButtonContainer* save_desk_button_container =
      GetSaveDeskButtonContainerForRoot(root_window);

  auto verify_save_desk_widget_bounds = [&overview_grid,
                                         save_desk_button_container]() {
    auto& window_list = overview_grid->item_list();
    ASSERT_FALSE(window_list.empty());
    EXPECT_EQ(std::round(window_list.front()->target_bounds().x()),
              save_desk_button_container->GetBoundsInScreen().x());
    EXPECT_EQ(std::round(window_list.front()->target_bounds().y()) - 45,
              save_desk_button_container->GetBoundsInScreen().y());
    EXPECT_EQ(16, save_desk_button_container->GetBetweenChildSpacing());
  };

  verify_save_desk_widget_bounds();

  // Tests that the save desk button remains slightly above the first overview
  // item after changes to the window position. Regression test for
  // https://crbug.com/1289020.

  // Delete an overview item and verify.
  OverviewItem* overview_item = static_cast<OverviewItem*>(
      GetOverviewItemForWindow(test_widget->GetNativeWindow()));
  overview_item->CloseWindow();

  // `NativeWidgetAura::Close()` fires a post task.
  base::RunLoop().RunUntilIdle();
  verify_save_desk_widget_bounds();

  // Create a new desk to leave zero state and verify.
  const DeskBarViewBase* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  auto* new_desk_button = desks_bar_view->new_desk_button();
  LeftClickOn(new_desk_button);
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  verify_save_desk_widget_bounds();
}

// Tests that the focus ring of the save desk button focus ring is as shown as
// expected.
TEST_F(SavedDeskTest, SaveDeskButtonFocusRing) {
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP()
        << "Save desk buttons have been moved to the desk context menu.";
  }

  // Create a test window in the current desk.
  auto test_window = CreateAppWindow();

  ToggleOverview();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* save_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root_window);
  auto* save_for_later_button = GetSaveDeskForLaterButtonForRoot(root_window);

  // Both buttons are not focused.
  ASSERT_FALSE(save_as_template_button->HasFocus());
  ASSERT_FALSE(save_for_later_button->HasFocus());

  // Reverse tab, then save desk for later button is focused.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(save_as_template_button->HasFocus());
  ASSERT_TRUE(save_for_later_button->HasFocus());

  // Reverse tab, then save desk as template button is focused.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(save_as_template_button->HasFocus());
  ASSERT_FALSE(save_for_later_button->HasFocus());

  // Reverse tab, then both buttons are not focused.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(save_as_template_button->HasFocus());
  ASSERT_FALSE(save_for_later_button->HasFocus());
}

// Tests that the save desk as template option and save for later option
// are enabled and disabled as expected based on the number of saved desk
// entries.
TEST_F(SavedDeskTest, SaveDeskOptionsEnabledDisabled) {
  // Prepare the test environment, like creating an app window which should be
  // supported.
  auto no_app_id_window = CreateAppWindow();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* delegate = Shell::Get()->saved_desk_delegate();
  ASSERT_TRUE(delegate->IsWindowSupportedForSavedDesk(no_app_id_window.get()));

  // Test `Save Desk as Template` option.
  {
    // Add 6 `kTemplate` entries.
    for (size_t i = 1; i <= 6; i++) {
      const std::string desk_name = "desk_template " + base::NumberToString(i);
      AddEntry(base::Uuid::GenerateRandomV4(), desk_name, base::Time::Now(),
               DeskTemplateType::kTemplate);
    }

    // Open overview and expect the option to be disabled.
    ToggleOverview();
    if (features::IsSavedDeskUiRevampEnabled()) {
      auto* template_item = GetActiveDeskActionContextMenuItem(
          root_window, DeskActionContextMenu::kSaveAsTemplate);
      ASSERT_TRUE(template_item);
      EXPECT_FALSE(template_item->GetEnabled());
    } else {
      EXPECT_EQ(views::Button::STATE_DISABLED,
                GetSaveDeskAsTemplateButtonForRoot(root_window)->GetState());
    }

    // Exit and reopen overview, then verify that the entry count reaches the
    // maximum.
    ToggleOverview();
    OpenOverviewAndShowSavedDeskGrid();
    const SavedDeskPresenter* saved_desk_presenter =
        saved_desk_util::GetSavedDeskPresenter();
    ASSERT_EQ(
        saved_desk_presenter->GetMaxEntryCount(DeskTemplateType::kTemplate),
        saved_desk_presenter->GetEntryCount(DeskTemplateType::kTemplate));

    // Verify that the option is re-enabled after we delete all entries.
    std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
        GetAllEntries();
    for (size_t i = entries.size(); i > 0; i--) {
      DeleteSavedDeskItem(/*uuid=*/entries[i - 1]->uuid(),
                          /*expected_current_item_count=*/i);
    }
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

    // Leave and re-enter overview.
    ToggleOverview();
    ToggleOverview();

    EXPECT_FALSE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

    if (features::IsSavedDeskUiRevampEnabled()) {
      auto* template_item = GetActiveDeskActionContextMenuItem(
          root_window, DeskActionContextMenu::kSaveAsTemplate);
      ASSERT_TRUE(template_item);
      EXPECT_TRUE(template_item->GetEnabled());
    } else {
      EXPECT_EQ(views::Button::STATE_NORMAL,
                GetSaveDeskAsTemplateButtonForRoot(root_window)->GetState());
    }

    // Exit overview.
    ToggleOverview();
  }

  // Test `Save Desk for Later` button.
  {
    // Add 6 `kSaveAndRecall` entries.
    for (size_t i = 1; i <= 6; i++) {
      const std::string desk_name = "saved_desk " + base::NumberToString(i);
      AddEntry(base::Uuid::GenerateRandomV4(), desk_name, base::Time::Now(),
               DeskTemplateType::kSaveAndRecall);
    }

    // Open overview and expect the button to be disabled.
    ToggleOverview();
    if (features::IsSavedDeskUiRevampEnabled()) {
      auto* save_later_item = GetActiveDeskActionContextMenuItem(
          root_window, DeskActionContextMenu::kSaveForLater);
      ASSERT_TRUE(save_later_item);
      EXPECT_FALSE(save_later_item->GetEnabled());
    } else {
      EXPECT_EQ(views::Button::STATE_DISABLED,
                GetSaveDeskForLaterButtonForRoot(root_window)->GetState());
    }

    // Exit and reopen overview, then verify that the entry count reaches the
    // maximum.
    ToggleOverview();
    OpenOverviewAndShowSavedDeskGrid();
    const SavedDeskPresenter* saved_desk_presenter =
        saved_desk_util::GetSavedDeskPresenter();
    ASSERT_EQ(
        saved_desk_presenter->GetMaxEntryCount(
            DeskTemplateType::kSaveAndRecall),
        saved_desk_presenter->GetEntryCount(DeskTemplateType::kSaveAndRecall));

    // Verify that the button is re-enabled after we delete all entries.
    std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
        GetAllEntries();
    for (size_t i = entries.size(); i > 0; i--) {
      DeleteSavedDeskItem(/*uuid=*/entries[i - 1]->uuid(),
                          /*expected_current_item_count=*/i);
    }
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

    // Leave and re-enter overview.
    ToggleOverview();
    ToggleOverview();

    EXPECT_FALSE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

    if (features::IsSavedDeskUiRevampEnabled()) {
      auto* save_later_item = GetActiveDeskActionContextMenuItem(
          root_window, DeskActionContextMenu::kSaveForLater);
      ASSERT_TRUE(save_later_item);
      EXPECT_TRUE(save_later_item->GetEnabled());
    } else {
      EXPECT_EQ(views::Button::STATE_NORMAL,
                GetSaveDeskForLaterButtonForRoot(root_window)->GetState());
    }

    // Exit overview.
    ToggleOverview();
  }
}

// Tests that clicking the save desk as template button shows the saved desk
// grid.
TEST_F(SavedDeskTest, SaveDeskAsTemplateButtonShowsSavedDeskGrid) {
  // There are no saved desk entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // The "Save desk as template" option is visible when at least one window is
  // open.
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        Shell::GetPrimaryRootWindow(), DeskActionContextMenu::kSaveAsTemplate));
  } else {
    SavedDeskSaveDeskButton* save_desk_as_template_button =
        GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(save_desk_as_template_button);
    EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                    ->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_desk_as_template_button);
  }

  ASSERT_EQ(1ul, GetAllEntries().size());
  WaitForSavedDeskUI();

  // Expect that the saved desk grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());
}

// Tests that the desks bar is created before the save desk buttons are visible.
// Regression test for https://crbug.com/1349971.
TEST_F(SavedDeskTest, DesksBarLoadsBeforeSaveDeskButtons) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Release the window since it will be automatically destroyed when the desk
  // is saved.
  auto* test_window = CreateAppWindow().release();
  ASSERT_FALSE(WindowState::Get(test_window)->IsMaximized());

  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  if (features::IsSavedDeskUiRevampEnabled()) {
    ToggleOverview();
    WaitForOverviewEnterAnimation();
  } else {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "SaveDeskButtonContainerWidget");
    EnterOverview();
    waiter.WaitIfNeededAndGet();
  }

  // Ensure we are in overview.
  auto* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Check to see that the desks bar has been created. Previously, there was a
  // crash caused by the save desk buttons being clicked before the desks bar
  // was created and initialized.
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_NE(desks_bar_view, nullptr);

  // Click on the "Save desk for later" option. We should transition into the
  // desk library and there should be no crash.
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root_window, DeskActionContextMenu::kSaveForLater));
  } else {
    LeftClickOn(GetSaveDeskForLaterButtonForRoot(root_window));
  }

  WaitForSavedDeskUI();
  WaitForSavedDeskUI();
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());
}

// Tests that saving a template nudges the correct name view.
TEST_F(SavedDeskTest, SaveTemplateNudgesNameView) {
  // Other templates were added earlier.
  AddEntry(base::Uuid::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateAppWindow();

  // Capture the current desk as a template, which is by default named "Desk 1".
  // We open overview and save template without clicking out of the newly
  // created template name view.
  ToggleOverview();
  auto* root = Shell::Get()->GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveAsTemplate));
  } else {
    LeftClickOn(GetSaveDeskAsTemplateButtonForRoot(root));
  }

  WaitForSavedDeskUI();
  ASSERT_EQ(3ul, GetAllEntries().size());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Expect that the last added template item name view has focus.
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_TRUE(name_view->HasSelection());
  EXPECT_EQ(u"Desk 1", name_view->GetText());
}

// Tests that launching templates from the templates grid functions correctly.
// We test both clicking on the card, as well as clicking on the "Use template"
// button that shows up on hover. Both should do the same thing.
TEST_F(SavedDeskTest, LaunchTemplate) {
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateAppWindow();

  // Capture the current desk and open the templates grid.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Click on the grid item to launch the template.
  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Launching a template creates and activates a new desk without exiting
  // overview mode, so we check that we're still in overview.
  EXPECT_TRUE(InOverviewSession());

  // This section tests clicking on the "Use template" button to launch the
  // template.
  ToggleOverview();
  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  LeftClickOn(SavedDeskItemViewTestApi(item_view).launch_button());

  EXPECT_EQ(3ul, desks_controller->desks().size());
  EXPECT_EQ(2, desks_controller->GetActiveDeskIndex());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that launching templates from the templates grid nudges the new desk
// name view.
TEST_F(SavedDeskTest, LaunchTemplateNudgesNewDeskName) {
  // Save an entry in the templates grid.
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(1ul, desks_controller->desks().size());

  // Click on the "Use template" button to launch the template.
  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  LeftClickOn(SavedDeskItemViewTestApi(item_view).launch_button());

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Launching a template creates and activates a new desk without exiting
  // overview mode, so we check that we're still in overview.
  EXPECT_TRUE(InOverviewSession());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  DeskNameView* desk_name_view =
      overview_grid->desks_bar_view()->mini_views().back()->desk_name_view();

  // Expect that the new desk name view has focus.
  EXPECT_TRUE(overview_grid->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());
  EXPECT_EQ(u"template", desk_name_view->GetText());
}

TEST_F(SavedDeskTest, AccessibleName) {
  // Save an entry in the templates grid.
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "save_and_recall_template",
           base::Time::Now(), DeskTemplateType::kSaveAndRecall);

  // Click on the "Use template" button to launch the template.
  OpenOverviewAndShowSavedDeskGrid();

  ui::AXNodeData data;
  GetItemViewFromSavedDeskGrid(0)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_LIBRARY_TEMPLATES_GRID_ITEM_ACCESSIBLE_NAME,
          u"template"));

  data = ui::AXNodeData();
  GetItemViewFromSavedDeskGrid(1)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(
      data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVE_AND_RECALL_GRID_ITEM_ACCESSIBLE_NAME,
          u"save_and_recall_template"));
}

// Tests that the order of SavedDeskItemView is in order.
TEST_F(SavedDeskTest, IconsOrder) {
  // Create a `DeskTemplate` using which has 5 apps and each app has 1 window.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>(5, 1)));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The items previews should be ordered by activation index. Exclude the
  // final SavedDeskIconView since it will be the overflow counter.
  EXPECT_EQ(6u, icon_views.size());
  int previous_id;
  for (size_t i = 0; i < icon_views.size() - 1; ++i) {
    int current_id;
    ASSERT_TRUE(base::StringToInt(
        GetSavedDeskRegularIconView(icon_views[i])->icon_identifier().url_or_id,
        &current_id));

    if (i)
      EXPECT_TRUE(current_id > previous_id);

    previous_id = current_id;
  }
}

// Tests that both regular and lacros browsers have an icon for each unique tab.
TEST_F(SavedDeskTest, NumIconsForBrowser) {
  // Create fake restore data with one chrome and one lacros browser. Each
  // browser has two unique tabs.
  const std::string kAppId1 = app_constants::kChromeAppId;
  constexpr int kWindowId1 = 1;
  const std::vector<GURL> kTabs1{GURL("http://a.com"), GURL("http://b.com")};

  const std::string kAppId2 = app_constants::kLacrosAppId;
  constexpr int kWindowId2 = 2;
  const std::vector<GURL> kTabs2{GURL("http://c.com"), GURL("http://d.com")};

  auto restore_data = std::make_unique<app_restore::RestoreData>();

  // Add app launch info for the chrome browser instance.
  auto app_launch_info_1 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId1, kWindowId1);
  app_launch_info_1->browser_extra_info.active_tab_index = 1;
  app_launch_info_1->browser_extra_info.urls = kTabs1;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));

  // Add app launch info for the lacros browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->browser_extra_info.active_tab_index = 1;
  app_launch_info_2->browser_extra_info.urls = kTabs2;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));

  // A non empty activation index is assumed by the icon placing logic.
  app_restore::WindowInfo window_info;
  window_info.activation_index = 0;
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info);

  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowSavedDeskGrid();

  // Test that there is a total of 4 icons, one for each tab on each browser.
  // There is also the overflow icon, which is created but hidden.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();
  EXPECT_EQ(5u, icon_views.size());
}

// Tests that icons are ordered such that active tabs and windows are ordered
// before inactive tabs.
TEST_F(SavedDeskTest, IconsOrderWithInactiveTabs) {
  const std::string kAppId1 = app_constants::kChromeAppId;
  constexpr int kWindowId1 = 1;
  constexpr int kActiveTabIndex1 = 1;
  const std::vector<GURL> kTabs1{GURL("http://a.com"), GURL("http://b.com"),
                                 GURL("http://c.com")};

  const std::string kAppId2 = app_constants::kChromeAppId;
  constexpr int kWindowId2 = 2;
  constexpr int kActiveTabIndex2 = 2;
  const std::vector<GURL> kTabs2{GURL("http://d.com"), GURL("http://e.com"),
                                 GURL("http://f.com")};

  // Create `restore_data` for the template.
  auto restore_data = std::make_unique<app_restore::RestoreData>();

  // Add app launch info for the first browser instance.
  auto app_launch_info_1 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId1, kWindowId1);
  app_launch_info_1->browser_extra_info.active_tab_index = kActiveTabIndex1;
  app_launch_info_1->browser_extra_info.urls = kTabs1;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));
  app_restore::WindowInfo window_info_1;
  window_info_1.activation_index = std::make_optional<int32_t>(kWindowId1);
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info_1);

  // Add app launch info for the second browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->browser_extra_info.active_tab_index = kActiveTabIndex2;
  app_launch_info_2->browser_extra_info.urls = kTabs2;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));
  app_restore::WindowInfo window_info_2;
  window_info_2.activation_index = std::make_optional<int32_t>(kWindowId2);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info_2);

  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // Check the icon views. The first two items should be the active tabs,
  // ordered by activation index. The next two items should be the inactive tabs
  // with the lowest activation indices, i.e. the rest of the tabs from the
  // first browser instance.
  ASSERT_EQ(7u, icon_views.size());
  EXPECT_EQ(
      kTabs1[kActiveTabIndex1].spec(),
      GetSavedDeskRegularIconView(icon_views[0])->icon_identifier().url_or_id);
  EXPECT_EQ(
      kTabs2[kActiveTabIndex2].spec(),
      GetSavedDeskRegularIconView(icon_views[1])->icon_identifier().url_or_id);
  EXPECT_EQ(
      kTabs1[0].spec(),
      GetSavedDeskRegularIconView(icon_views[2])->icon_identifier().url_or_id);
  EXPECT_EQ(
      kTabs1[2].spec(),
      GetSavedDeskRegularIconView(icon_views[3])->icon_identifier().url_or_id);
}

// Tests that when two tabs are put into a desk template that have the same
// domain but different query parameters, only one icon shows up in the template
// to represent both tabs.
TEST_F(SavedDeskTest, IdenticalURL) {
  const std::string kAppId = app_constants::kChromeAppId;
  constexpr int kWindowId = 1;
  constexpr int kActiveTabIndex = 1;
  const std::vector<GURL> kTabs{GURL("http://google.com/?query=a"),
                                GURL("http://google.com/?query=b")};

  // Create `restore_data` for the template.
  auto restore_data = std::make_unique<app_restore::RestoreData>();

  // Add app launch info.
  auto app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId, kWindowId);
  app_launch_info->browser_extra_info.active_tab_index = kActiveTabIndex;
  app_launch_info->browser_extra_info.urls = kTabs;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info));
  app_restore::WindowInfo window_info;
  window_info.activation_index = std::make_optional<int32_t>(kWindowId);
  restore_data->ModifyWindowInfo(kAppId, kWindowId, window_info);

  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should be one icon view for both the urls, and another icon view for
  // the overflow icon.
  ASSERT_EQ(2u, icon_views.size());
  // The first icon view should have the first url including the query parameter
  // as its identifier, and have a count of 2 because its representing both
  // urls.
  EXPECT_EQ(
      kTabs[0].spec(),
      GetSavedDeskRegularIconView(icon_views[0])->icon_identifier().url_or_id);
  EXPECT_EQ(2, icon_views[0]->GetCount());
  // The second icon view should have a count of 0, because there are no
  // overflow windows.
  EXPECT_EQ(0, icon_views[1]->GetCount());
}

// Tests that the overflow count view is visible, in bounds, displays the right
// count when there is more than `SavedDeskIconContainer::kMaxIcons` icons.
TEST_F(SavedDeskTest, OverflowIconView) {
  // Create a `DeskTemplate` using which has 1 app more than the max and each
  // app has 1 window.
  const int kNumOverflowApps = 1;
  std::vector<int> window_info(
      kNumOverflowApps + SavedDeskIconContainer::kMaxIcons, 1);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should be `kMaxIcons` + 1 visible icons.
  int num_of_visibile_icon_views = 0;
  for (auto* icon_view : icon_views) {
    if (icon_view->GetVisible())
      num_of_visibile_icon_views++;
  }
  EXPECT_EQ(SavedDeskIconContainer::kMaxIcons + 1, num_of_visibile_icon_views);

  // The overflow counter should have no identifier and its count should be
  // non-zero. It should also be visible and within the bounds of the host
  // SavedDeskItemView.
  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+1", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->GetVisible());
  EXPECT_TRUE(item_view->Contains(overflow_icon_view.saved_desk_icon_view()));
}

// Tests that when there isn't enough space to display
// `SavedDeskIconContainer::kMaxIcons` icons and the overflow
// icon view, the overflow icon view is visible and its count incremented by the
// number of icons that had to be hidden.
TEST_F(SavedDeskTest, OverflowIconViewIncrementsForHiddenIcons) {
  // Create a `DeskTemplate` using which has 3 apps more than
  // `SavedDeskIconContainer::kMaxIcons` and each app has 2 windows.
  // With each app having 2 windows, only 2 app icon views and the overflow view
  // will be able to fit in the container, the rest will overflow.
  const int kNumOverflowApps = 3;
  std::vector<int> window_info(
      kNumOverflowApps + SavedDeskIconContainer::kMaxIcons, 2);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // Only 3 visible icons can fit, e.g. 2 icons with `+1` label, and the
  // overflow icon.
  int num_of_visibile_icon_views = 0;
  for (auto* icon_view : icon_views) {
    if (icon_view->GetVisible())
      num_of_visibile_icon_views++;
  }
  EXPECT_GE(3, num_of_visibile_icon_views);

  // Count the number of hidden icon views and also check that there's a
  // contiguous block of visible icon views, followed by a contiguous block of
  // invisible icon views, followed by the overflow icon view.
  size_t num_hidden = 0;
  bool started_hiding_icon_views = false;
  for (size_t i = 0; i < icon_views.size(); ++i) {
    const bool is_visible = icon_views[i]->GetVisible();

    if (!is_visible) {
      ++num_hidden;
      started_hiding_icon_views = true;
    }

    EXPECT_TRUE((is_visible && !started_hiding_icon_views) ||
                (!is_visible && started_hiding_icon_views) ||
                (is_visible && i == icon_views.size() - 1));
  }

  // There should be at least one hidden view.
  EXPECT_LT(0u, num_hidden);

  // The overflow counter should have no identifier and its count should be
  // non-zero, accounting for the number of windows that are not represented by
  // app icons. It should also be visible and within the bounds of the host
  // SavedDeskItemView.
  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());

  // (3 + 4) * 2 = 14 windows were added to the desk template, from 7 apps with
  // 2 windows each. The first 4 icon views are created, each with a "+1" count
  // label. The last 2 of the 4 views did not fit, so they were made not visible
  // and were added to the overflow count. With 2 of the 7 apps being displayed,
  // having 2 windows each, the overflow count should be 14 - (2 * 2) = 10.
  EXPECT_EQ(u"+10", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->GetVisible());
  EXPECT_TRUE(item_view->Contains(overflow_icon_view.saved_desk_icon_view()));
}

// Tests that apps with multiple window are counted correctly.
//  ______________________________________________________
//  |  ________   ________   ________________   ________ |
//  | |       |  |       |  |       |       |  |       | |
//  | |   I   |  |   I   |  |   I      + 1  |  |  + 5  | |
//  | |_______|  |_______|  |_______|_______|  |_______| |
//  |____________________________________________________|
//
TEST_F(SavedDeskTest, IconViewMultipleWindows) {
  // Create a `DeskTemplate` that contains some apps with multiple windows and
  // more than kMaxIcons windows. The grid should appear like the above diagram.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>{1, 1, 2, 2, 3}));

  // Enter overview and show the Desks Templates Grid.
  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should be 2 icon views with count 1, 2 icon views with count 2, 1
  // icon views with count 3, and 1 overflow icon view.
  EXPECT_EQ(6u, icon_views.size());

  // Verify each of the apps' count labels are correct.
  SavedDeskIconViewTestApi icon_view_1(icon_views[0]);
  EXPECT_TRUE(icon_view_1.saved_desk_icon_view()->GetVisible());
  EXPECT_FALSE(icon_view_1.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_FALSE(icon_view_1.count_label());

  SavedDeskIconViewTestApi icon_view_2(icon_views[1]);
  EXPECT_TRUE(icon_view_2.saved_desk_icon_view()->GetVisible());
  EXPECT_FALSE(icon_view_2.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_FALSE(icon_view_2.count_label());

  SavedDeskIconViewTestApi icon_view_3(icon_views[2]);
  EXPECT_TRUE(icon_view_3.saved_desk_icon_view()->GetVisible());
  EXPECT_FALSE(icon_view_3.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(icon_view_3.count_label());
  EXPECT_EQ(u"+1", icon_view_3.count_label()->GetText());

  SavedDeskIconViewTestApi icon_view_4(icon_views[3]);
  EXPECT_FALSE(icon_view_4.saved_desk_icon_view()->GetVisible());
  EXPECT_FALSE(icon_view_4.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(icon_view_4.count_label());
  EXPECT_EQ(u"+1", icon_view_4.count_label()->GetText());

  SavedDeskIconViewTestApi icon_view_5(icon_views[4]);
  EXPECT_FALSE(icon_view_5.saved_desk_icon_view()->GetVisible());
  EXPECT_FALSE(icon_view_5.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(icon_view_5.count_label());
  EXPECT_EQ(u"+2", icon_view_5.count_label()->GetText());

  // The overflow counter should display the number of excess windows.
  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->GetVisible());
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+5", overflow_icon_view.count_label()->GetText());
}

// Tests that when an app has more than 99 windows, its label is changed to
// "+99".
TEST_F(SavedDeskTest, IconViewMoreThan99Windows) {
  // Create a `DeskTemplate` using which has 1 app with 101 windows.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>{101}));

  // Enter overview and show the saved desk Grid.
  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should only be 1 icon view for the app and 1 icon view for the
  // overflow.
  EXPECT_EQ(2u, icon_views.size());

  // The app's icon view should have a "+99" label.
  SavedDeskIconViewTestApi icon_view(icon_views[0]);
  EXPECT_FALSE(icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(icon_view.count_label());
  EXPECT_EQ(u"+99", icon_view.count_label()->GetText());

  // The overflow counter should not be visible.
  EXPECT_FALSE(icon_views.back()->GetVisible());
}

// Tests that when there are less than `SavedDeskIconContainer::kMaxIcons`
// the overflow icon is not visible.
TEST_F(SavedDeskTest, OverflowIconViewHiddenOnNoOverflow) {
  // Create a `DeskTemplate` using which has
  // `SavedDeskIconContainer::kMaxIcons` apps and each app has 1 window.
  std::vector<int> window_info(SavedDeskIconContainer::kMaxIcons, 1);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // All the icon views should be visible and the overflow icon view should be
  // invisible.
  for (size_t i = 0; i < icon_views.size() - 1; ++i)
    EXPECT_TRUE(icon_views[i]->GetVisible());
  EXPECT_FALSE(icon_views.back()->GetVisible());
}

// Test that the overflow icon counts unavailable icons when there are less than
// kMaxIcons visible in the container.
TEST_F(SavedDeskTest, OverflowUnavailableLessThan5Icons) {
  // Create a `DeskTemplate` which has 4 apps and each app has 1 window. Set 2
  // of those app ids to be unavailable.
  std::vector<int> window_info(4, 1);
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // `CreateRestoreData` creates the windows with app ids of "0", "1", "2", etc.
  // Set 2 of those app ids to be unavailable.
  GetSavedDeskDelegate()->set_unavailable_apps({"0", "1"});

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The 2 available app icons should be visible, and the overflow icon should
  // contain the hidden (0) + unavailable (2) app counts.
  EXPECT_EQ(3u, icon_views.size());

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+2", overflow_icon_view.count_label()->GetText());
}

// Test that the overflow icon counts unavailable icons when there are more than
// kMaxIcons visible in the container, and hidden icons are also added.
TEST_F(SavedDeskTest, OverflowUnavailableMoreThan5Icons) {
  // Create a `DeskTemplate` which has 8 apps and each app has 1 window. Set 2
  // of those app ids to be unavailable.
  std::vector<int> window_info(8, 1);
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // `CreateRestoreData` creates the windows with app ids of "0", "1", "2", etc.
  // Set 2 of those app ids to be unavailable.
  GetSavedDeskDelegate()->set_unavailable_apps({"0", "1"});

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The 4 available app icons should be visible, and the overflow icon should
  // contain the hidden (2) + unavailable (2) app counts.
  int num_of_visibile_icon_views = 0;
  for (auto* icon_view : icon_views) {
    if (icon_view->GetVisible())
      num_of_visibile_icon_views++;
  }
  EXPECT_EQ(SavedDeskIconContainer::kMaxIcons + 1, num_of_visibile_icon_views);

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+4", overflow_icon_view.count_label()->GetText());
}

// Test that the overflow icon displays the count without a plus when all icons
// are unavailable.
TEST_F(SavedDeskTest, OverflowUnavailableAllUnavailableIcons) {
  // Create a `DeskTemplate` which has 10 apps and each app has 1 window.
  std::vector<int> window_info(10, 1);
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // Set all 10 app ids to be unavailable.
  GetSavedDeskDelegate()->set_unavailable_apps(
      {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});

  OpenOverviewAndShowSavedDeskGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The only added icon view is the overflow icon, and it should have a "10"
  // label without the plus sign.
  EXPECT_EQ(1u, icon_views.size());

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_TRUE(overflow_icon_view.saved_desk_icon_view()->IsOverflowIcon());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"10", overflow_icon_view.count_label()->GetText());
}

// Tests that the desks templates and save desk button container are hidden when
// entering overview in tablet mode.
TEST_F(SavedDeskTest, EnteringInTabletMode) {
  // Create a desk before entering tablet mode, otherwise the desks bar will not
  // show up.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up in clamshell mode either.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  EnterTabletMode();

  // Test that the templates buttons are created but invisible. The save desk as
  // template button is not created.
  ToggleOverview();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* library_button = GetLibraryButtonForRoot(root);
  EXPECT_FALSE(IsLazyInitViewVisible(library_button));
  EXPECT_FALSE(GetSaveDeskButtonContainerForRoot(root));
}

// Tests that the library buttons and save desk buttons are hidden when
// transitioning from clamshell to tablet mode.
TEST_F(SavedDeskTest, ClamshellToTabletModeOld) {
  base::test::ScopedFeatureList disable;
  disable.InitAndDisableFeature(features::kSavedDeskUiRevamp);

  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Test that on entering overview, the zero state desks templates button and
  // the save template button are visible.
  ToggleOverview();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* desks_bar_view = GetDesksBarViewForRoot(root);
  ASSERT_TRUE(desks_bar_view);
  auto* library_button = GetLibraryButtonForRoot(root);
  ASSERT_TRUE(library_button);
  EXPECT_TRUE(library_button->GetVisible());
  EXPECT_EQ(DeskIconButton::State::kZero, library_button->state());
  EXPECT_TRUE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());

  // Tests that after transitioning, we remain in overview mode and all the
  // buttons are invisible.
  EnterTabletMode();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(library_button->GetVisible());
  EXPECT_FALSE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
}

// Tests that the library button and save desk options in the desk context menu
// are hidden when transitioning from clamshell to tablet mode.
TEST_F(SavedDeskTest, ClamshellToTabletMode) {
  base::test::ScopedFeatureList enable{features::kSavedDeskUiRevamp};

  // Add one desk so we start overview with expanded desk bar, which is needed
  // to open the context menu.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Create a window and add a test entry. Otherwise the library button wouldn't
  // show up.
  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Tests that on entering overview, the library button is visible.
  ToggleOverview();
  auto* library_button = GetLibraryButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(library_button);
  EXPECT_TRUE(library_button->GetVisible());

  // Tests that after opening the context menu, there is a save desk as template
  // option.
  DeskActionContextMenu* clamshell_context_menu =
      DesksTestApi::GetContextMenuForDesk(DeskBarViewBase::Type::kOverview,
                                          /*index=*/0);
  EXPECT_TRUE(DesksTestApi::GetDeskActionContextMenuItem(
      clamshell_context_menu,
      DeskActionContextMenu::CommandId::kSaveAsTemplate));

  // Tests that after transitioning, we remain in overview mode and the library
  // button is invisible and the context menu has closed.
  EnterTabletMode();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(library_button->GetVisible());
  EXPECT_FALSE(DesksTestApi::IsContextMenuRunningForDesk(
      DeskBarViewBase::Type::kOverview, /*index=*/0));

  // Tests that after reopening the context menu, there is no save desk as
  // template option.
  DeskActionContextMenu* tablet_context_menu =
      DesksTestApi::GetContextMenuForDesk(DeskBarViewBase::Type::kOverview,
                                          /*index=*/0);
  EXPECT_FALSE(DesksTestApi::GetDeskActionContextMenuItem(
      tablet_context_menu, DeskActionContextMenu::CommandId::kSaveAsTemplate));
}

// Tests that the saved desk library gets hidden when transitioning to tablet
// mode.
TEST_F(SavedDeskTest, ShowingSavedDeskLibraryToTabletMode) {
  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ASSERT_TRUE(GetOverviewSession()
                  ->GetGridWithRootWindow(root_window)
                  ->saved_desk_library_widget()
                  ->IsVisible());

  // Tests that the templates button is in expanded state when the grid is
  // showing, even with one desk.
  auto* desks_bar_view = GetDesksBarViewForRoot(root_window);
  ASSERT_TRUE(desks_bar_view);
  auto* library_button = GetLibraryButtonForRoot(root_window);
  ASSERT_TRUE(library_button);
  EXPECT_TRUE(library_button->GetVisible());
  EXPECT_EQ(DeskIconButton::State::kActive, library_button->state());

  // Tests that after transitioning, we remain in overview mode and the grid is
  // hidden.
  EnterTabletMode();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(GetOverviewSession()
                   ->GetGridWithRootWindow(root_window)
                   ->saved_desk_library_widget()
                   ->IsVisible());

  // Tests that the library button is also hidden in tablet mode. Regression
  // test for https://crbug.com/1291777.
  EXPECT_FALSE(library_button->GetVisible());
}

// In certain cases there are activation issues when we enter tablet mode,
// causing us to exit overview mode. This tests that if we save a template (and
// get dropped into the templates grid), and then enter tablet mode, we remain
// in overview mode. Regression test for https://crbug.com/1277769.
TEST_F(SavedDeskTest, TabletModeActivationIssues) {
  // Create a test window.
  auto test_window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  // Tests that after transitioning into tablet mode, the activation and focus
  // is correct and we remain in overview mode.
  EnterTabletMode();
  ASSERT_TRUE(InOverviewSession());
}

TEST_F(SavedDeskTest, OverviewTabbing) {
  const base::Time saved_desk_creation_time =
      base::Time::FromSecondsSinceUnixEpoch(10);

  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template1",
           saved_desk_creation_time, DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template2",
           saved_desk_creation_time, DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* first_item = GetItemViewFromSavedDeskGrid(0);
  SavedDeskItemView* second_item = GetItemViewFromSavedDeskGrid(1);

  // Testing that we first traverse the views of the first item.
  EXPECT_EQ(first_item, GetFocusedView());

  // Testing that we traverse to the `name_view` of the first item.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(first_item->name_view(), GetFocusedView());

  // When we're done with the first item, we'll go on to the second.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(second_item, GetFocusedView());

  // Testing that we traverse to the `name_view` of the second item.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(second_item->name_view(), GetFocusedView());

  // Traversing name views should not update the template if there was no
  // editing.
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(saved_desk_creation_time, entries[0]->GetLastUpdatedTime());
  EXPECT_EQ(saved_desk_creation_time, entries[1]->GetLastUpdatedTime());
}

// Tests that if the templates button is invisible, it is not part of the
// tabbing order. Regression test for https://crbug.com/1313761.
TEST_F(SavedDeskTest, TabbingInvisibleTemplatesButton) {
  // First test the case there are no templates.
  ToggleOverview();

  auto* button = GetLibraryButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_FALSE(IsLazyInitViewVisible(button));

  // Test that we do not focus the templates button.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_NE(button, GetFocusedView());

  // Test the case where it was visible at one point, but became invisible (last
  // template was deleted).
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());

  // Add an entry to delete later.
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);
  OpenOverviewAndShowSavedDeskGrid();

  DeleteSavedDeskItem(uuid, /*expected_current_item_count=*/1);
  // `NativeWidgetAura::Close()`, which is the underlying class for a dialog
  // widget is on a post task so flush that task.
  base::RunLoop().RunUntilIdle();

  // The library button should be visible and active state.
  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(desks_bar_view);
  auto* library_button = desks_bar_view->library_button();
  ASSERT_TRUE(library_button);
  EXPECT_TRUE(library_button->GetVisible());
  EXPECT_EQ(library_button->state(), DeskIconButton::State::kActive);
}

// Tests that the desks bar does not return to zero state if the second-to-last
// desk is deleted while viewing the saved desk grid.
TEST_F(SavedDeskTest, DesksBarDoesNotReturnToZeroState) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root_window);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Close one of the desks. Test that we remain in expanded state.
  auto* mini_view = desks_bar_view->FindMiniViewForDesk(
      DesksController::Get()->active_desk());
  LeftClickOn(mini_view->desk_action_view()->close_all_button());
  // The new desk button and the library button should be active state.
  EXPECT_EQ(DeskIconButton::State::kExpanded,
            desks_bar_view->new_desk_button()->state());
  // `OpenOverviewAndShowSavedDeskGrid` clicks on the library button, so it
  // should be active.
  ASSERT_TRUE(desks_bar_view->library_button());
  EXPECT_EQ(DeskIconButton::State::kActive,
            desks_bar_view->library_button()->state());

  // Delete the one and only template, we should remain in the desk library.
  DeleteSavedDeskItem(uuid, /*expected_current_item_count=*/1);
  EXPECT_TRUE(GetOverviewSession()
                  ->GetGridWithRootWindow(root_window)
                  ->saved_desk_library_widget()
                  ->IsVisible());

  // Test that the new desk button is expanded state.
  EXPECT_EQ(DeskIconButton::State::kExpanded,
            desks_bar_view->new_desk_button()->state());
  EXPECT_FALSE(desks_bar_view->IsZeroState());
}

// Tests that the unsupported apps dialog is shown when a user attempts to save
// an active desk with unsupported apps.
TEST_F(SavedDeskTest, UnsupportedAppsDialog) {
  // Create a crostini window.
  auto crostini_window = CreateAppWindow();
  crostini_window->SetProperty(chromeos::kAppTypeKey,
                               chromeos::AppType::CROSTINI_APP);

  // Create a normal window.
  auto test_window = CreateAppWindow();

  auto* root = Shell::Get()->GetPrimaryRootWindow();
  ToggleOverview();
  auto* save_desk_as_template_button = GetSaveDeskAsTemplateButtonForRoot(root);
  if (features::IsSavedDeskUiRevampEnabled()) {
    // Open overview and then the desk context menu and then click on the save
    // template menu item. The unsupported apps dialog should show up.
    auto* save_desk_as_template_menu_item = GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveAsTemplate);
    LeftClickOn(save_desk_as_template_menu_item);
  } else {
    // Open overview and click on the save template button. The unsupported apps
    // dialog should show up.
    ASSERT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_desk_as_template_button);
  }
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Decline the dialog. We should stay in overview and no template should have
  // been saved.
  LeftClickOn(saved_desk_util::GetSavedDeskDialogController()
                  ->GetSystemDialogViewForTesting()
                  ->GetCancelButtonForTesting());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());

  if (features::IsSavedDeskUiRevampEnabled()) {
    // Click on the save template menu item again. The unsupported apps dialog
    // should
    // show up.
    auto* save_desk_as_template_menu_item = GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveAsTemplate);
    LeftClickOn(save_desk_as_template_menu_item);
  } else {
    // Click on the save template button again. The unsupported apps dialog
    // should
    // show up.
    LeftClickOn(save_desk_as_template_button);
  }
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Accept the dialog. The template should have been saved and the saved desk
  // grid should now be shown.
  LeftClickOn(GetSavedDeskDialogAcceptButton());
  WaitForSavedDeskUI();
  EXPECT_TRUE(GetOverviewSession());
  EXPECT_TRUE(GetOverviewGridList()[0]->saved_desk_library_widget());

  ASSERT_EQ(1ul, GetAllEntries().size());
}

// Tests that the save desk as template button and save for later button are
// disabled when all windows on the desk are unsupported or there are no windows
// with Full Restore app ids. See crbug.com/1277763.
TEST_F(SavedDeskTest, AllUnsupportedAppsDisablesSaveDeskButtons) {
  // Need expanded bar to open context menu.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  disable_app_id_check_.reset();

  // Use `CreateTestWindow()` instead of `CreateAppWindow()`, which by default
  // creates a supported window.
  auto test_window = CreateTestWindow();

  // Also create an app window which should not have an app id, making it
  // "unsupported".
  auto no_app_id_window = CreateAppWindow();
  auto* delegate = Shell::Get()->saved_desk_delegate();
  ASSERT_TRUE(delegate->IsWindowSupportedForSavedDesk(no_app_id_window.get()));
  ASSERT_TRUE(saved_desk_util::GetAppId(no_app_id_window.get()).empty());

  // Open overview.
  ToggleOverview();

  auto* root = Shell::Get()->GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    DeskActionContextMenu* menu = DesksTestApi::GetContextMenuForDesk(
        DeskBarViewBase::Type::kOverview, /*index=*/0);
    auto* template_item = DesksTestApi::GetDeskActionContextMenuItem(
        menu, DeskActionContextMenu::kSaveAsTemplate);
    auto* save_later_item = DesksTestApi::GetDeskActionContextMenuItem(
        menu, DeskActionContextMenu::kSaveForLater);
    ASSERT_TRUE(template_item);
    ASSERT_TRUE(save_later_item);
    EXPECT_FALSE(template_item->GetEnabled());
    EXPECT_FALSE(save_later_item->GetEnabled());
    return;
  }

  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(2, GetOverviewGridList()[0]->num_unsupported_windows());
  EXPECT_EQ(views::Button::STATE_DISABLED,
            GetSaveDeskAsTemplateButtonForRoot(root)->GetState());
  EXPECT_EQ(views::Button::STATE_DISABLED,
            GetSaveDeskForLaterButtonForRoot(root)->GetState());
}

// Tests that adding and removing unsupported windows is counted correctly.
TEST_F(SavedDeskTest, AddRemoveUnsupportedWindows) {
  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(2, GetOverviewGridList()[0]->num_unsupported_windows());

  window1.reset();

  // Expect `num_unsupported_windows_` to be 0.
  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(1, GetOverviewGridList()[0]->num_unsupported_windows());

  window2.reset();

  // Re-open overview because all the windows closing caused it to close too.
  ToggleOverview();
  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(0, GetOverviewGridList()[0]->num_unsupported_windows());
}

// Tests the mouse and touch hover behavior on the saved desk item view.
TEST_F(SavedDeskTest, HoverOnTemplateItemView) {
  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* first_item = GetItemViewFromSavedDeskGrid(0);
  SavedDeskItemView* second_item = GetItemViewFromSavedDeskGrid(1);
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kIcons);

  // Move the mouse to hover over `first_item`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kHover);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kIcons);

  // Move the mouse to hover over `second_item`.
  event_generator->MoveMouseTo(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kHover);

  // Long press on the `first_item`.
  LongPressAt(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kHover);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kIcons);
  // Long press on the `second_item`.
  LongPressAt(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kHover);

  // Move the mouse to hover over `first_item` again.
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kHover);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kIcons);

  // Long press on the `second_item`.
  LongPressAt(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kHover);

  // Move the mouse but make it still remain on top of `first_item`.
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kHover);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kIcons);

  // Test to make sure hover is updated after dragging to another item.
  event_generator->DragMouseTo(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(GetHoverState(first_item), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(second_item), SavedDeskItemHoverState::kHover);
}

// Tests that when a supported app doesn't have any app launch info and a
// template is saved, the unsupported apps dialog isn't shown. See
// crbug.com/1269466.
TEST_F(SavedDeskTest, DialogDoesntShowForSupportedAppsWithoutLaunchInfo) {
  constexpr int kInvalidWindowKey = -10000;

  // Create a normal window.
  auto test_window = CreateAppWindow();

  // Set its `app_restore::kWindowIdKey` to an untracked window id. This
  // simulates a supported window not having a corresponding app launch info.
  test_window->SetProperty(app_restore::kWindowIdKey, kInvalidWindowKey);

  // Open overview and click on the save desk as template button. The
  // unsupported apps dialog should not show up and an entry should be saved.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  ASSERT_EQ(1ul, GetAllEntries().size());
}

// Tests that if there is a window minimized in overview, we don't crash when
// launching a template. Regression test for https://crbug.com/1271337.
TEST_F(SavedDeskTest, LaunchTemplateWithMinimizedOverviewWindow) {
  // Create a test minimized window.
  auto window = CreateAppWindow();
  WindowState::Get(window.get())->Minimize();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Click on the grid item to launch the template. We should remain in overview
  // and there should be no crash.
  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  EXPECT_TRUE(InOverviewSession());
}

// Tests that there is no crash if we launch a template after deleting the
// active desk. Regression test for https://crbug.com/1277203.
TEST_F(SavedDeskTest, LaunchTemplateAfterClosingActiveDesk) {
  auto* desks_controller = DesksController::Get();
  while (desks_controller->CanCreateDesks())
    desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // One window is needed to save a template.
  auto window = CreateAppWindow();

  // Open overview and save a template. This will also take us to the desks
  // templates grid view.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Remove the active desk. This caused a crash prior because the "Save desk as
  // a template" button was not moved when the active desk was removed.
  RemoveDesk(desks_controller->active_desk());

  // Click on the grid item to launch the template. There should be no crash.
  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  EXPECT_TRUE(InOverviewSession());
}

// Tests that the desks templates are organized in alphabetical order.
TEST_F(SavedDeskTest, ShowTemplatesInAlphabeticalOrder) {
  // Create a window and add three test entry in different names.
  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "1_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "a_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "b_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  ASSERT_EQ(5ul, grid_items.size());

  // Tests that templates are sorted in alphabetical order.
  EXPECT_EQ(u"Template, 1_template",
            grid_items[0]->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Template, a_template",
            grid_items[1]->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Template, A_template",
            grid_items[2]->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Template, b_template",
            grid_items[3]->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Template, B_template",
            grid_items[4]->GetViewAccessibility().GetCachedName());
}

// Tests that the color of the library button focus ring is as expected.
// Regression test for https://crbug.com/1265003.
TEST_F(SavedDeskTest, DesksTemplatesButtonFocusColor) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  AddEntry(base::Uuid::GenerateRandomV4(), "name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  const ui::ColorId focused_color_id = ui::kColorAshFocusRing;

  ToggleOverview();

  auto* desks_bar_view = GetDesksBarViewForRoot(Shell::GetPrimaryRootWindow());
  const ui::ColorId active_color_id = cros_tokens::kCrosSysTertiary;

  const DeskIconButton* button = desks_bar_view->library_button();
  ASSERT_TRUE(button);
  ASSERT_TRUE(button->GetVisible());

  // The library button starts of neither focused nor active.
  EXPECT_EQ(DeskIconButton::State::kExpanded, button->state());
  EXPECT_FALSE(button->GetFocusColorIdForTesting());

  // Tests that when we are viewing the saved desk grid, the button border is
  // active.
  LeftClickOn(button);
  EXPECT_EQ(DeskIconButton::State::kActive, button->state());
  EXPECT_EQ(active_color_id, *button->GetFocusColorIdForTesting());

  // Tests that when focused, the library button border has a focused color.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(DeskIconButton::State::kActive, button->state());
  EXPECT_EQ(focused_color_id, *button->GetFocusColorIdForTesting());

  // Shift focus away from the library button. The button border should be
  // active.
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(DeskIconButton::State::kActive, button->state());
  EXPECT_EQ(active_color_id, *button->GetFocusColorIdForTesting());
}

// Tests that if we save a template (and get dropped into the templates grid),
// delete all the templates (and the templates grid gets hidden), the windows in
// overview get activated and restored when selected.
TEST_F(SavedDeskTest, WindowActivatableAfterSaveAndDeleteTemplate) {
  // Create a test window.
  auto test_window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  // Delete the one and only template, which should hide the saved desk grid but
  // remain in overview.
  DeleteSavedDeskItem(entries[0]->uuid(), /*expected_current_item_count=*/1);
  ASSERT_TRUE(InOverviewSession());

  // Click on the `test_window` to activate it.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(test_window.get());
  event_generator->ClickLeftButton();

  // Verify that we exit the overview session.
  EXPECT_FALSE(InOverviewSession());

  // Check that the window is active.
  EXPECT_EQ(test_window.get(), window_util::GetActiveWindow());
}

// Tests that we can open the saved desk library if the current active window
// prior to entering overview is one that activates on occlusion change.
// Regression test for http://b/307963349.
TEST_F(SavedDeskTest, ShowLibraryWithWindowActivatingOnOcclusion) {
  // Need at least one entry to show the saved desk library.
  AddEntry(base::Uuid::GenerateRandomV4(), "template name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create a test window that has similar properties to a browser window with
  // the print preview showing. Its occlusion state is tracked, and when
  // occlusion state changes, the window gets activated.
  auto window = CreateToplevelTestWindow(gfx::Rect(400, 300));
  window->TrackOcclusionState();
  window->Show();

  auto* window_delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());
  window_delegate->set_on_occlusion_changed(
      base::BindLambdaForTesting([&]() { wm::ActivateWindow(window.get()); }));

  // Entering overview also pauses the occlusion tracker. Wait a bit for it to
  // unpause other occlusion won't try to be recomputed when showing the
  // library.
  ToggleOverview();
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(200));
  loop.Run();

  // Show the saved desk library. We should stay in overview, and there should
  // be no u-a-f.
  ShowSavedDeskLibrary();
  WaitForSavedDeskLibrary();
  EXPECT_TRUE(InOverviewSession());
}

TEST_F(SavedDeskTest, TemplateNameBounds) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(0);
  SavedDeskNameView* name_view = item_view->name_view();
  const views::Label* time_view =
      SavedDeskItemViewTestApi(item_view).time_view();

  // Tests that the contents of the two views are the aligned. We use contents
  // bounds here since the name view has a background which is larger than the
  // text.
  gfx::Point name_contents_in_screen = name_view->GetContentsBounds().origin();
  views::View::ConvertPointToScreen(name_view, &name_contents_in_screen);
  gfx::Point time_contents_in_screen = time_view->GetContentsBounds().origin();
  views::View::ConvertPointToScreen(time_view, &time_contents_in_screen);
  EXPECT_EQ(name_contents_in_screen.x(), time_contents_in_screen.x());

  // Add a long name which will cause `name_view` to reach its max width. Test
  // that the distance from `item_view` is the same on both sides.
  LeftClickOn(name_view);
  for (int i = 0; i < 200; ++i)
    PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSavedDeskUI();
  EXPECT_EQ(
      name_view->GetBoundsInScreen().x() - item_view->GetBoundsInScreen().x(),
      item_view->GetBoundsInScreen().right() -
          name_view->GetBoundsInScreen().right());
}

TEST_F(SavedDeskTest, AccessibleProperties) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(0);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVED_DESK_GRID_ITEM_EXTRA_ACCESSIBLE_DESCRIPTION),
      item_view->GetViewAccessibility().GetCachedDescription());
}

// Tests that we are able to edit the saved desk name.
TEST_F(SavedDeskTest, EditSavedDeskName) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::Uuid::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Test that we can add characters to the name and press enter to save it.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_B);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSavedDeskUI();
  name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(template_name) + u"ab", name_view->GetText());

  // Deleting characters and pressing enter saves the name.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_BACK);
  PressAndReleaseKey(ui::VKEY_BACK);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSavedDeskUI();
  name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // The `name_view` defaults to select all, so typing a letter while all
  // selected replaces the text. Also, clicking anywhere outside of the text
  // field will try to save it.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_A);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  event_generator->ClickLeftButton();
  EXPECT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  WaitForSavedDeskUI();
  name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_EQ(u"a", name_view->GetText());

  // Test that clicking on the grid item (outside of the textfield) will save
  // it.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_B);
  LeftClickOn(GetItemViewFromSavedDeskGrid(0));
  WaitForSavedDeskUI();
  name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_EQ(u"ab", name_view->GetText());

  // Pressing TAB also saves the name.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_TAB);
  WaitForSavedDeskUI();
  name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_EQ(u"abc", name_view->GetText());

  // There was a bug where a relayout could cause a revert of the name changes,
  // and lead to a crash if the name view had focus. This is a regression test
  // for that. See https://crbug.com/1285113 for more details.
  GetItemViewFromSavedDeskGrid(0)->SetBoundsRect(gfx::Rect(150, 40));
  EXPECT_EQ(u"abc", GetItemViewFromSavedDeskGrid(0)->name_view()->GetText());
}

// Tests for checking that certain conditions will revert the saved desk name to
// its original name, even if the text in the textfield has been updated.
TEST_F(SavedDeskTest, SavedDeskNameChangeAborted) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::Uuid::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Pressing enter with no changes to the text.
  LeftClickOn(name_view);
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_TRUE(name_view->HasSelection());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(name_view->HasFocus());
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // Pressing the escape key will revert the changes made to the name in the
  // textfield.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_B);
  PressAndReleaseKey(ui::VKEY_C);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // Empty text fields will also revert back to the original name.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_BACK);
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
}

// Tests to verify that clicking the spacebar doesn't cause the name view to
// lose focus (since it's within a button), and that whitespaces are handled
// correctly.
TEST_F(SavedDeskTest, TemplateNameTestSpaces) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::Uuid::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Pressing spacebar does not cause `name_view` to lose focus.
  LeftClickOn(name_view);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_EQ(base::UTF8ToUTF16(template_name) + u" ", name_view->GetText());

  // Extra whitespace should be trimmed.
  PressAndReleaseKey(ui::VKEY_HOME);
  PressAndReleaseKey(ui::VKEY_SPACE);
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(u"  " + base::UTF8ToUTF16(template_name) + u" ",
            name_view->GetText());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // A string consisting of just spaces is considered an empty string, and the
  // name change is reverted.
  EXPECT_FALSE(name_view->HasFocus());
  LeftClickOn(name_view);
  EXPECT_TRUE(name_view->HasFocus());
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(u" ", name_view->GetText());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
}

// Tests that there is no crash after we use the keyboard to change the name of
// a template. Regression test for https://crbug.com/1279649.
TEST_F(SavedDeskTest, EditTemplateNameWithKeyboardNoCrash) {
  AddEntry(base::Uuid::GenerateRandomV4(), "a", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "b", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Tab until we focus the name view of the first saved desk item.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(name_view, GetFocusedView());

  // Rename template "a" to template "d".
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSavedDeskUI();

  // Verify that there is no crash after we tab again.
  PressAndReleaseKey(ui::VKEY_TAB);
}

// Tests that there is no crash when leaving the saved desk name view focused
// with a changed name during shutdown. Regression test for
// https://crbug.com/1281422.
TEST_F(SavedDeskTest, EditSavedDeskNameShutdownNoCrash) {
  // The fade out animation of the saved desk grid must be enabled for this
  // crash to have happened.
  animation_scale_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AddEntry(base::Uuid::GenerateRandomV4(), "a", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "b", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();

  // Tab until we focus the name view of the first saved desk item.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(name_view, GetFocusedView());

  // Rename template "a" to template "ddd".
  PressAndReleaseKey(ui::VKEY_RETURN);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_D);

  // Verify that there is no crash while the test tears down.
}

// Tests that the hovering over the templates name shows the expected cursor.
TEST_F(SavedDeskTest, TemplatesNameHitTest) {
  auto* cursor_manager = Shell::Get()->cursor_manager();

  for (bool is_rtl : {true, false}) {
    SCOPED_TRACE(is_rtl ? "rtl" : "ltr");
    base::i18n::SetRTLForTesting(is_rtl);

    AddEntry(base::Uuid::GenerateRandomV4(), "a", base::Time::Now(),
             DeskTemplateType::kTemplate);

    OpenOverviewAndShowSavedDeskGrid();
    SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
    const gfx::Rect name_view_bounds = name_view->GetBoundsInScreen();
    // Hover to a point just inside main edge. This will cover the case where
    // the hit test logic is inverted.
    const gfx::Point hover_point =
        is_rtl ? name_view_bounds.right_center() + gfx::Vector2d(-2, 0)
               : name_view_bounds.left_center() + gfx::Vector2d(2, 0);

    // Tests that the hover cursor is an IBeam.
    GetEventGenerator()->MoveMouseTo(hover_point);
    EXPECT_EQ(ui::mojom::CursorType::kIBeam,
              cursor_manager->GetCursor().type());

    // Exit overview for the next run.
    ToggleOverview();
  }
}

// Tests that it can unfocus the desk name view and saved desk name view from
// active status on clicking library button when we stay in saved desk grid.
TEST_F(SavedDeskTest, UnFocusNameChangeOnClickingLibrary) {
  // Save an entry in the templates grid.
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();
  // Expect we stay in the saved desk grid.
  auto* overview_grid = GetOverviewSession()->GetGridWithRootWindow(
      Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());

  DeskNameView* desk_name_view =
      overview_grid->desks_bar_view()->mini_views().back()->desk_name_view();
  // Tests if the desk name view rename can work correctly.
  // Click the new desk name view which will be focused.
  LeftClickOn(desk_name_view);
  EXPECT_TRUE(overview_grid->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());

  // Click Library button again to unfocus the desk name view when we stay in
  // the saved desk grid.
  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  ShowSavedDeskLibrary();
  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Tests if the saved desk name view rename can work correctly.
  // Click the desk name view at first, and then click the saved desk name view,
  // and finally click the Library button.
  LeftClickOn(desk_name_view);
  EXPECT_TRUE(overview_grid->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());

  SavedDeskNameView* saved_name_view =
      GetItemViewFromSavedDeskGrid(0)->name_view();
  LeftClickOn(saved_name_view);
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(saved_name_view->HasFocus());
  EXPECT_TRUE(saved_name_view->HasSelection());

  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  ShowSavedDeskLibrary();

  // Check if the desk name view and the saved desk name view are all unfocused.
  ASSERT_TRUE(overview_grid->IsShowingSavedDeskLibrary());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_FALSE(saved_name_view->HasFocus());
}

// Tests that accessibility overrides are set as expected.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorInOverview) {
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP()
        << "Save desk buttons have been moved to the desk context menu.";
  }

  auto window = CreateAppWindow(gfx::Rect(100, 100));

  ToggleOverview();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  ASSERT_TRUE(focus_widget);

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  auto* desk_widget = const_cast<views::Widget*>(grid->desks_widget());
  ASSERT_TRUE(desk_widget);

  auto* save_widget = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                          ->save_desk_button_container_widget();
  auto* item_widget = GetOverviewItemForWindow(window.get())->item_widget();

  // Order should be [focus_widget, item_widget, desk_widget, save_widget].
  CheckA11yOverrides("focus", focus_widget, save_widget, item_widget);
  CheckA11yOverrides("item", item_widget, focus_widget, desk_widget);
  CheckA11yOverrides("desk", desk_widget, item_widget, save_widget);
  CheckA11yOverrides("save", save_widget, desk_widget, focus_widget);
}

// Tests that accessibility overrides are set as expected after entering
// library view.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorInLibrary) {
  auto window = CreateAppWindow(gfx::Rect(100, 100));

  AddEntry(base::Uuid::GenerateRandomV4(), "test_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  ASSERT_TRUE(focus_widget);

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* desk_widget =
      const_cast<views::Widget*>(overview_grid->desks_widget());
  ASSERT_TRUE(desk_widget);
  views::Widget* library_widget = overview_grid->saved_desk_library_widget();
  ASSERT_TRUE(library_widget);

  // Order should be [focus_widget, library_widget, desk_widget].
  CheckA11yOverrides("focus", focus_widget, desk_widget, library_widget);
  CheckA11yOverrides("template", library_widget, focus_widget, desk_widget);
  CheckA11yOverrides("desk", desk_widget, library_widget, focus_widget);
}

// Tests that accessibility overrides are set as expected after entering
// templates view when no window opens.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorWhenNoWindowOpen) {
  AddEntry(base::Uuid::GenerateRandomV4(), "test_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  ASSERT_TRUE(focus_widget);

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* desk_widget =
      const_cast<views::Widget*>(overview_grid->desks_widget());
  ASSERT_TRUE(desk_widget);
  views::Widget* library_widget = overview_grid->saved_desk_library_widget();
  ASSERT_TRUE(library_widget);

  // Order should be [focus_widget, template_widget, desk_widget].
  CheckA11yOverrides("focus", focus_widget, desk_widget, library_widget);
  CheckA11yOverrides("template", library_widget, focus_widget, desk_widget);
  CheckA11yOverrides("desk", desk_widget, library_widget, focus_widget);
}

// Tests that the children of the overview grid matches the order they are
// displayed so accessibility traverses it correctly.
TEST_F(SavedDeskTest, AccessibilityGridItemTraversalOrder) {
  auto window = CreateTestWindow(gfx::Rect(100, 100));

  AddEntry(base::Uuid::GenerateRandomV4(), "template_4", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_3", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskLibraryView* library_view = overview_grid->GetSavedDeskLibraryView();
  ASSERT_FALSE(library_view->grid_views().empty());

  SavedDeskGridView* grid_view = library_view->grid_views().front();
  ASSERT_TRUE(grid_view);

  // The grid items are sorted and displayed alphabetically.
  std::vector<raw_ptr<SavedDeskItemView, VectorExperimental>> grid_items =
      grid_view->grid_items();
  views::View::Views grid_child_views = grid_view->children();

  // Verifies the order of the children matches what is displayed in the grid.
  for (size_t i = 0; i < grid_items.size(); i++)
    ASSERT_EQ(grid_items[i], grid_child_views[i]);
}

TEST_F(SavedDeskTest, LayoutItemsInLandscape) {
  UpdateDisplay("800x600");

  // Create a window and add four test entries.
  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "C_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "D_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  ASSERT_EQ(4ul, grid_items.size());

  // We expect the first three items to be laid out in one row.
  EXPECT_EQ(grid_items[0]->bounds().y(), grid_items[1]->bounds().y());
  EXPECT_EQ(grid_items[0]->bounds().y(), grid_items[2]->bounds().y());
  // The fourth item goes in the second row.
  EXPECT_NE(grid_items[0]->bounds().y(), grid_items[3]->bounds().y());
}

TEST_F(SavedDeskTest, LayoutItemsInPortrait) {
  UpdateDisplay("600x800");

  // Create a window and add four test entries.
  auto test_window = CreateAppWindow();
  AddEntry(base::Uuid::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "C_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "D_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  ASSERT_EQ(4ul, grid_items.size());

  // We expect the first two items to be laid out in one row.
  EXPECT_EQ(grid_items[0]->bounds().y(), grid_items[1]->bounds().y());
  // And the last two items on the next row.
  EXPECT_NE(grid_items[0]->bounds().y(), grid_items[2]->bounds().y());
  EXPECT_EQ(grid_items[2]->bounds().y(), grid_items[3]->bounds().y());
}

// Tests that there is no overlap with the shelf on our smallest supported
// resolution.
TEST_F(SavedDeskTest, ItemsDoNotOverlapShelf) {
  // The smallest display resolution we support is 1087x675.
  UpdateDisplay("1000x600");

  // Create 6 entries to max out the grid.
  for (const std::string& name : {"A", "B", "C", "D", "E", "F"}) {
    AddEntry(base::Uuid::GenerateRandomV4(), name, base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  OpenOverviewAndShowSavedDeskGrid();

  SavedDeskLibraryView* library_view =
      GetOverviewGridList().front()->GetSavedDeskLibraryView();

  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();

  // Test that none of the grid items overlap with the shelf.
  SavedDeskLibraryViewTestApi test_api(library_view);
  for (views::View* view : test_api.scroll_view()->contents()->children())
    EXPECT_FALSE(view->GetBoundsInScreen().Intersects(shelf_bounds));
}

// Tests that showing the overview records to the saved desk grid histogram.
TEST_F(SavedDeskTest, RecordDesksTemplateGridShowMetric) {
  // Make sure that LoadTemplateHistogram is recorded.
  base::HistogramTester histogram_tester;

  // Entry needed so that overview is accessible
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  // Assert load grid histogram recorded.
  constexpr int kExpectedGridShows = 1;
  histogram_tester.ExpectTotalCount(kLoadTemplateGridHistogramName,
                                    kExpectedGridShows);
}

// Tests that deleting templates in the saved desk grid Records to the delete
// template histogram.
TEST_F(SavedDeskTest, DeleteTemplateRecordsMetric) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::Uuid uuid_1 = base::Uuid::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // This window should be hidden whenever the saved desk grid is open.
  auto test_window = CreateTestWindow();

  // This action should record deletions and grid shows in a UMA histogram.
  base::HistogramTester histogram_tester;

  OpenOverviewAndShowSavedDeskGrid();

  // The window is hidden because the desk templates grid is open.
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the saved desk item with `uuid_1`.
  DeleteSavedDeskItem(uuid_1, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());

  // Verifies that the saved desk item with `uuid_1`, doesn't exist anymore.
  DeleteSavedDeskItem(uuid_1, /*expected_current_item_count=*/0,
                      /*expect_saved_desk_item_exists=*/false);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());

  // Assert that histogram metrics were recorded.
  const int expected_deletes = 1;
  histogram_tester.ExpectTotalCount(kDeleteTemplateHistogramName,
                                    expected_deletes);
}

// Tests that Launches are recorded to the appropriate histogram.
TEST_F(SavedDeskTest, LaunchTemplateRecordsMetric) {
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateAppWindow();

  // Log histogram recording
  base::HistogramTester histogram_tester;

  // Capture the current desk and open the templates grid.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Click on the grid item to launch the template.
  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Assert load grid histogram and launch histogram recorded.
  constexpr int kExpectedLaunches = 1;
  histogram_tester.ExpectTotalCount(kLaunchTemplateHistogramName,
                                    kExpectedLaunches);

  histogram_tester.ExpectBucketCount(
      kNewDeskHistogramName,
      static_cast<int>(DesksCreationRemovalSource::kLaunchTemplate),
      kExpectedLaunches);
  histogram_tester.ExpectBucketCount(
      kDeskSwitchHistogramName,
      static_cast<int>(DesksSwitchSource::kLaunchTemplate), kExpectedLaunches);
}

// Tests that clicking the save desk as template button records to the
// new template histogram.
TEST_F(SavedDeskTest, SaveDeskAsTemplateRecordsMetric) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // Record histogram.
  base::HistogramTester histogram_tester;

  // The "Save desk as template" option is visible when at least one window is
  // open.
  OpenOverviewAndSaveTemplate(Shell::GetPrimaryRootWindow());

  ASSERT_EQ(1ul, GetAllEntries().size());

  // Expect that the saved desk grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

  // Assert that there was a new template event recorded to the proper
  // histogram.
  constexpr int kExpectedNewTemplates = 1;
  histogram_tester.ExpectTotalCount(kNewTemplateHistogramName,
                                    kExpectedNewTemplates);
  histogram_tester.ExpectBucketCount(
      kAddOrUpdateTemplateStatusHistogramName,
      static_cast<int>(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk),
      kExpectedNewTemplates);
}

// Tests that UnsupportedAppDialogShow metric is recorded when the unsupported
// app dialog is shown.
TEST_F(SavedDeskTest, UnsupportedAppDialogRecordsMetric) {
  // For asserting histogram was captured.
  base::HistogramTester histogram_tester;

  // Create a crostini window.
  auto crostini_window = CreateAppWindow();
  crostini_window->SetProperty(chromeos::kAppTypeKey,
                               chromeos::AppType::CROSTINI_APP);

  // Create a normal window.
  auto test_window = CreateAppWindow();

  auto* root = Shell::Get()->GetPrimaryRootWindow();
  ToggleOverview();
  if (features::IsSavedDeskUiRevampEnabled()) {
    // Open overview and then the desk context menu and then click on the save
    // template menu item. The unsupported apps dialog should show up.
    auto* save_desk_as_template_menu_item = GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveAsTemplate);
    LeftClickOn(save_desk_as_template_menu_item);
  } else {
    // Open overview and click on the save desk as template button. The
    // unsupported apps dialog should show up.
    SavedDeskSaveDeskButton* save_template_button =
        GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_template_button);
  }
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Now we assert that we've recorded the metric.
  constexpr int kExpectedDialogShows = 1;
  histogram_tester.ExpectTotalCount(
      kTemplateUnsupportedAppDialogShowHistogramName, kExpectedDialogShows);
}

// Tests that the window and tab counts are properly recorded in their
// resepctive metrics.
TEST_F(SavedDeskTest, SaveDeskRecordsWindowAndTabCountMetrics) {
  const std::string kAppId1 = app_constants::kChromeAppId;
  constexpr int kWindowId1 = 1;
  constexpr int kActiveTabIndex1 = 1;
  const std::vector<GURL> kTabs1{GURL("http://a.com"), GURL("http://b.com"),
                                 GURL("http://c.com")};

  const std::string kAppId2 = app_constants::kChromeAppId;
  constexpr int kWindowId2 = 2;
  constexpr int kActiveTabIndex2 = 2;
  const std::vector<GURL> kTabs2{GURL("http://d.com"), GURL("http://e.com"),
                                 GURL("http://f.com")};

  // Create `restore_data` for the template.
  auto restore_data = std::make_unique<app_restore::RestoreData>();

  // Add app launch info for the first browser instance.
  auto app_launch_info_1 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId1, kWindowId1);
  app_launch_info_1->browser_extra_info.active_tab_index = kActiveTabIndex1;
  app_launch_info_1->browser_extra_info.urls = kTabs1;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));
  app_restore::WindowInfo window_info_1;
  window_info_1.activation_index = std::make_optional<int32_t>(kWindowId1);
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info_1);

  // Add app launch info for the second browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->browser_extra_info.active_tab_index = kActiveTabIndex2;
  app_launch_info_2->browser_extra_info.urls = kTabs2;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));
  app_restore::WindowInfo window_info_2;
  window_info_2.activation_index = std::make_optional<int32_t>(kWindowId2);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info_2);

  auto desk_template = std::make_unique<DeskTemplate>(
      base::Uuid::GenerateRandomV4(), DeskTemplateSource::kUser, "template_1",
      base::Time::Now(), DeskTemplateType::kTemplate);
  desk_template->set_desk_restore_data(std::move(restore_data));

  // Record histogram.
  base::HistogramTester histogram_tester;

  ToggleOverview();

  // Mocks saving templates with some browsers.
  saved_desk_util::GetSavedDeskPresenter()->SaveOrUpdateSavedDesk(
      /*is_update=*/false, Shell::GetPrimaryRootWindow(),
      std::move(desk_template));

  histogram_tester.ExpectBucketCount(kTemplateWindowCountHistogramName, 2, 1);
  histogram_tester.ExpectBucketCount(kTemplateTabCountHistogramName, 6, 1);
  histogram_tester.ExpectBucketCount(kTemplateWindowAndTabCountHistogramName, 6,
                                     1);
}

// Tests that the user template count metric is recorded correctly.
TEST_F(SavedDeskTest, UserTemplateCountRecordsMetricCorrectly) {
  // Record histogram.
  base::HistogramTester histogram_tester;

  // Create three new templates through the UI.
  for (unsigned long num_templates = 0; num_templates < 3; ++num_templates) {
    // There are no saved template entries and one test window initially.
    auto test_window = CreateAppWindow();

    ASSERT_FALSE(GetOverviewSession());

    // Enter overview and save the active desk as a template.
    OpenOverviewAndSaveTemplate(Shell::GetPrimaryRootWindow());
    ASSERT_EQ(num_templates + 1, GetAllEntries().size());

    // Expect that the saved desk grid is visible.
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());

    // Exit overview.
    ToggleOverview();
  }

  OpenOverviewAndShowSavedDeskGrid();

  // Delete one of the templates which will iterate the histogram's second
  // bucket.
  DeleteSavedDeskItem(GetAllEntries()[0]->uuid(),
                      /*expected_current_item_count=*/3);

  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 2, 2);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 3, 1);
}

// Test that things don't crash when exiting overview immediately after
// triggering the replace dialog. Regression test for http://b/258306298.
TEST_F(SavedDeskTest, ReplaceTemplateAndExitOverview) {
  UpdateDisplay("800x600");

  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(1)->name_view();
  // Ensure that we have the right item.
  EXPECT_EQ(name_view->GetText(), u"template_2");

  LeftClickOn(name_view);
  EXPECT_TRUE(name_view->HasFocus());

  // Change the name of "template_2" to "template_1", which will trigger the
  // replace dialog to be shown.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  PressAndReleaseKey(ui::VKEY_BACK);
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Immediately exit overview. It is important that this is done with a
  // non-zero duration. This will cause saved desk UI items to live on for
  // slightly longer as they will be briefly owned by an animation.
  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview();
  WaitForOverviewExitAnimation();
}

// Tests record metrics when current template being replaced.
TEST_F(SavedDeskTest, ReplaceTemplateMetric) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600,800x600");

  const base::Uuid uuid_1 = base::Uuid::GenerateRandomV4();
  const std::string name_1 = "template_1";
  AddEntry(uuid_1, name_1, base::Time::Now(), DeskTemplateType::kTemplate);

  const base::Uuid uuid_2 = base::Uuid::GenerateRandomV4();
  const std::string name_2 = "template_2";
  AddEntry(uuid_2, name_2, base::Time::Now(), DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(
      /*grid_item_index=*/1);
  // Show replace dialogs.
  auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
  auto callback = base::BindLambdaForTesting([&]() {
    item_view->name_view()->SetText(base::UTF8ToUTF16(name_1));
    item_view->ReplaceSavedDesk(uuid_1);
  });

  dialog_controller->ShowReplaceDialog(
      Shell::GetPrimaryRootWindow(), base::UTF8ToUTF16(name_1),
      DeskTemplateType::kTemplate, callback, base::DoNothing());
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  ASSERT_TRUE(GetOverviewSession());

  // Accepting the dialog will record metrics.
  LeftClickOn(GetSavedDeskDialogAcceptButton());

  WaitForSavedDeskUI();

  // Only one template left.
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // And we expect one template in the UI.
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  EXPECT_EQ(1u, grid_items.size());

  // The Template has been replaced.
  SavedDeskNameView* name_view = grid_items[0]->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(name_1), name_view->GetText());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  EXPECT_EQ(uuid_2, entries[0]->uuid());
  // Assert metrics being recorded.
  histogram_tester.ExpectTotalCount(kReplaceTemplateHistogramName, 1);

  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());
}

// Tests that there is no animation when removing a desk with windows while the
// grid is shown. Regression test for https://crbug.com/1291770.
TEST_F(SavedDeskTest, NoAnimationWhenRemovingDesk) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create and a new desk, and create a test window on the active desk.
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  auto test_window = CreateAppWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window.get()));

  OpenOverviewAndShowSavedDeskGrid();

  // Remove the active desk. Ensure that there are no animations on the overview
  // item, otherwise a flicker will be seen as they should be hidden when the
  // desks templates grid is shown.
  RemoveDesk(desks_controller->active_desk());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  auto* overview_item =
      overview_grid->GetOverviewItemContaining(test_window.get());
  ASSERT_TRUE(overview_item);
  ui::Layer* item_widget_layer = overview_item->item_widget()->GetLayer();
  EXPECT_FALSE(item_widget_layer->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, item_widget_layer->opacity());
  EXPECT_FALSE(test_window->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(0.f, test_window->layer()->opacity());
}

// Tests that windows have their opacity reset after being hidden and then going
// to a different desk. Regression test for https://crbug.com/1292174.
TEST_F(SavedDeskTest, WindowOpacityResetAfterImmediateExit) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create and a new desk, and create a couple of test windows on the active
  // desk.
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  auto test_window1 = CreateAppWindow();
  auto test_window2 = CreateAppWindow();
  auto test_window3 = CreateAppWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window1.get()));
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window2.get()));
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window3.get()));

  OpenOverviewAndShowSavedDeskGrid();

  // All the windows are hidden to show the templates grid.
  EXPECT_EQ(0.f, test_window1->layer()->opacity());
  EXPECT_EQ(0.f, test_window2->layer()->opacity());
  EXPECT_EQ(0.f, test_window3->layer()->opacity());

  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Activate the second desk which has no windows. Test that all the windows
  // have their opacity restored.
  ActivateDesk(desks_controller->GetDeskAtIndex(1));
  EXPECT_EQ(1.f, test_window1->layer()->opacity());
  EXPECT_EQ(1.f, test_window2->layer()->opacity());
  EXPECT_EQ(1.f, test_window3->layer()->opacity());
}

// Tests that windows have their opacity reset after being hidden and then
// leaving overview. Regression test for https://crbug.com/1292773.
TEST_F(SavedDeskTest, WindowOpacityResetAfterLeavingOverview) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);

  // Create and a new desk, and create a couple of test windows on the active
  // desk.
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  auto test_window1 = CreateAppWindow();
  auto test_window2 = CreateAppWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window1.get()));
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window2.get()));
  ASSERT_EQ(2u, desks_controller->desks().size());

  OpenOverviewAndShowSavedDeskGrid();

  // The windows are hidden to show the templates grid.
  ASSERT_EQ(0.f, test_window1->layer()->opacity());
  ASSERT_EQ(0.f, test_window2->layer()->opacity());

  // The bug did not repro with zero duration as the animation callback to
  // reshow the windows would happen immediately.
  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Launch a new desk.
  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  views::Widget* library_widget =
      GetOverviewGridList()[0]->saved_desk_library_widget();
  library_widget->GetLayer()->GetAnimator()->StopAnimating();
  ASSERT_FALSE(library_widget->IsVisible());
  ASSERT_EQ(3u, desks_controller->desks().size());

  // Tests that after exiting overview, the windows have their opacities
  // restored.
  ToggleOverview();
  WaitForOverviewExitAnimation();
  ASSERT_FALSE(InOverviewSession());
  EXPECT_EQ(1.f, test_window1->layer()->opacity());
  EXPECT_EQ(1.f, test_window2->layer()->opacity());
}

// Tests that the saved desk name view can accept touch events and get
// focused. Regression test for https://crbug.com/1291769.
TEST_F(SavedDeskTest, TouchForNameView) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  ASSERT_FALSE(name_view->HasFocus());

  // The name view should receive focus after getting a gesture tap.
  GetEventGenerator()->GestureTapAt(
      name_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(name_view->HasFocus());
}

// Tests that the templates/saved desks use the right time string format. It's
// expected to align with the File App. More details can be found at:
// https://crbug.com/1268922.
TEST_F(SavedDeskTest, TimeStrFormat) {
  // Uses `01-01-2022 10:30 AM`, `Today 10:30 AM`, `Yesterday 10:30 AM`, and
  // ``Tomorrow 10:30 AM`` for test.
  base::Time time_long_ago, time_today, time_yesterday;

  static constexpr base::Time::Exploded kLongAgo = {.year = 2022,
                                                    .month = 1,
                                                    .day_of_week = 6,
                                                    .day_of_month = 1,
                                                    .hour = 10,
                                                    .minute = 30};
  ASSERT_TRUE(base::Time::FromLocalExploded(kLongAgo, &time_long_ago));

  // Today 10:30 AM.
  base::Time::Exploded exploded_today;
  base::Time::Now().LocalExplode(&exploded_today);
  exploded_today.hour = 10;
  exploded_today.minute = 30;
  exploded_today.second = 0;
  exploded_today.millisecond = 0;
  ASSERT_TRUE(base::Time::FromLocalExploded(exploded_today, &time_today));

  // Yesterday 10:30 AM.
  base::Time::Exploded exploded_yesterday;
  (base::Time::Now() - base::Days(1)).LocalExplode(&exploded_yesterday);
  exploded_yesterday.hour = 10;
  exploded_yesterday.minute = 30;
  exploded_yesterday.second = 0;
  exploded_yesterday.millisecond = 0;
  ASSERT_TRUE(
      base::Time::FromLocalExploded(exploded_yesterday, &time_yesterday));

  const std::vector<base::Uuid> uuid = {
      base::Uuid::GenerateRandomV4(),
      base::Uuid::GenerateRandomV4(),
      base::Uuid::GenerateRandomV4(),
  };
  const std::vector<std::string> name = {
      "template_1",
      "template_2",
      "template_3",
  };
  // The expected time string for each template.
  const std::vector<std::u16string> expected_timestr = {
      u"Jan 1, 2022, 10:30\u202f"
      "AM",
      u"Today 10:30\u202f"
      "AM",
      u"Yesterday 10:30\u202f"
      "AM",
  };
  std::vector<base::Time> time = {
      time_long_ago,
      time_today,
      time_yesterday,
  };

  for (size_t i = 0; i < 3; i++)
    AddEntry(uuid[i], name[i], time[i], DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  // Tests that each template comes with an expected time string format.
  std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
  for (size_t i = 0; i < 3; i++) {
    auto iter =
        base::ranges::find(grid_items, uuid[i], [](const SavedDeskItemView* v) {
          return SavedDeskItemViewTestApi(v).uuid();
        });
    ASSERT_NE(grid_items.end(), iter);

    SavedDeskItemView* item_view = *iter;
    EXPECT_EQ(expected_timestr[i],
              SavedDeskItemViewTestApi(item_view).time_view()->GetText());
  }
}

// Test that desk templates can launch snapped windows properly.
TEST_F(SavedDeskTest, SnapWindowTest) {
  auto test_window = CreateAppWindow();

  WindowState* window_state = WindowState::Get(test_window.get());
  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  LeftClickOn(GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0));

  // Test that overview is still active and there is no crash.
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

// Test that when an unsupported window left in overview grid and a supported
// window snapped into the split view, the saved desk buttons should be enabled.
TEST_F(SavedDeskTest, ButtonsEnabledForUnsupportedWindowAndSplitView) {
  // TODO(http://b/350771229): The desk bar is not shown in split view when
  // Forest is enabled, so we would not be able to access the save desk options
  // in the desk context menu.
  if (ash::features::IsForestFeatureEnabled()) {
    GTEST_SKIP() << "Skipping test body for Forest Feature.";
  }

  auto* delegate = Shell::Get()->saved_desk_delegate();

  // Create a supported app window.
  auto app_window = CreateAppWindow();
  ASSERT_TRUE(delegate->IsWindowSupportedForSavedDesk(app_window.get()));

  // Create an unsupported test window.
  auto test_window = CreateTestWindow();
  ASSERT_FALSE(delegate->IsWindowSupportedForSavedDesk(test_window.get()));

  // Start overview mode.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(1, GetOverviewGridList()[0]->num_unsupported_windows());

  auto* snappable_overview_item = GetOverviewItemForWindow(app_window.get());

  EXPECT_FALSE(GetCannotSnapWidget(snappable_overview_item));

  // Snap the extra snappable window to enter split view mode.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_window);

  // Snap the app window into the primary position in the split view.
  split_view_controller->SnapWindow(app_window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller->InSplitViewMode());

  // Now only an unsupported window left in overview grid, but the desk has
  // another supported app window in the split view, so the two buttons should
  // still be enabled.
  auto* save_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root_window);
  auto* save_for_later_button = GetSaveDeskForLaterButtonForRoot(root_window);

  EXPECT_EQ(views::Button::STATE_NORMAL, save_as_template_button->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL, save_for_later_button->GetState());
}

// Tests that we cap the number of saved desk items shown, even if the backend
// has more saved.
TEST_F(SavedDeskTest, CapTemplateItemsShown) {
  desks_storage::LocalDeskDataManager::SetDisableMaxTemplateLimitForTesting(
      true);

  constexpr unsigned long kMaxItemCount = 6;
  // Create more than the maximum number of templates allowable.
  for (unsigned long i = 1; i < kMaxItemCount + 20; i++) {
    AddEntry(base::Uuid::GenerateRandomV4(),
             "template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  OpenOverviewAndShowSavedDeskGrid();

  // Check to make sure we are only showing up to the maximum number of items.
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
  EXPECT_EQ(kMaxItemCount, grid_items.size());
}

// Tests that click or tap could exit grid view and commit name change when
// appropriate. Regression test for https://crbug.com/1290568.
TEST_F(SavedDeskTest, ClickOrTapToExitGridView) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_2", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template_3", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Test mouse click.
  {
    OpenOverviewAndShowSavedDeskGrid();

    SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
    EXPECT_FALSE(name_view->HasFocus());

    // The name view should receive focus after getting a mouse click.
    LeftClickOn(name_view);
    EXPECT_TRUE(name_view->HasFocus());

    // The name view should release focus after getting a mouse click outside
    // the grid item.
    std::vector<SavedDeskItemView*> grid_items =
        GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(grid_items[0]->GetBoundsInScreen().origin() -
                                 gfx::Vector2d(20, 20));
    event_generator->ClickLeftButton();
    EXPECT_FALSE(name_view->HasFocus());

    // It should exit overview when click outside the grid items.
    event_generator->ClickLeftButton();
    EXPECT_FALSE(GetOverviewSession());
  }

  // Test gesture tap.
  {
    OpenOverviewAndShowSavedDeskGrid();

    SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
    EXPECT_FALSE(name_view->HasFocus());

    // The name view should receive focus after getting a gesture tap.
    auto* event_generator = GetEventGenerator();
    event_generator->GestureTapAt(name_view->GetBoundsInScreen().CenterPoint());
    EXPECT_TRUE(name_view->HasFocus());

    // The name view should release focus after getting a gesture tap outside
    // the grid item.
    std::vector<SavedDeskItemView*> grid_items =
        GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
    event_generator->GestureTapAt(
        {grid_items[0]->GetBoundsInScreen().x() - 20,
         grid_items[0]->GetBoundsInScreen().y() - 20});
    EXPECT_FALSE(name_view->HasFocus());

    // It should exit overview when tap outside the grid items.
    event_generator->GestureTapAt(grid_items[0]->GetBoundsInScreen().origin() -
                                  gfx::Vector2d(20, 20));
    EXPECT_FALSE(GetOverviewSession());
  }
}

// Tests that long pressing on the library view to commit name changes.
TEST_F(SavedDeskTest, LongPressToCommitNameChanges) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::Uuid::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskLibraryView* library_view = overview_grid->GetSavedDeskLibraryView();
  SavedDeskItemView* template1 = GetItemViewFromSavedDeskGrid(0);
  SavedDeskItemView* template2 = GetItemViewFromSavedDeskGrid(1);
  SavedDeskNameView* name_view1 = template1->name_view();
  SavedDeskNameView* name_view2 = template2->name_view();
  EXPECT_EQ(GetHoverState(template1), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(template2), SavedDeskItemHoverState::kIcons);

  // Tests that long pressing on template1 which is in edit mode should commit
  // changes and bring up the hover button.
  auto* event_generator = GetEventGenerator();
  LeftClickOn(name_view1);
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(name_view1->HasFocus());
  EXPECT_TRUE(name_view1->HasSelection());
  LongGestureTap(template1->GetBoundsInScreen().CenterPoint(), event_generator);
  EXPECT_FALSE(name_view1->HasFocus());
  EXPECT_EQ(GetHoverState(template1), SavedDeskItemHoverState::kHover);
  EXPECT_EQ(GetHoverState(template2), SavedDeskItemHoverState::kIcons);

  // Test that long pressing the library view outside of template2 which is in
  // edit mode should commit changes.
  LeftClickOn(name_view2);
  EXPECT_EQ(GetHoverState(template1), SavedDeskItemHoverState::kIcons);
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(name_view2->HasFocus());
  EXPECT_TRUE(name_view2->HasSelection());
  gfx::Point p = library_view->GetBoundsInScreen().bottom_center();
  p.Offset(0, -50);
  LongGestureTap(p, event_generator);
  EXPECT_FALSE(name_view2->HasFocus());
  EXPECT_EQ(GetHoverState(template1), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(template2), SavedDeskItemHoverState::kIcons);

  // Tests that long pressing on template2 when template1 in edit mode should
  // commit changes for template1 and bring up the hover button for template2.
  LeftClickOn(name_view1);
  EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
  EXPECT_TRUE(name_view1->HasFocus());
  EXPECT_TRUE(name_view1->HasSelection());
  LongGestureTap(template2->GetBoundsInScreen().CenterPoint(), event_generator);
  EXPECT_FALSE(name_view1->HasFocus());
  EXPECT_EQ(GetHoverState(template1), SavedDeskItemHoverState::kIcons);
  EXPECT_EQ(GetHoverState(template2), SavedDeskItemHoverState::kHover);
}

// Tests that right clicking on the wallpaper while showing the saved desks grid
// does not exit overview.
TEST_F(SavedDeskTest, RightClickOnWallpaperStaysInOverview) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowSavedDeskGrid();

  // Find a point that doesn't touch the item view, but is still in the library
  // widget's window bounds. Right clicking this point should not close
  // overview, as it should open the wallpaper context menu.
  SavedDeskItemView* item_view =
      GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0);
  DCHECK(item_view);
  gfx::Rect item_view_expanded_bounds = item_view->GetBoundsInScreen();
  item_view_expanded_bounds.set_y(item_view_expanded_bounds.y() - 32);
  item_view_expanded_bounds.set_height(item_view_expanded_bounds.height() + 32);
  const gfx::Rect library_widget_bounds = GetOverviewGridList()[0]
                                              ->saved_desk_library_widget()
                                              ->GetWindowBoundsInScreen();
  ASSERT_TRUE(library_widget_bounds.Contains(item_view_expanded_bounds));

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(item_view_expanded_bounds.bottom_right());
  event_generator->ClickRightButton();
  ASSERT_TRUE(InOverviewSession());

  // Left click twice to exit overview, once to close the context menu and once
  // to do the actual exiting.
  event_generator->ClickLeftButton();
  event_generator->ClickLeftButton();
  EXPECT_FALSE(InOverviewSession());
}

// Tests that if there is an existing visible on all desks window, after
// launching a new desk the window is part of the new desk and is in an overview
// item.
TEST_F(SavedDeskTest, VisibleOnAllDesksWindowShownProperly) {
  auto* controller = DesksController::Get();
  ASSERT_EQ(1, controller->GetNumberOfDesks());

  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create a window which is shown on all desks.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));

  OpenOverviewAndShowSavedDeskGrid();

  // Click on the saved desk item to launch the new template.
  SavedDeskItemView* template_item =
      GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0);
  DCHECK(template_item);
  LeftClickOn(template_item);
  ASSERT_EQ(2, controller->GetNumberOfDesks());

  // The visible on all desks window belongs to the active desk, and has an
  // associated overview item.
  EXPECT_TRUE(controller->BelongsToActiveDesk(window.get()));
  EXPECT_TRUE(GetOverviewItemForWindow(window.get()));
}

// Test save same desk as template won't create name with number on the template
// view for the second template.
TEST_F(SavedDeskTest, NoDuplicateDisplayedName) {
  // There are no saved desk entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // The "Save desk as template" option is visible when at least one window is
  // open.
  auto* root = Shell::GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveAsTemplate));
  } else {
    SavedDeskSaveDeskButton* save_desk_as_template_button =
        GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(save_desk_as_template_button);
    EXPECT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_desk_as_template_button);
  }

  ASSERT_EQ(1ul, GetAllEntries().size());
  WaitForSavedDeskUI();
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  EXPECT_EQ(u"Desk 1", GetItemViewFromSavedDeskGrid(0)->name_view()->GetText());
  // The new template name still have name nudge to maintain it's uniqueness.
  EXPECT_EQ(u"Desk 1", GetAllEntries().back()->template_name());

  // Exit overview and save the same desk again.
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());
  ToggleOverview();

  // The "Save desk as template" option is visible when at least one window is
  // open.
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveAsTemplate));
  } else {
    SavedDeskSaveDeskButton* save_desk_as_template_button =
        GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(save_desk_as_template_button);
    EXPECT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_desk_as_template_button);
  }

  // At this point the template name matches the desk name.
  ASSERT_EQ(2ul, GetAllEntries().size());
  WaitForSavedDeskUI();
  // Newly created template name_view.
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_TRUE(name_view->HasFocus());
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  DeskNameView* desk_name_view =
      overview_grid->desks_bar_view()->mini_views().back()->desk_name_view();
  // Check newly created template name view doesn't have a "(1)" appended.
  EXPECT_EQ(desk_name_view->GetText(), name_view->GetText());
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  EXPECT_EQ(u"Desk 1", name_view->GetText());
  // The actual template name will still have "(1)" appended to maintain its
  // uniqueness.
  EXPECT_EQ(u"Desk 1 (1)",
            GetItemViewFromSavedDeskGrid(0)->saved_desk().template_name());

  // Set the second template to have a new unique name by updating the model
  // directly. This mimics updating the name on a different device and is the
  // only way to change the name without prompting the replace dialog.
  SavedDeskItemView* second_item = GetItemViewFromSavedDeskGrid(1);
  auto new_desk_template = second_item->saved_desk().Clone();
  new_desk_template->set_template_name(u"Desk 2");
  const base::Uuid uuid = new_desk_template->uuid();

  base::RunLoop loop;
  desk_model()->AddOrUpdateEntry(
      std::move(new_desk_template),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();

  desks_storage::DeskModel::GetEntryByUuidResult result =
      desk_model()->GetEntryByUUID(uuid);

  EXPECT_EQ(desks_storage::DeskModel::GetEntryByUuidStatus::kOk, result.status);
  // `LocalDeskStorage` does not support
  // `EntriesAddedOrUpdatedRemotely`, so
  // manually call it to simulate what the real model would do.
  saved_desk_util::GetSavedDeskPresenter()->EntriesAddedOrUpdatedRemotely(
      {result.entry.get()});
  ASSERT_EQ(u"Desk 2", second_item->name_view()->GetText());
  ASSERT_EQ(u"Desk 2", second_item->saved_desk().template_name());

  // Save template 2 under new name and confirm, this will trigger replace
  // dialog.
  name_view->RequestFocus();
  name_view->SetText(u"Desk 2");
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "TemplateDialogForTesting");
  // Normally we would want to clear focus by simulating a user action (enter or
  // click outside) but that doesn't work after already clear focus once before.
  views::Widget* library_widget = overview_grid->saved_desk_library_widget();
  library_widget->GetFocusManager()->ClearFocus();
  views::Widget* dialog_widget = waiter.WaitIfNeededAndGet();
  EXPECT_TRUE(dialog_widget->IsActive());

  // Cancel on replace dialog will revert view name to template name.
  LeftClickOn(saved_desk_util::GetSavedDeskDialogController()
                  ->GetSystemDialogViewForTesting()
                  ->GetCancelButtonForTesting());
  EXPECT_EQ(u"Desk 1", name_view->GetText());
}

// Tests that if there is a duplicate template name, saving a new template will
// select all the text. Regression test for https://crbug.com/1303924.
TEST_F(SavedDeskTest, SelectAllAfterSavingDuplicateTemplate) {
  // First add a template that has the same name as the active desk.
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  AddEntry(base::Uuid::GenerateRandomV4(), "Desk 1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto test_window = CreateAppWindow();
  ToggleOverview();

  // Click on the "Save desk as template" option.
  auto* root = Shell::GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveAsTemplate));
  } else {
    LeftClickOn(GetSaveDeskAsTemplateButtonForRoot(root));
  }

  // Wait for the saved desk UI but don't click off or press enter/exit, as we
  // want to stay focused on the desk's name field.
  WaitForSavedDeskUI();

  // Expect that the entire text of the new template is selected.
  EXPECT_EQ(u"Desk 1", GetItemViewFromSavedDeskGrid(0)->name_view()->GetText());
  EXPECT_EQ(u"Desk 1", GetItemViewFromSavedDeskGrid(1)->name_view()->GetText());
  EXPECT_TRUE(GetItemViewFromSavedDeskGrid(0)->name_view()->HasFocus());
  EXPECT_EQ(u"Desk 1",
            GetItemViewFromSavedDeskGrid(0)->name_view()->GetSelectedText());
}

// Tests that a newly saved template will always show up on the top left corner
// regardless of its name and verify that it goes to its alphabetical order
// once the name is confirmed.
TEST_F(SavedDeskTest, NoSortBeforeNameConfirmed) {
  // Create a window to enable the save as template button.
  auto test_window = CreateAppWindow();

  // Add an entry with a low lexiconic value for the template name to test that
  // the new saved template is always preceding this one.
  AddEntry(base::Uuid::GenerateRandomV4(), "aaaa", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Enter overview and save the same desk again.
  ToggleOverview();

  // The "Save desk as template" option is visible when at least one window is
  // open.
  auto* root = Shell::GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveAsTemplate));
  } else {
    SavedDeskSaveDeskButton* save_desk_as_template_button =
        GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(save_desk_as_template_button);
    EXPECT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
    LeftClickOn(save_desk_as_template_button);
  }

  // The newly saved template should be in the front, even though its name is
  // not in alphabetical order.
  ASSERT_EQ(2ul, GetAllEntries().size());
  WaitForSavedDeskUI();

  // Newly created template name_view.
  SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
  EXPECT_TRUE(name_view->HasFocus());
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  EXPECT_EQ(u"Desk 1", name_view->GetText());

  // Change the saved template name and save it.
  PressAndReleaseKey(ui::VKEY_Z);
  PressAndReleaseKey(ui::VKEY_Z);
  PressAndReleaseKey(ui::VKEY_RETURN);
  WaitForSavedDeskUI();

  // Check that the name is saved and it's moved to its proper alphabetical
  // order. This should be the second entry in the templates grid.
  name_view = GetItemViewFromSavedDeskGrid(1)->name_view();
  EXPECT_EQ(u"zz", name_view->GetText());
}

TEST_F(SavedDeskTest, NudgeOnTheCorrectDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto test_window = CreateAppWindow();
  OpenOverviewAndSaveTemplate(Shell::GetPrimaryRootWindow());

  // The desks templates widget associated with the primary display should be
  // active.
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            window_util::GetActiveWindow()->GetRootWindow());
}

// Tests that the save desk button container is properly placed after an
// overview item is closed via swipe.
TEST_F(SavedDeskTest, SaveDeskButtonContainerVisibleAfterSwipeToClose) {
  // The save desk button container is removed as part of the UI revamp.
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP();
  }

  // Use a test widget so we can close it properly after swiping to close. The
  // order matters here; overview items are ordered by MRU order, so the most
  // recently created widget corresponds to the first overview item.
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto widget1 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ToggleOverview();

  auto* item1 = GetOverviewItemForWindow(widget1->GetNativeWindow());
  ASSERT_TRUE(item1);

  // Swipe down on `item1` to close it.
  GetEventGenerator()->set_current_screen_location(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  GetEventGenerator()->PressMoveAndReleaseTouchBy(0, 200);

  // `NativeWidgetAura::Close()` is on a post task so flush that task.
  base::RunLoop().RunUntilIdle();

  item1 = GetOverviewItemForWindow(widget1->GetNativeWindow());
  ASSERT_FALSE(item1);

  // Tests that the save desk as template button and the remaining overview item
  // bounds do not intersect (they are both fully visible).
  auto* item2 = GetOverviewItemForWindow(widget2->GetNativeWindow());
  ASSERT_TRUE(item2);
  SavedDeskSaveDeskButtonContainer* save_desk_button_container =
      GetSaveDeskButtonContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(item2->target_bounds().Intersects(
      gfx::RectF(save_desk_button_container->GetBoundsInScreen())));
}

TEST_F(SavedDeskTest, AdminTemplate) {
  AddEntry(base::Uuid::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kPolicy, DeskTemplateType::kTemplate,
           std::make_unique<app_restore::RestoreData>());

  OpenOverviewAndShowSavedDeskGrid();

  // Tests that the name is read only and not focusable.
  SavedDeskItemView* item_view =
      GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0);
  SavedDeskNameView* name_view = item_view->name_view();
  EXPECT_TRUE(name_view->GetReadOnly());
  EXPECT_FALSE(name_view->IsFocusable());

  // Tests that there is an admin message in the time view and that the delete
  // button is not created.
  SavedDeskItemViewTestApi test_api(item_view);
  EXPECT_EQ(u"Shared by your administrator", test_api.time_view()->GetText());
  EXPECT_FALSE(test_api.delete_button());

  // Tests that the name view cannot be tabbed into for admin templates, as they
  // aren't editable anyhow.
  EXPECT_EQ(item_view, GetFocusedView());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_NE(name_view, GetFocusedView());
}

// Tests that the scroll bar is hidden when there is enough space to show all
// scroll contents, and is shown otherwise.
TEST_F(SavedDeskTest, ScrollBarVisibility) {
  // Make display size large enough so that we could test both hidden and shown
  // scroll bar.
  UpdateDisplay("1000x800");

  // 3 templates and 3 save-and-recall desks would not show the scroll bar.
  {
    // Add 3 `kTemplate` entries.
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 1",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 2",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 3",
             base::Time::Now(), DeskTemplateType::kTemplate);

    // Add 3 `kSaveAndRecall` entries.
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 1",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 2",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 3",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);

    OpenOverviewAndShowSavedDeskGrid();

    SavedDeskLibraryView* library_view =
        GetOverviewGridList()[0]->GetSavedDeskLibraryView();
    SavedDeskLibraryViewTestApi test_api(library_view);
    EXPECT_FALSE(test_api.scroll_view()->vertical_scroll_bar()->GetVisible());

    ExitOverview();
  }

  // 6 templates and 6 save-and-recall desks would show the scroll bar.
  {
    // Add 3 more `kTemplate` entries.
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 4",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 5",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 6",
             base::Time::Now(), DeskTemplateType::kTemplate);

    // Add 3 more `kSaveAndRecall` entries.
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 4",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 5",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::Uuid::GenerateRandomV4(), "desk_template 6",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);

    OpenOverviewAndShowSavedDeskGrid();

    SavedDeskLibraryView* library_view =
        GetOverviewGridList()[0]->GetSavedDeskLibraryView();
    SavedDeskLibraryViewTestApi test_api(library_view);
    EXPECT_TRUE(test_api.scroll_view()->vertical_scroll_bar()->GetVisible());

    ExitOverview();
  }
}

// Tests that the save desk item view is fully visible with the focus change.
TEST_F(SavedDeskTest, ScrollWithHighlightChange) {
  // Add 6 `kTemplate` entries and 6 `kSaveAndRecall` entries.
  for (size_t i = 1; i <= 6; i++) {
    AddEntry(base::Uuid::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(),
             "saved_desk " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kSaveAndRecall);
  }

  OpenOverviewAndShowSavedDeskGrid();

  for (size_t i = 0; i < 12; i++) {
    SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(i);

    // Verify item view is focused and fully visible.
    EXPECT_TRUE(item_view->HasFocus());
    EXPECT_EQ(item_view->GetPreferredSize(),
              item_view->GetVisibleBounds().size());
    PressAndReleaseKey(ui::VKEY_TAB);

    // Verify name view is focused and fully visible.
    EXPECT_TRUE(item_view->name_view()->HasFocus());
    EXPECT_EQ(item_view->name_view()->GetPreferredSize(),
              item_view->name_view()->GetVisibleBounds().size());
    PressAndReleaseKey(ui::VKEY_TAB);
  }
}

// Tests that the scroll bar works with the keyboard.
TEST_F(SavedDeskTest, ScrollWithKeyboard) {
  // Add 6 `kTemplate` entries and 6 `kSaveAndRecall` entries.
  for (size_t i = 1; i <= 6; i++) {
    AddEntry(base::Uuid::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
    AddEntry(base::Uuid::GenerateRandomV4(),
             "saved_desk " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kSaveAndRecall);
  }

  OpenOverviewAndShowSavedDeskGrid();

  // Press keys to scroll through the whole library page, and verify the scroll
  // position.
  std::array<ui::KeyboardCode, 4> keys = {ui::VKEY_END, ui::VKEY_HOME,
                                          ui::VKEY_NEXT, ui::VKEY_PRIOR};
  SavedDeskLibraryView* library_view =
      GetOverviewGridList()[0]->GetSavedDeskLibraryView();
  SavedDeskLibraryViewTestApi test_api(library_view);
  int scroll_position =
      test_api.scroll_view()->vertical_scroll_bar()->GetPosition();
  for (ui::KeyboardCode key : keys) {
    PressAndReleaseKey(key);
    int new_scroll_position =
        test_api.scroll_view()->vertical_scroll_bar()->GetPosition();
    EXPECT_NE(scroll_position, new_scroll_position);
    scroll_position = new_scroll_position;
  }
}

// Tests that the save desk item view is fully visible when it gains focus.
TEST_F(SavedDeskTest, FocusedDeskItemFullyVisible) {
  // Set up a small display, so that the new saved desk may fall completely
  // outside the display.
  UpdateDisplay("800x500");

  // Add 6 `kTemplate` entries.
  for (size_t i = 1; i <= 6; i++) {
    AddEntry(base::Uuid::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  // Create a window then save the current desk for later.
  CreateAppWindow().release();
  ToggleOverview();
  auto* root = Shell::Get()->GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveForLater));
  } else {
    auto* save_desk_button = GetSaveDeskForLaterButtonForRoot(root);
    ASSERT_TRUE(save_desk_button);
    LeftClickOn(save_desk_button);
  }

  WaitForSavedDeskUI();
  WaitForSavedDeskUI();

  // The newly saved desk item should be fully visible.
  SavedDeskItemView* item_view = GetItemViewFromSavedDeskGrid(6);
  ASSERT_EQ(u"Desk 1", item_view->name_view()->GetText());
  ASSERT_TRUE(item_view->name_view()->HasFocus());
  EXPECT_EQ(item_view->name_view()->GetPreferredSize(),
            item_view->name_view()->GetVisibleBounds().size());
  EXPECT_EQ(item_view->GetPreferredSize(),
            item_view->GetVisibleBounds().size());
}

// Tests that the save desk button is hidden when an active desk with windows is
// closed and a desk with no windows is activated. Then checks to see that the
// visibility is restored for the button when desk removal is undone.
TEST_F(SavedDeskTest,
       CorrectlyUpdateSaveDeskButtonVisibilityOnActiveDeskClose) {
  auto* controller = DesksController::Get();
  const auto& desks = controller->desks();

  // We create a new desk and add an app window to the first desk so that
  // closing the first desk with its windows will result in us going from having
  // app windows in overview to having no app windows in overview, which should
  // cause an update to the visibility of the save desk buttons.
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(2u, desks.size());
  Desk* active_desk = desks[0].get();
  Desk* inactive_desk = desks[1].get();
  ASSERT_TRUE(active_desk->is_active());
  ASSERT_FALSE(active_desk->ContainsAppWindows());
  ASSERT_FALSE(inactive_desk->ContainsAppWindows());
  const auto& window = CreateAppWindow();
  controller->SendToDeskAtIndex(window.get(), 0);
  ASSERT_EQ(1u, active_desk->GetAllAppWindows().size());

  ToggleOverview();
  auto* root = Shell::GetPrimaryRootWindow();
  OverviewGrid* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root);

  // Pre-check whether the save desk button is in the correct state.
  if (features::IsSavedDeskUiRevampEnabled()) {
    // Close the menu after getting each item so we can properly check the
    // context menu button visibility.
    EXPECT_TRUE(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveAsTemplate));
    DesksTestApi::MaybeCloseContextMenuForGrid(overview_grid);
    EXPECT_TRUE(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveForLater));
    DesksTestApi::MaybeCloseContextMenuForGrid(overview_grid);
  } else {
    EXPECT_TRUE(overview_grid->IsSaveDeskButtonContainerVisible());
  }

  const OverviewDeskBarView* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  DeskMiniView* mini_view_to_be_removed =
      desks_bar_view->FindMiniViewForDesk(active_desk);
  ASSERT_TRUE(mini_view_to_be_removed);

  // Close the active desk and check that the save desk option updates
  // correctly.
  GetEventGenerator()->MoveMouseTo(
      mini_view_to_be_removed->GetBoundsInScreen().CenterPoint());
  auto* close_button =
      mini_view_to_be_removed->desk_action_view()->close_all_button();
  ASSERT_TRUE(close_button);
  LeftClickOn(close_button);
  if (features::IsSavedDeskUiRevampEnabled()) {
    auto* overview_controller = Shell::Get()->overview_controller();
    ASSERT_TRUE(overview_controller->InOverviewSession());
    ASSERT_EQ(desks_bar_view->mini_views().size(), 1u);
    EXPECT_FALSE(
        desks_bar_view->mini_views()[0]->desk_action_view()->GetVisible());
  } else {
    auto* overview_controller = Shell::Get()->overview_controller();
    ASSERT_TRUE(overview_controller->InOverviewSession());
    EXPECT_FALSE(overview_grid->IsSaveDeskButtonContainerVisible());
  }

  // Try undoing desk close to see if the save desk button returns to the right
  // state.
  views::Button* undo_button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  ASSERT_TRUE(undo_button);

  // Clicking the undo button on the toast re-adds the mini view for the closed
  // desk, so we need to wait for the bar view to layout again.
  LeftClickOn(undo_button);
  views::test::RunScheduledLayout(overview_grid->desks_bar_view());
  if (features::IsSavedDeskUiRevampEnabled()) {
    EXPECT_TRUE(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveAsTemplate));
    DesksTestApi::MaybeCloseContextMenuForGrid(overview_grid);
    EXPECT_TRUE(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::CommandId::kSaveForLater));
  } else {
    EXPECT_TRUE(overview_grid->IsSaveDeskButtonContainerVisible());
  }
}

// Tests that there are no overview item windows on theme change.
TEST_F(SavedDeskTest, NoOverviewItemWindowOnThemeChange) {
  // Add a saved desk entry.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create a test window.
  auto window = CreateAppWindow();

  // Enter overview and ensure overview item window is visible.
  ToggleOverview();
  auto* overview_item = GetOverviewItemForWindow(window.get());
  EXPECT_EQ(window.get(), overview_item->GetWindow());
  EXPECT_TRUE(overview_item->item_widget()->IsVisible());

  // Enter library and ensure overview item window is *not* visible.
  ShowSavedDeskLibrary();
  WaitForSavedDeskLibrary();
  EXPECT_FALSE(overview_item->item_widget()->IsVisible());

  // Simulate theme change and ensure overview item window is *not* visible.
  overview_item->item_widget()->ThemeChanged();
  EXPECT_FALSE(overview_item->item_widget()->IsVisible());
}

using DeskSaveAndRecallTest = SavedDeskTest;

TEST_F(DeskSaveAndRecallTest, SaveDeskForLater) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600,800x600");

  constexpr char16_t kDeskName[] = u"Save for later";

  // Create and activate a new desk.
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk = desks_controller->desks().back().get();
  desk->SetName(kDeskName, /*set_by_user=*/true);
  ActivateDesk(desk);

  // Verify that we have two desks (saving will later remove one of them).
  EXPECT_EQ(2ul, desks_controller->desks().size());

  // Create a couple of test windows.
  auto test_window1 = CreateAppWindow();
  auto test_window2 = CreateAppWindow();
  // When saving the desk, the windows will be closed automatically. To verify
  // that this happens we create a WindowTracker. The unique_ptrs have to be
  // released since they would otherwise end up with dangling pointers.
  aura::WindowTracker tracker({test_window1.release(), test_window2.release()});

  // Open overview and save the desk.
  OpenOverviewAndSaveDeskForLater(Shell::Get()->GetPrimaryRootWindow());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  const DeskTemplate& saved_desk = *entries[0];
  EXPECT_EQ(DeskTemplateType::kSaveAndRecall, saved_desk.type());
  EXPECT_EQ(kDeskName, saved_desk.template_name());

  // Verify that saving the desk has closed the test windows.
  EXPECT_TRUE(tracker.windows().empty());

  // Verify that the desk has been removed.
  EXPECT_EQ(1ul, desks_controller->desks().size());

  histogram_tester.ExpectTotalCount(kNewSaveAndRecallHistogramName, 1);
}

TEST_F(DeskSaveAndRecallTest, SaveDeskForLaterWithSingleDesk) {
  UpdateDisplay("800x600");

  constexpr char16_t kDeskName[] = u"Save for later";

  // Verify that we have one desk. If there is only a single desk when saving, a
  // new desk will be created.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(1ul, desks_controller->desks().size());

  // Rename the current desk so that we can later verify that we have a new
  // desk.
  const_cast<Desk*>(desks_controller->active_desk())
      ->SetName(kDeskName, /*set_by_user=*/true);

  // Create a test window that we release immediately as it will be closed
  // automatically by the code under test.
  CreateAppWindow().release();

  // Open overview and save the desk.
  OpenOverviewAndSaveDeskForLater(Shell::Get()->GetPrimaryRootWindow());

  // We should still only have one desk, but it should be a new one (name is
  // different from before).
  EXPECT_EQ(1ul, desks_controller->desks().size());
  EXPECT_NE(kDeskName, desks_controller->active_desk()->name());
}

// Tests that all desk window is not closed nor saved by clicking save desk for
// later button.
TEST_F(DeskSaveAndRecallTest, SaveDeskForLaterWithAllDeskWindow) {
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Create an all desk window.
  auto all_desk_window = CreateAppWindow(gfx::Rect(300, 300));
  auto* all_desk_widget =
      views::Widget::GetWidgetForNativeWindow(all_desk_window.get());
  all_desk_widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(
      desks_util::IsWindowVisibleOnAllWorkspaces(all_desk_window.get()));

  // Create two test windows.
  auto test_window1 = CreateAppWindow(gfx::Rect(400, 400));
  auto test_window2 = CreateAppWindow(gfx::Rect(500, 500));

  // When saving the desk, the windows will be closed automatically. To verify
  // that this happens we create a WindowTracker. The unique_ptrs have to be
  // released since they would otherwise end up with dangling pointers.
  aura::WindowTracker tracker({all_desk_window.release(),
                               test_window1.release(), test_window2.release()});

  // Open overview and save the desk.
  OpenOverviewAndSaveDeskForLater(Shell::Get()->GetPrimaryRootWindow());
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> entries =
      GetAllEntries();
  ASSERT_EQ(1u, entries.size());

  // Verify that saving the desk has closed the two test windows but not all
  // desk window.
  ASSERT_EQ(1u, tracker.windows().size());
  EXPECT_TRUE(
      desks_util::IsWindowVisibleOnAllWorkspaces(tracker.windows().front()));

  // Verify the overview item window for all desk window is not visible since
  // it's still in the library view.
  auto* all_desk_window_overview_item =
      GetOverviewItemForWindow(tracker.windows().front());
  EXPECT_FALSE(all_desk_window_overview_item->item_widget()->IsVisible());
}

// Tests that when saving a desk with only all desk window, it can show the
// library view and remove the desk. More details about the bug from
// b/272343211.
TEST_F(DeskSaveAndRecallTest, SaveDeskForLaterForAllDeskWindowOnDesk) {
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  EXPECT_EQ(2ul, desks_controller->desks().size());

  // Create an all desk window.
  auto all_desk_window = CreateAppWindow(gfx::Rect(300, 300));
  auto* all_desk_widget =
      views::Widget::GetWidgetForNativeWindow(all_desk_window.get());
  all_desk_widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(
      desks_util::IsWindowVisibleOnAllWorkspaces(all_desk_window.get()));

  // When there is only all desk window, no window would be closed. Therefore we
  // do not need to wait.
  OpenOverviewAndSaveDeskForLater(Shell::Get()->GetPrimaryRootWindow(),
                                  /*observe_closing_windows=*/false);
  EXPECT_EQ(1ul, desks_controller->desks().size());
}

TEST_F(DeskSaveAndRecallTest, RecallSavedDesk) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600");

  constexpr char16_t kDeskName[] = u"Save for later";

  DesksController* desks_controller = DesksController::Get();
  const_cast<Desk*>(desks_controller->active_desk())
      ->SetName(kDeskName, /*set_by_user=*/true);

  // Create a test window that we release immediately as it will be closed
  // automatically by the code under test.
  CreateAppWindow().release();

  // Open overview and save the desk.
  OpenOverviewAndSaveDeskForLater(Shell::Get()->GetPrimaryRootWindow());

  // Recall the desk.
  SavedDeskItemView* template_item =
      GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0);
  ASSERT_TRUE(template_item);
  LeftClickOn(template_item);

  // Verify that a new desk has been created and that it has the name of the
  // saved desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(kDeskName, desks_controller->GetDeskAtIndex(1)->name());

  // Verify that the saved desk has been deleted.
  EXPECT_TRUE(GetAllEntries().empty());

  // Assert that histogram metrics were recorded.
  histogram_tester.ExpectTotalCount(kLaunchSaveAndRecallHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kNewDeskHistogramName,
      static_cast<int>(DesksCreationRemovalSource::kSaveAndRecall), 1);
}

TEST_F(DeskSaveAndRecallTest, DeleteSaveAndRecallRecordsMetric) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600");

  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  AddEntry(uuid, "saved_desk", base::Time::Now(),
           DeskTemplateType::kSaveAndRecall);

  OpenOverviewAndShowSavedDeskGrid();

  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the saved desk.
  DeleteSavedDeskItem(uuid, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());

  // Assert that histogram metrics were recorded.
  histogram_tester.ExpectTotalCount(kDeleteSaveAndRecallHistogramName, 1);
}

// Tests that we no longer pull the comparison for the desk names from the
// currently active desk. Regression test for https://crbug.com/1344915.
TEST_F(DeskSaveAndRecallTest, SaveDeskWithDuplicateName) {
  UpdateDisplay("800x600");

  constexpr char16_t kDefaultDeskName[] = u"Desk 1";
  constexpr char16_t kNewDeskName[] = u"Save for later";

  // Verify that we have one desk. If there is only a single desk when saving, a
  // new desk will be created.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(1ul, desks_controller->desks().size());
  EXPECT_EQ(kDefaultDeskName, desks_controller->active_desk()->name());

  auto save_and_check = [this](const char16_t* name) {
    // Create a test window that we release immediately as it will be closed
    // automatically by the code under test.
    CreateAppWindow().release();

    // Open overview and save the desk.
    ToggleOverview();

    auto* root = Shell::Get()->GetPrimaryRootWindow();
    if (features::IsSavedDeskUiRevampEnabled()) {
      LeftClickOn(GetActiveDeskActionContextMenuItem(
          root, DeskActionContextMenu::kSaveForLater));
    } else {
      LeftClickOn(GetSaveDeskForLaterButtonForRoot(root));
    }

    WaitForSavedDeskUI();
    WaitForSavedDeskUI();

    // Expect that the last added template item name view has focus, and verify
    // that we have a saved desk with the expected `name`.
    OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
    SavedDeskNameView* name_view = GetItemViewFromSavedDeskGrid(0)->name_view();
    EXPECT_TRUE(overview_grid->IsSavedDeskNameBeingModified());
    EXPECT_TRUE(name_view->HasFocus());
    EXPECT_TRUE(name_view->HasSelection());
    EXPECT_EQ(name, name_view->GetText());
  };

  // Save the currently active desk which has the default name "Desk 1".
  save_and_check(kDefaultDeskName);

  // Exit overview.
  ToggleOverview();

  // Expect we have only one desk, and rename the active desk to "Save for
  // later".
  EXPECT_EQ(1ul, desks_controller->desks().size());
  const_cast<Desk*>(desks_controller->active_desk())
      ->SetName(kNewDeskName, /*set_by_user=*/true);

  // Verify that the desk is saved correctly, and that the name is not replaced
  // by the active desk name.
  save_and_check(kNewDeskName);

  // Verify the active desk is now named "Desk 1".
  EXPECT_EQ(1ul, desks_controller->desks().size());
  EXPECT_EQ(kDefaultDeskName, desks_controller->active_desk()->name());
}

// Tests that ChromeVox focuses correctly when we exit the desks library and
// that there is no crash. Regression test for https://crbug.com/1351467.
TEST_F(DeskSaveAndRecallTest, ExitOverviewDeskItemFocusCrash) {
  // The fade out animation of the desks templates grid must be enabled for this
  // crash to have happened.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->spoken_feedback().SetEnabled(true);
  EXPECT_TRUE(accessibility_controller->spoken_feedback().enabled());

  // Ensure we have a desk saved so we can go into the library.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kSaveAndRecall);

  // Check that we can enter overview, and that there are no windows present.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->no_windows_widget());

  // Enter the desks library.
  ShowSavedDeskLibrary();
  WaitForSavedDeskLibrary();

  // Simulate how ChromeVox would focus on the desk item view.
  SavedDeskItemView* first_item = GetItemViewFromSavedDeskGrid(0);
  first_item->RequestFocus();

  ASSERT_FALSE(Shell::IsSystemModalWindowOpen());

  // Press "Ctrl + w" to show and focus on the delete dialog.
  PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(Shell::IsSystemModalWindowOpen());

  // Exit the dialog and wait for it to close.
  // TOOD(b/292156927): Use esc key to dismiss the dialog when this is fixed.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_RETURN);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(Shell::IsSystemModalWindowOpen());
  ASSERT_TRUE(InOverviewSession());

  // We should be able to exit overview without a crash.
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());
}

// Tests that adding desks to the max and then saving and recalling one of the
// desks successfully disables the new desk button.
TEST_F(DeskSaveAndRecallTest, NewDeskButtonDisabledWhenRecallingToMaxDesks) {
  auto* controller = DesksController::Get();

  while (controller->CanCreateDesks())
    NewDesk();

  // Activate the last desk and add a window in it that will be destroyed later.
  ActivateDesk(controller->desks().back().get());
  aura::WindowTracker tracker({CreateAppWindow().release()});
  ToggleOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // We should have the max number of desks at this point and therefore the new
  // desk button should be disabled.
  ASSERT_FALSE(controller->CanCreateDesks());
  ASSERT_FALSE(GetNewDeskButtonEnabledState(Shell::GetPrimaryRootWindow()));

  // After saving the last desk for later, the new desk button should be enabled
  // again.
  auto* root = Shell::GetPrimaryRootWindow();
  if (features::IsSavedDeskUiRevampEnabled()) {
    LeftClickOn(GetActiveDeskActionContextMenuItem(
        root, DeskActionContextMenu::kSaveForLater));
  } else {
    LeftClickOn(GetSaveDeskForLaterButtonForRoot(root));
  }

  WaitForSavedDeskUI();
  WaitForSavedDeskUI();
  ASSERT_TRUE(controller->CanCreateDesks());
  ASSERT_TRUE(GetNewDeskButtonEnabledState(Shell::GetPrimaryRootWindow()));

  // Press return so that we can open the saved desk next.
  PressAndReleaseKey(ui::VKEY_RETURN);

  // Recall the desk. This should disable the new desk button again.
  SavedDeskItemView* template_item =
      GetItemViewFromSavedDeskGrid(/*grid_item_index=*/0);
  ASSERT_TRUE(template_item);
  LeftClickOn(template_item);
  ASSERT_FALSE(controller->CanCreateDesks());
  EXPECT_FALSE(GetNewDeskButtonEnabledState(Shell::GetPrimaryRootWindow()));
}

// Tests that we can not save an empty desk as a template. Regression test for
// https://crbug.com/1351520.
TEST_F(SavedDeskTest, NoEmptyDeskTemplate) {
  // Try to save a template for the current desk without having any windows.
  ToggleOverview();
  auto* overview_session = GetOverviewSession();
  ASSERT_TRUE(overview_session);
  overview_session->saved_desk_presenter()->MaybeSaveActiveDeskAsSavedDesk(
      DeskTemplateType::kTemplate, Shell::GetPrimaryRootWindow());

  // Ensure there are no templates.
  EXPECT_EQ(0u, desk_model()->GetEntryCount());
}

// Tests that you can't save the same desk more than once at a time by spamming
// the save desk as template or save desk for later buttons.
TEST_F(SavedDeskTest, SpamClickSaveDeskButtons) {
  if (features::IsSavedDeskUiRevampEnabled()) {
    GTEST_SKIP()
        << "Save desk buttons have been moved to the desk context menu.";
  }

  // Add a window.
  auto test_window = CreateAppWindow();

  // Enter overview.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  // Click the save desk as template button 5 times.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  SavedDeskSaveDeskButton* save_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root);
  ASSERT_TRUE(save_template_button);
  SpamLeftClickOn(save_template_button);
  WaitForSavedDeskUI();
  WaitForSavedDeskLibrary();

  // Ensure there is only 1 template, from the first of the 5 clicks.
  OverviewGrid* overview_grid = GetOverviewGridList().front().get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  EXPECT_EQ(1u, GetItemViewsFromDeskLibrary(overview_grid).size());

  // Leave and re-enter overview.
  ToggleOverview();
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  // Release the window because saving the desk for later will close it.
  test_window.release();

  // Click the save desk for later button 5 times.
  SavedDeskSaveDeskButton* save_desk_button =
      GetSaveDeskForLaterButtonForRoot(root);
  ASSERT_TRUE(save_desk_button);
  SpamLeftClickOn(save_desk_button);
  // Wait an extra time like in `OpenOverviewAndSaveDeskForLater` to wait for
  // the WindowCloseObserver watcher that handles blocking dialogs.
  WaitForSavedDeskUI();
  WaitForSavedDeskUI();

  // Ensure there are only 2 templates: one from the first of the 5 clicks of
  // the save desk as template button and one from the first of the 5 clicks of
  // the save desk for later button.
  OverviewGrid* overview_grid2 = GetOverviewGridList().front().get();
  const std::vector<SavedDeskItemView*> grid_items2 =
      GetItemViewsFromDeskLibrary(overview_grid2);
  EXPECT_EQ(2u, GetItemViewsFromDeskLibrary(overview_grid2).size());
}

// Tests that when saving a desk with windows owned by other user accounts, we
// only save the windows that are owned by the current active user.
TEST_F(SavedDeskTest, SaveDeskFilterByAccountID) {
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window_1 = CreateAppWindow();
  auto test_window_2 = CreateAppWindow();
  const int win_2_id = test_window_2->GetId();
  // Change the owner of `test_window_2` to be another user and set the
  // visibility so that the current user can see the window.
  AccountId account_id_A = AccountId::FromUserEmail("a");
  multi_user_window_manager()->SetWindowOwner(test_window_2.get(),
                                              account_id_A);
  multi_user_window_manager()->ShowWindowForUser(
      test_window_2.get(), multi_user_window_manager()->CurrentAccountId());
  //  Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());
  const auto* app_restore_data =
      QueryRestoreData(*GetAllEntries()[0], {}, win_2_id);
  EXPECT_FALSE(app_restore_data);
}

// Tests that if we tab while the saved desks library is fading out, there is no
// crash. Regression test for http://b/302708219.
TEST_F(SavedDeskTest, TabbingDuringExitAnimation) {
  ui::ScopedAnimationDurationScaleMode scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Ensure we have a desk saved so we can go into the library.
  AddEntry(base::Uuid::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kSaveAndRecall);

  ToggleOverview();
  WaitForOverviewEnterAnimation();

  // Enter the desks library.
  ShowSavedDeskLibrary();
  WaitForSavedDeskLibrary();

  // Exit overview. This will fade out the saved desks library.
  ToggleOverview();

  // Try tab focus traversal while the animation is in progress. There should be
  // no crash.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
}

TEST_F(SavedDeskTest, SaveDeskFilterByProfileID) {
  // Disable max limit for testing. This is needed since the max limit for
  // floating workspace templates is 0.
  desks_storage::LocalDeskDataManager::SetDisableMaxTemplateLimitForTesting(
      true);
  desks_storage::LocalDeskDataManager* local_desk_data_manager =
      static_cast<desks_storage::LocalDeskDataManager*>(desk_model());
  local_desk_data_manager->SetupFloatingWorkspaceForTest();
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  uint64_t lacros_profile_id = 1001;

  // Adds a dummy lacros profiles to the test delegate.
  LacrosProfileSummary summary;
  summary.profile_id = lacros_profile_id;
  summary.name = u"lacros_user";
  summary.email = u"lacros_user@gmail.com";
  TestDeskProfilesDelegate* desk_profile_delegate =
      static_cast<TestDeskProfilesDelegate*>(
          Shell::Get()->GetDeskProfilesDelegate());
  desk_profile_delegate->UpdateTestProfile(std::move(summary));
  desk_profile_delegate->SetPrimaryProfileByProfileId(lacros_profile_id);
  auto test_window_1 = CreateAppWindow();
  auto test_window_2 = CreateAppWindow();
  const int win_2_id = test_window_2->GetId();
  // Change the profile id of `test_window_2` to be another profile id and set
  // the profile id of `test_window_1` to be the lacros primary id.

  test_window_1->SetProperty(kLacrosProfileId,
                             desk_profile_delegate->GetPrimaryProfileId());
  test_window_2->SetProperty(kLacrosProfileId,
                             desk_profile_delegate->GetPrimaryProfileId() + 1);
  // Open overview and save a floating workspace template.
  ToggleOverview();
  auto* overview_session = GetOverviewSession();
  ASSERT_TRUE(overview_session);
  overview_session->saved_desk_presenter()->MaybeSaveActiveDeskAsSavedDesk(
      DeskTemplateType::kFloatingWorkspace, Shell::GetPrimaryRootWindow());

  ASSERT_EQ(1ul, GetAllEntries().size());
  const auto* app_restore_data =
      QueryRestoreData(*GetAllEntries()[0], {}, win_2_id);
  EXPECT_FALSE(app_restore_data);
}

// Tests that we can enter tablet mode while in overview during a guest session
// without crashing. Regression test for http://b/328708800.
TEST_F(SavedDeskTest, NoCrashDuringGuest) {
  SimulateGuestLogin();
  ToggleOverview();
  EnterTabletMode();
}

class ForestSavedDeskTest : public SavedDeskTest {
 public:
  ForestSavedDeskTest() {
    forest_feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kSavedDeskUiRevamp}, {});
  }
  ForestSavedDeskTest(const ForestSavedDeskTest&) = delete;
  ForestSavedDeskTest& operator=(const ForestSavedDeskTest&) = delete;
  ~ForestSavedDeskTest() override = default;

  void TearDown() override {
    // Due to the nested ScopedFeatureLists, `scoped_feature_list_` must be
    // reset before `this` is destroyed and `forest_feature_list_` is reset.
    scoped_feature_list_.Reset();
    SavedDeskTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList forest_feature_list_;
};

// Tests that the layout of the desk mini view context menu is correct, and the
// items are enabled and visible.
TEST_F(ForestSavedDeskTest, ContextMenuLayout) {
  // Add a window and an empty desk.
  auto test_window = CreateAppWindow();
  NewDesk();

  // Enter overview. There should be two mini views, one to represent the desk
  // with the window, and one to represent the empty desk.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_EQ(2u, GetPrimaryRootDesksBarView()->mini_views().size());

  // If there are no windows, there should only be 1 option in the context menu.
  EXPECT_EQ(1u, DesksTestApi::GetContextMenuModelForDesk(
                    DeskBarViewBase::Type::kOverview, 1)
                    .GetItemCount());

  const ui::SimpleMenuModel& menu_model =
      DesksTestApi::GetContextMenuModelForDesk(DeskBarViewBase::Type::kOverview,
                                               0);
  EXPECT_EQ(4u, menu_model.GetItemCount());

  DeskActionContextMenu::CommandId expected_command[] = {
      DeskActionContextMenu::CommandId::kSaveAsTemplate,
      DeskActionContextMenu::CommandId::kSaveForLater,
      DeskActionContextMenu::CommandId::kCombineDesks,
      DeskActionContextMenu::CommandId::kCloseAll};
  for (size_t i = 0; i < 4u; ++i) {
    EXPECT_EQ(expected_command[i],
              static_cast<DeskActionContextMenu::CommandId>(
                  menu_model.GetCommandIdAt(i)));
    EXPECT_TRUE(menu_model.IsEnabledAt(i));
    EXPECT_TRUE(menu_model.IsVisibleAt(i));
  }
}

// Tests that the context menu button in the desk action view is visible under
// the right conditions.
TEST_F(ForestSavedDeskTest, ContextMenuButtonVisibility) {
  // Add a window and an empty desk.
  auto test_window = CreateAppWindow();
  NewDesk();

  // Enter overview. There should be two mini views, one to represent the desk
  // with the window, and one to represent the empty desk.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_EQ(2u, GetPrimaryRootDesksBarView()->mini_views().size());

  // The context menu button should be visible on the first desk as it contains
  // a window.
  DeskMiniView* window_mini_view =
      GetPrimaryRootDesksBarView()->mini_views()[0];
  auto* context_menu_button =
      window_mini_view->desk_action_view()->context_menu_button();
  ASSERT_TRUE(context_menu_button);
  EXPECT_TRUE(context_menu_button->GetVisible());
  EXPECT_FALSE(window_mini_view->desk_action_view()->combine_desks_button());

  // The context menu button should not be visible on the second desk as it does
  // not contain a window.
  DeskMiniView* empty_mini_view = GetPrimaryRootDesksBarView()->mini_views()[1];
  context_menu_button =
      empty_mini_view->desk_action_view()->context_menu_button();
  ASSERT_TRUE(context_menu_button);
  EXPECT_FALSE(context_menu_button->GetVisible());
  EXPECT_FALSE(empty_mini_view->desk_action_view()->combine_desks_button());
}

// Tests that the "Save As Template" option in the desk mini view context menu
// works as intended.
TEST_F(ForestSavedDeskTest, ContextMenuSaveAsTemplate) {
  // Add a window and an empty desk.
  auto test_window = CreateAppWindow();
  NewDesk();

  // Enter overview.
  ToggleOverview();
  auto* overview_session = GetOverviewSession();
  ASSERT_TRUE(overview_session);

  // There should be two mini views, one to represent the desk with the window,
  // and one to represent the empty desk.
  ASSERT_EQ(2u, GetPrimaryRootDesksBarView()->mini_views().size());

  // Open the context menu and get the context menu item to save the desk as a
  // template.
  DeskMiniView* mini_view =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
          ->desks_bar_view()
          ->mini_views()[0];
  ASSERT_TRUE(mini_view);
  RightClickOn(mini_view);
  DeskActionContextMenu* menu = mini_view->context_menu();
  ASSERT_TRUE(menu);
  views::MenuItemView* menu_item = DesksTestApi::GetDeskActionContextMenuItem(
      menu, DeskActionContextMenu::CommandId::kSaveAsTemplate);
  ASSERT_TRUE(menu_item);

  // Activate the menu item.
  LeftClickOn(menu_item);
  WaitForSavedDeskUI();
  WaitForSavedDeskLibrary();

  // Clicking the save desk as template button selects the newly created saved
  // desk's name field. We can press enter or escape or click to select out of
  // it.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());
}

// Tests that the "Save For Later" option in the desk mini view context menu
// works as intended.
TEST_F(ForestSavedDeskTest, ContextMenuSaveForLater) {
  // Create a test window that we release immediately as it will be closed
  // automatically by the code under test. Also create an empty desk.
  CreateAppWindow().release();
  NewDesk();

  // Enter overview.
  ToggleOverview();
  auto* overview_session = GetOverviewSession();
  ASSERT_TRUE(overview_session);

  // There should be two mini views, one to represent the desk with the window,
  // and one to represent the empty desk.
  ASSERT_EQ(2u, GetPrimaryRootDesksBarView()->mini_views().size());

  // Open the context menu and get the context menu item to save the desk for
  // later.
  DeskMiniView* mini_view =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
          ->desks_bar_view()
          ->mini_views()[0];
  ASSERT_TRUE(mini_view);
  RightClickOn(mini_view);
  DeskActionContextMenu* menu = mini_view->context_menu();
  ASSERT_TRUE(menu);
  views::MenuItemView* menu_item = DesksTestApi::GetDeskActionContextMenuItem(
      menu, DeskActionContextMenu::CommandId::kSaveForLater);
  ASSERT_TRUE(menu_item);

  // Activate the menu item.
  LeftClickOn(menu_item);

  // Wait an extra time like in `OpenOverviewAndSaveDeskForLater` to wait for
  // the WindowCloseObserver watcher that handles blocking dialogs.
  WaitForSavedDeskUI();
  WaitForSavedDeskUI();
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingSavedDeskLibrary());
}

// Tests that the saved desk buttons are not created when the Forest feature is
// enabled.
TEST_F(ForestSavedDeskTest, SaveDeskButtonsHidden) {
  // Add a window and an empty desk.
  auto test_window = CreateAppWindow();
  NewDesk();

  // Enter overview.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  EXPECT_FALSE(
      GetSaveDeskButtonContainerForRoot(Shell::GetPrimaryRootWindow()));
}

}  // namespace ash
