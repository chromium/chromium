// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/test/test_desks_templates_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/save_desk_template_button.h"
#include "ash/wm/desks/templates/save_desk_template_button_container.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_icon_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/test/pixel_comparator.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class SavedDeskTest : public OverviewTestBase {
 public:
  SavedDeskTest() {}
  SavedDeskTest(const SavedDeskTest&) = delete;
  SavedDeskTest& operator=(const SavedDeskTest&) = delete;
  ~SavedDeskTest() override = default;

  // Adds an entry to the desks model directly without capturing a desk. Allows
  // for testing the names and times of the UI directly.
  void AddEntry(const base::GUID& uuid,
                const std::string& name,
                base::Time created_time,
                DeskTemplateType type) {
    AddEntry(uuid, name, created_time, DeskTemplateSource::kUser, type,
             std::make_unique<app_restore::RestoreData>());
  }

  void AddEntry(const base::GUID& uuid,
                const std::string& name,
                base::Time created_time,
                DeskTemplateSource source,
                DeskTemplateType type,
                std::unique_ptr<app_restore::RestoreData> restore_data) {
    auto desk_template =
        std::make_unique<DeskTemplate>(uuid, source, name, created_time, type);
    desk_template->set_desk_restore_data(std::move(restore_data));

    AddEntry(std::move(desk_template));
  }

  // Adds a captured desk entry to the desks model.
  void AddEntry(std::unique_ptr<DeskTemplate> desk_template) {
    base::RunLoop loop;
    desk_model()->AddOrUpdateEntry(
        std::move(desk_template),
        base::BindLambdaForTesting(
            [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
              EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                        status);
              loop.Quit();
            }));
    loop.Run();
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
      desks_storage::desk_test_util::AddAppIdToAppRegistryCache(
          account_id_, cache_.get(), app_id.c_str());

      for (int32_t window_id = 0; window_id < num_windows[i]; ++window_id) {
        restore_data->AddAppLaunchInfo(
            std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id));

        app_restore::WindowInfo window_info;
        window_info.activation_index =
            absl::make_optional<int32_t>(activation_index_counter++);

        restore_data->ModifyWindowInfo(app_id, window_id, window_info);
      }
    }
    return restore_data;
  }

  // Gets the current list of template entries from the desk model directly
  // without updating the UI.
  const std::vector<const DeskTemplate*> GetAllEntries() {
    auto result = desk_model()->GetAllEntries();
    EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk,
              result.status);
    return result.entries;
  }

  // Deletes an entry to the desks model directly without interacting with the
  // UI.
  void DeleteEntry(const base::GUID& uuid) {
    base::RunLoop loop;
    desk_model()->DeleteEntry(
        uuid.AsLowercaseString(),
        base::BindLambdaForTesting(
            [&](desks_storage::DeskModel::DeleteEntryStatus status) {
              loop.Quit();
            }));
    loop.Run();
  }

  views::View* GetDesksTemplatesButtonForRoot(aura::Window* root_window,
                                              bool zero_state) {
    auto* overview_session = GetOverviewSession();
    if (!overview_session)
      return nullptr;

    const auto* overview_grid =
        overview_session->GetGridWithRootWindow(root_window);

    // May be null in tablet mode.
    const auto* desks_bar_view = overview_grid->desks_bar_view();
    if (!desks_bar_view)
      return nullptr;

    if (zero_state)
      return desks_bar_view->zero_state_desks_templates_button();
    return desks_bar_view->expanded_state_desks_templates_button();
  }

  OverviewGrid* GetOverviewGridForRoot(aura::Window* root_window) {
    auto* overview_session = GetOverviewSession();
    return overview_session
               ? overview_session->GetGridWithRootWindow(root_window)
               : nullptr;
  }

  SaveDeskTemplateButton* GetSaveDeskAsTemplateButtonForRoot(
      aura::Window* root_window) {
    const auto* overview_grid = GetOverviewGridForRoot(root_window);
    DCHECK(overview_grid);
    return overview_grid->GetSaveDeskAsTemplateButton();
  }

  SaveDeskTemplateButton* GetSaveDeskForLaterButtonForRoot(
      aura::Window* root_window) {
    const auto* overview_grid = GetOverviewGridForRoot(root_window);
    DCHECK(overview_grid);
    return overview_grid->GetSaveDeskForLaterButton();
  }

  SaveDeskTemplateButtonContainer* GetSaveDeskButtonContainerForRoot(
      aura::Window* root_window) {
    const auto* overview_grid = GetOverviewGridForRoot(root_window);
    DCHECK(overview_grid);
    return overview_grid->GetSaveDeskTemplateButtonContainer();
  }

  // Shows the desks templates grid by emulating a click on the templates
  // button. It is required to have at least one entry in the desk model for the
  // button to be visible and clickable.
  void ShowDesksTemplatesGrids() {
    auto* root_window = Shell::GetPrimaryRootWindow();
    auto* zero_button =
        GetDesksTemplatesButtonForRoot(root_window, /*zero_state=*/true);
    auto* expanded_button =
        GetDesksTemplatesButtonForRoot(root_window, /*zero_state=*/false);
    ASSERT_TRUE(zero_button);
    ASSERT_TRUE(expanded_button);
    ASSERT_TRUE(zero_button->GetVisible() || expanded_button->GetVisible());

    if (zero_button->GetVisible())
      ClickOnView(zero_button);
    else
      ClickOnView(expanded_button);
  }

  // Helper function for attempting to delete a template based on its uuid. Also
  // checks if the grid item count is as expected before deleting. This function
  // assumes we are already in overview mode and viewing the desks templates
  // grid.
  void DeleteTemplate(const base::GUID uuid,
                      const size_t expected_current_item_count,
                      bool expect_template_exists = true) {
    const auto& grid_list = GetOverviewGridList();
    auto* saved_desk_library_view = grid_list[0]->GetSavedDeskLibraryView();
    ASSERT_TRUE(saved_desk_library_view);

    size_t total_item_count = 0;

    SavedDeskGridView* grid_view = nullptr;
    SavedDeskItemView* item_view = nullptr;
    for (auto* grid : saved_desk_library_view->grid_views()) {
      for (auto* item : grid->grid_items()) {
        if (SavedDeskItemViewTestApi(item).uuid() == uuid) {
          grid_view = grid;
          item_view = item;
        }
      }
      total_item_count += grid->grid_items().size();
    }

    // Check the current grid item count.
    ASSERT_EQ(expected_current_item_count, total_item_count);

    if (!expect_template_exists) {
      ASSERT_EQ(item_view, nullptr);
      return;
    }

    ASSERT_TRUE(item_view);
    ASSERT_TRUE(grid_view);

    ClickOnView(SavedDeskItemViewTestApi(item_view).delete_button());

    // Clicking on the delete button should bring up the delete dialog.
    ASSERT_TRUE(Shell::IsSystemModalWindowOpen());

    // Click the delete button on the delete dialog. Show delete dialog and
    // select accept.
    auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
    auto* dialog_delegate = dialog_controller->dialog_widget()
                                ->widget_delegate()
                                ->AsDialogDelegate();
    dialog_delegate->AcceptDialog();
    // Wait for the dialog to close.
    base::RunLoop().RunUntilIdle();
    SavedDeskGridViewTestApi(grid_view).WaitForItemMoveAnimationDone();
    SavedDeskLibraryViewTestApi(saved_desk_library_view).WaitForAnimationDone();
  }

  void WaitForLibraryUI() {
    SavedDeskLibraryView* library_view =
        GetOverviewGridList().front().get()->GetSavedDeskLibraryView();
    DCHECK(library_view);
    SavedDeskLibraryViewTestApi(library_view).WaitForAnimationDone();
  }

  // Open overview mode if we're not in overview mode yet, and then show the
  // desks templates grid.
  void OpenOverviewAndShowTemplatesGrid() {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    ShowDesksTemplatesGrids();
    WaitForLibraryUI();
  }

  void ClickOnView(const views::View* view) {
    DCHECK(view);

    const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(view_center);
    DCHECK(view->GetVisible());
    event_generator->ClickLeftButton();
  }

  void LongPressAt(const gfx::Point& point) {
    ui::TouchEvent long_press(ui::ET_GESTURE_LONG_PRESS, point,
                              base::TimeTicks::Now(),
                              ui::PointerDetails(ui::EventPointerType::kTouch));
    GetEventGenerator()->Dispatch(&long_press);
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& GetOverviewGridList() {
    auto* overview_session = GetOverviewSession();
    DCHECK(overview_session);

    return overview_session->grid_list();
  }

  OverviewHighlightableView* GetHighlightedView() {
    return GetOverviewSession()->highlight_controller()->highlighted_view();
  }

  // Opens overview mode and then clicks the save template button. This should
  // save a new desk template and open the desks templates grid.
  void OpenOverviewAndSaveTemplate(aura::Window* root) {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    auto* save_template_button = GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskButtonContainerVisible());
    ClickOnView(save_template_button);
    WaitForDesksTemplatesUI();
    WaitForLibraryUI();
    // Clicking the save template button selects the newly created template's
    // name field. We can press enter or escape or click to select out of it.
    SendKey(ui::VKEY_RETURN);
    for (auto& overview_grid : GetOverviewGridList())
      ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  }

  // Opens overview mode and then clicks the "save desk for later" button. This
  // should create a new saved desk and open the library page.
  void OpenOverviewAndSaveDeskForLater(aura::Window* root) {
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    auto* save_desk_button = GetSaveDeskForLaterButtonForRoot(root);
    ASSERT_TRUE(
        GetOverviewGridForRoot(root)->IsSaveDeskForLaterButtonVisible());
    ClickOnView(save_desk_button);
    WaitForDesksTemplatesUI();
    WaitForDesksTemplatesUI();

    // Clicking the save desk button selects the newly saved desk's name
    // field. We can press enter or escape or click to select out of it.
    SendKey(ui::VKEY_RETURN);
    for (auto& overview_grid : GetOverviewGridList())
      ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  }

  void SetDisableAppIdCheckForDeskTemplates(bool disabled) {
    Shell::Get()
        ->overview_controller()
        ->set_disable_app_id_check_for_saved_desks(disabled);
  }

  SkBitmap GetFocusRingBitmap(views::FocusRing* focus_ring) {
    gfx::Canvas canvas(focus_ring->size(), /*image_scale=*/1.0f,
                       /*is_opaque=*/false);
    focus_ring->OnPaint(&canvas);
    return canvas.GetBitmap();
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
    bounds.Inset(gfx::InsetsF(static_cast<float>(stroke_width) / 2.0f));
    canvas.DrawRoundRect(bounds, /*radius=*/bounds.height() / 2.0f,
                         paint_flags);
    return canvas.GetBitmap();
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesksTemplates, features::kEnableSavedDesks,
         features::kDesksCloseAll},
        {});
    OverviewTestBase::SetUp();

    // The `FullRestoreSaveHandler` isn't setup during tests so every window we
    // create in tests doesn't have an app id associated with it. Since these
    // windows don't have app ids, `OverviewGrid` won't consider them supported
    // windows so we need to disable the app id check during tests.
    SetDisableAppIdCheckForDeskTemplates(true);
  }

  void TearDown() override {
    SetDisableAppIdCheckForDeskTemplates(false);
    OverviewTestBase::TearDown();
  }

 protected:
  // Tests should normally create a local `ScopedAnimationDurationScaleMode`.
  // Create this object if a non zero scale mode needs to be used during test
  // tear down.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> animation_scale_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the helpers `AddEntry()` and `DeleteEntry()`, which will be used in
// different tests.
TEST_F(SavedDeskTest, AddDeleteEntry) {
  const base::GUID expected_uuid = base::GUID::GenerateRandomV4();
  const std::string expected_name = "desk name";
  base::Time expected_time = base::Time::Now();
  AddEntry(expected_uuid, expected_name, expected_time,
           DeskTemplateType::kTemplate);

  std::vector<const DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, entries.size());
  EXPECT_EQ(expected_uuid, entries[0]->uuid());
  EXPECT_EQ(base::UTF8ToUTF16(expected_name), entries[0]->template_name());
  EXPECT_EQ(expected_time, entries[0]->created_time());

  DeleteEntry(expected_uuid);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());
}

// Tests the desks templates button visibility in clamshell mode.
TEST_F(SavedDeskTest, DesksTemplatesButtonVisibilityClamshell) {
  // Helper function to verify which of the desks templates buttons are
  // currently shown.
  auto verify_button_visibilities = [this](bool zero_state_shown,
                                           bool expanded_state_shown,
                                           const std::string& trace_string) {
    SCOPED_TRACE(trace_string);
    for (auto* root_window : Shell::GetAllRootWindows()) {
      auto* zero_button =
          GetDesksTemplatesButtonForRoot(root_window, /*zero_state=*/true);
      auto* expanded_button =
          GetDesksTemplatesButtonForRoot(root_window, /*zero_state=*/false);
      ASSERT_TRUE(zero_button);
      ASSERT_TRUE(expanded_button);
      EXPECT_EQ(zero_state_shown, zero_button->GetVisible());
      EXPECT_EQ(expanded_state_shown, expanded_button->GetVisible());
    }
  };

  // The templates button should appear on all root windows.
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  // There are no entries initially, so the none of the desks templates buttons
  // are visible.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*trace_string=*/"one-desk-zero-entries");

  // Exit overview and add an entry.
  ToggleOverview();
  const base::GUID guid = base::GUID::GenerateRandomV4();
  AddEntry(guid, "template", base::Time::Now(), DeskTemplateType::kTemplate);

  // Reenter overview and verify the zero state desks templates buttons are
  // visible since there is one entry to view.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/true,
                             /*expanded_state_shown=*/false,
                             /*trace_string=*/"one-desk-one-entry");

  // Click on the templates button. It should expand the desks bar.
  ClickOnView(GetDesksTemplatesButtonForRoot(Shell::GetPrimaryRootWindow(),
                                             /*zero_state=*/true));
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/true,
                             /*trace_string=*/"expand-from-zero-state");

  // Exit overview and create a new desk.
  ToggleOverview();
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Reenter overview and verify the expanded state desks templates buttons are
  // visible since there is one entry to view.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/true,
                             /*trace_string=*/"two-desk-one-entry");

  // Exit overview and delete the entry.
  ToggleOverview();
  DeleteEntry(guid);

  // Reenter overview and verify neither of the buttons are shown.
  ToggleOverview();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*trace_string=*/"two-desk-zero-entries");
}

// Tests that the no windows widget is hidden when the desk templates grid is
// shown.
TEST_F(SavedDeskTest, NoWindowsLabelOnTemplateGridShow) {
  UpdateDisplay("400x300,400x300");

  // At least one entry is required for the templates grid to be shown.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Start overview mode. The no windows widget should be visible.
  ToggleOverview();
  auto& grid_list = GetOverviewGridList();
  ASSERT_EQ(2u, grid_list.size());
  EXPECT_TRUE(grid_list[0]->no_windows_widget());
  EXPECT_TRUE(grid_list[1]->no_windows_widget());

  // Open the desk templates grid. The no windows widget should now be hidden.
  ShowDesksTemplatesGrids();
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
  std::vector<const DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, desk_model()->GetEntryCount());

  // Close the window and enter overview mode. The no windows widget should be
  // shown.
  test_window.reset();
  ToggleOverview();
  EXPECT_TRUE(GetOverviewGridList()[0]->no_windows_widget());

  // Open the desk templates grid. The no windows widget should now be hidden
  // and the no items label in the library UI should also not be visible.
  ShowDesksTemplatesGrids();
  EXPECT_FALSE(GetOverviewGridList()[0]->no_windows_widget());
  EXPECT_FALSE(SavedDeskLibraryViewTestApi(
                   GetOverviewGridList()[0]->GetSavedDeskLibraryView())
                   .no_items_label()
                   ->GetVisible());

  // Delete the one and only template, which should hide the templates grid but
  // remain in overview. Check that the no windows widget is now back.
  OpenOverviewAndShowTemplatesGrid();
  DeleteTemplate(entries[0]->uuid(), /*expected_current_item_count=*/1);

  ASSERT_TRUE(InOverviewSession());
  // We should be in the saved desk UI and the no items label should now be
  // visible.
  EXPECT_TRUE(SavedDeskLibraryViewTestApi(
                  GetOverviewGridList()[0]->GetSavedDeskLibraryView())
                  .no_items_label()
                  ->GetVisible());
}

// Tests that the "App does not support split-screen" label is hidden when the
// desk templates grid is shown.
TEST_F(SavedDeskTest, NoAppSplitScreenLabelOnTemplateGridShow) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  auto test_window = CreateAppWindow();

  // At least one entry is required for the templates grid to be shown.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Start overview mode.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  OverviewItem* snappable_overview_item =
      GetOverviewItemForWindow(test_window.get());
  OverviewItem* unsnappable_overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());

  // Note: `cannot_snap_widget_` will be created on demand.
  EXPECT_FALSE(snappable_overview_item->cannot_snap_widget_for_testing());
  ASSERT_FALSE(unsnappable_overview_item->cannot_snap_widget_for_testing());

  // Snap the extra snappable window to enter split view mode.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());

  split_view_controller->SnapWindow(test_window.get(),
                                    SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller->InSplitViewMode());
  ASSERT_TRUE(unsnappable_overview_item->cannot_snap_widget_for_testing());
  ui::Layer* unsnappable_layer =
      unsnappable_overview_item->cannot_snap_widget_for_testing()->GetLayer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Entering the templates grid will hide the unsnappable label.
  ShowDesksTemplatesGrids();
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Tests when user enter desk templates, a11y alert being sent.
TEST_F(SavedDeskTest, InvokeAccessibilityAlertOnEnterDeskTemplates) {
  TestAccessibilityControllerClient client;

  // At least one entry is required for the templates grid to be shown.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Start overview mode.
  ToggleOverview();

  // Alert for entering overview mode should be sent.
  EXPECT_EQ(AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());

  // Enter desks templates
  ShowDesksTemplatesGrids();

  // Alert for entering templates should be sent.
  EXPECT_EQ(AccessibilityAlert::DESK_TEMPLATES_MODE_ENTERED,
            client.last_a11y_alert());
}

// Tests that overview items are hidden when the desk templates grid is shown.
TEST_F(SavedDeskTest, HideOverviewItemsOnTemplateGridShow) {
  UpdateDisplay("800x600,800x600");

  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto test_window = CreateAppWindow();

  // Start overview mode. The window is visible in the overview mode.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_EQ(1.0f, test_window->layer()->opacity());

  // Open the desk templates grid. This should hide the window.
  ShowDesksTemplatesGrids();
  EXPECT_EQ(0.0f, test_window->layer()->opacity());

  // Exit overview mode. The window is restored and visible again.
  ToggleOverview();
  EXPECT_EQ(1.0f, test_window->layer()->opacity());
}

// Tests that when the templates grid is shown and the active desk is closed,
// overview items stay hidden.
TEST_F(SavedDeskTest, OverviewItemsStayHiddenInTemplateGridOnDeskClose) {
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
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

  // Open the desk templates grid. This should hide `test_window_2`.
  ShowDesksTemplatesGrids();
  EXPECT_EQ(0.0f, test_window_2->layer()->opacity());

  // While in the desk templates grid, delete the active desk by clicking on the
  // mini view close button.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view =
      desks_bar_view->FindMiniViewForDesk(desks_controller->active_desk());
  ClickOnView(mini_view->desk_action_view()->combine_desks_button());

  // Expect we stay in the templates grid.
  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());

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

// Tests the modality of the dialogs shown in desks templates.
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

  // Checks that pressing tab does not trigger overview keyboard traversal.
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(
      GetOverviewSession()->highlight_controller()->IsFocusHighlightVisible());

  // Fetch the widget for the dialog and test that it appears on the primary
  // root window.
  const views::Widget* dialog_widget = dialog_controller->dialog_widget();
  ASSERT_TRUE(dialog_widget);
  EXPECT_EQ(Shell::GetPrimaryRootWindow(),
            dialog_widget->GetNativeWindow()->GetRootWindow());

  // Hit escape to delete the dialog. Tests that there are no more system modal
  // windows open, and that we are still in overview because the dialog takes
  // the escape event, not the overview session.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());
}

// Tests that the desks templates grid and item views are populated correctly.
TEST_F(SavedDeskTest, DesksTemplatesGridItems) {
  UpdateDisplay("800x600,800x600");

  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  const std::string name_1 = "template_1";
  base::Time time_1 = base::Time::Now();
  AddEntry(uuid_1, name_1, time_1, DeskTemplateType::kTemplate);

  const base::GUID uuid_2 = base::GUID::GenerateRandomV4();
  const std::string name_2 = "template_2";
  base::Time time_2 = time_1 + base::Hours(13);
  AddEntry(uuid_2, name_2, time_2, DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  // Check that the grid is populated with the correct number of items, as
  // well as with the correct name and timestamp.
  for (const auto& overview_grid : GetOverviewGridList()) {
    std::vector<SavedDeskItemView*> grid_items =
        GetItemViewsFromDeskLibrary(overview_grid.get());

    ASSERT_EQ(2ul, grid_items.size());

    // The grid item order is currently not guaranteed, so need to
    // verify that each template exists by looking them up by their
    // UUID.
    auto verify_template_grid_item = [&grid_items](const base::GUID& uuid,
                                                   const std::string& name) {
      auto iter =
          std::find_if(grid_items.cbegin(), grid_items.cend(),
                       [uuid](const SavedDeskItemView* v) {
                         return SavedDeskItemViewTestApi(v).uuid() == uuid;
                       });
      ASSERT_NE(grid_items.end(), iter);

      SavedDeskItemView* item_view = *iter;
      EXPECT_EQ(base::UTF8ToUTF16(name), item_view->name_view()->GetText());
      EXPECT_FALSE(
          SavedDeskItemViewTestApi(item_view).time_view()->GetText().empty());
    };

    verify_template_grid_item(uuid_1, name_1);
    verify_template_grid_item(uuid_2, name_2);
  }
}

// Tests that deleting templates in the templates grid functions correctly.
TEST_F(SavedDeskTest, DeleteTemplate) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  const base::GUID uuid_2 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_2, "template_2", base::Time::Now() + base::Hours(13),
           DeskTemplateType::kTemplate);

  // This window should be hidden whenever the desk templates grid is open.
  auto test_window = CreateAppWindow();

  OpenOverviewAndShowTemplatesGrid();

  // The window is hidden because the desk templates grid is open.
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(2ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_1`.
  DeleteTemplate(uuid_1, /*expected_current_item_count=*/2);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Verifies that the template with `uuid_1`, doesn't exist anymore.
  DeleteTemplate(uuid_1, /*expected_current_item_count=*/1,
                 /*expect_template_exists=*/false);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_2`.
  DeleteTemplate(uuid_2, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());
}

// Tests that the save desk button container is aligned with the first
// overview item. Regression test for https://crbug.com/1285491.
TEST_F(SavedDeskTest, SaveDeskButtonContainerAligned) {
  // Create a test window in the current desk.
  auto test_window1 = CreateAppWindow();
  auto test_window2 = CreateAppWindow();
  // A widget is needed to close.
  auto test_widget = CreateTestWidget();

  ToggleOverview();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root_window);
  SaveDeskTemplateButtonContainer* save_desk_button_container =
      GetSaveDeskButtonContainerForRoot(root_window);

  auto verify_save_desk_widget_bounds = [&overview_grid,
                                         save_desk_button_container]() {
    auto& window_list = overview_grid->window_list();
    ASSERT_FALSE(window_list.empty());
    EXPECT_EQ(
        std::round(window_list.front()->target_bounds().x()) + kWindowMargin,
        save_desk_button_container->GetBoundsInScreen().x());
    EXPECT_EQ(std::round(window_list.front()->target_bounds().y()) - 40,
              save_desk_button_container->GetBoundsInScreen().y());
    EXPECT_EQ(16, save_desk_button_container->GetBetweenChildSpacing());
  };

  verify_save_desk_widget_bounds();

  // Tests that the save desk button remains slightly above the first overview
  // item after changes to the window position. Regression test for
  // https://crbug.com/1289020.

  // Delete an overview item and verify.
  OverviewItem* item = GetOverviewItemForWindow(test_widget->GetNativeWindow());
  item->CloseWindow();

  // `NativeWidgetAura::Close()` fires a post task.
  base::RunLoop().RunUntilIdle();
  verify_save_desk_widget_bounds();

  // Create a new desk to leave zero state and verify.
  const DesksBarView* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view->IsZeroState());
  auto* new_desk_button = desks_bar_view->zero_state_new_desk_button();
  ClickOnView(new_desk_button);
  ASSERT_FALSE(desks_bar_view->IsZeroState());
  verify_save_desk_widget_bounds();
}

// Tests that the color of save desk button focus ring is as expected.
TEST_F(SavedDeskTest, SaveDeskButtonFocusRingColor) {
  // Create a test window in the current desk.
  auto test_window = CreateAppWindow();

  ToggleOverview();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* save_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root_window);
  auto* save_for_later_button = GetSaveDeskForLaterButtonForRoot(root_window);

  // Verify the focus ring of the given button is as expected.
  auto verify_button_focus_ring_color = [this](SaveDeskTemplateButton* button,
                                               bool highlighted) {
    EXPECT_EQ(
        cc::ExactPixelComparator(/*discard_alpha=*/false)
            .Compare(
                GetFocusRingBitmap(views::FocusRing::Get(button)),
                GetBitmapWithInnerRoundedRect(
                    views::FocusRing::Get(button)->size(),
                    /*stroke_width=*/2,
                    ColorProvider::Get()->GetControlsLayerColor(
                        ColorProvider::ControlsLayerType::kFocusRingColor))),
        highlighted);
  };

  // Both buttons are not highlighted.
  ASSERT_FALSE(save_as_template_button->IsViewHighlighted());
  ASSERT_FALSE(save_for_later_button->IsViewHighlighted());
  verify_button_focus_ring_color(save_as_template_button, false);
  verify_button_focus_ring_color(save_for_later_button, false);

  // Reverse tab, then `Save desk for later` button is highlighted.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(save_as_template_button->IsViewHighlighted());
  ASSERT_TRUE(save_for_later_button->IsViewHighlighted());
  verify_button_focus_ring_color(save_as_template_button, false);
  verify_button_focus_ring_color(save_for_later_button, true);

  // Reverse tab, then `Save desk as a template` button is highlighted.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(save_as_template_button->IsViewHighlighted());
  ASSERT_FALSE(save_for_later_button->IsViewHighlighted());
  verify_button_focus_ring_color(save_as_template_button, true);
  verify_button_focus_ring_color(save_for_later_button, false);

  // Reverse tab, then both buttons are not highlighted.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_FALSE(save_as_template_button->IsViewHighlighted());
  ASSERT_FALSE(save_for_later_button->IsViewHighlighted());
  verify_button_focus_ring_color(save_as_template_button, false);
  verify_button_focus_ring_color(save_for_later_button, false);
}

// Tests that the save desk as template button and save for later button are
// enabled and disabled as expected based on the number of templates.
TEST_F(SavedDeskTest, SaveDeskButtonsEnabledDisabled) {
  // Prepare the test environment, like creating an app window which should be
  // supported.
  auto no_app_id_window = CreateAppWindow();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* delegate = Shell::Get()->desks_templates_delegate();
  ASSERT_TRUE(
      delegate->IsWindowSupportedForDeskTemplate(no_app_id_window.get()));

  // Test `Save Desk as Template` button.
  {
    // Add 6 `kTemplate` entries.
    for (size_t i = 1; i <= 6; i++) {
      const std::string desk_name = "desk_template " + base::NumberToString(i);
      AddEntry(base::GUID::GenerateRandomV4(), desk_name, base::Time::Now(),
               DeskTemplateType::kTemplate);
    }

    // Open overview and expect the button to be disabled.
    ToggleOverview();
    EXPECT_EQ(views::Button::STATE_DISABLED,
              GetSaveDeskAsTemplateButtonForRoot(root_window)->GetState());

    // Exit and reopen overview, then verify that the entry count reaches the
    // maximum.
    ToggleOverview();
    OpenOverviewAndShowTemplatesGrid();
    const SavedDeskPresenter* saved_desk_presenter =
        saved_desk_util::GetSavedDeskPresenter();
    ASSERT_EQ(
        saved_desk_presenter->GetMaxEntryCount(DeskTemplateType::kTemplate),
        saved_desk_presenter->GetEntryCount(DeskTemplateType::kTemplate));

    // Verify that the button is re-enabled after we delete all entries.
    std::vector<const DeskTemplate*> entries = GetAllEntries();
    for (size_t i = entries.size(); i > 0; i--) {
      DeleteTemplate(/*uuid=*/entries[i - 1]->uuid(),
                     /*expected_current_item_count=*/i);
    }
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());

    // Leave and re-enter overview.
    ToggleOverview();
    ToggleOverview();

    EXPECT_FALSE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
    EXPECT_EQ(views::Button::STATE_NORMAL,
              GetSaveDeskAsTemplateButtonForRoot(root_window)->GetState());

    // Exit overview.
    ToggleOverview();
  }

  // Test `Save Desk for Later` button.
  {
    // Add 6 `kSaveAndRecall` entries.
    for (size_t i = 1; i <= 6; i++) {
      const std::string desk_name = "saved_desk " + base::NumberToString(i);
      AddEntry(base::GUID::GenerateRandomV4(), desk_name, base::Time::Now(),
               DeskTemplateType::kSaveAndRecall);
    }

    // Open overview and expect the button to be disabled.
    ToggleOverview();
    EXPECT_EQ(views::Button::STATE_DISABLED,
              GetSaveDeskForLaterButtonForRoot(root_window)->GetState());

    // Exit and reopen overview, then verify that the entry count reaches the
    // maximum.
    ToggleOverview();
    OpenOverviewAndShowTemplatesGrid();
    const SavedDeskPresenter* saved_desk_presenter =
        saved_desk_util::GetSavedDeskPresenter();
    ASSERT_EQ(
        saved_desk_presenter->GetMaxEntryCount(
            DeskTemplateType::kSaveAndRecall),
        saved_desk_presenter->GetEntryCount(DeskTemplateType::kSaveAndRecall));

    // Verify that the button is re-enabled after we delete all entries.
    std::vector<const DeskTemplate*> entries = GetAllEntries();
    for (size_t i = entries.size(); i > 0; i--) {
      DeleteTemplate(/*uuid=*/entries[i - 1]->uuid(),
                     /*expected_current_item_count=*/i);
    }
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());

    // Leave and re-enter overview.
    ToggleOverview();
    ToggleOverview();

    EXPECT_FALSE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
    EXPECT_EQ(views::Button::STATE_NORMAL,
              GetSaveDeskForLaterButtonForRoot(root_window)->GetState());

    // Exit overview.
    ToggleOverview();
  }
}

// Tests that pressing `Enter` on save desk buttons does not save anything when
// disabled.
TEST_F(SavedDeskTest, SaveDeskButtonsPressEnterWhenDisabled) {
  // Prepare the test environment, like creating an app window which should be
  // supported.
  auto no_app_id_window = CreateAppWindow();
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* delegate = Shell::Get()->desks_templates_delegate();
  ASSERT_TRUE(
      delegate->IsWindowSupportedForDeskTemplate(no_app_id_window.get()));

  // Toggle overview and set the save desk buttons to disabled.
  ToggleOverview();
  auto* save_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root_window);
  auto* save_for_later_button = GetSaveDeskForLaterButtonForRoot(root_window);
  save_as_template_button->SetEnabled(false);
  save_for_later_button->SetEnabled(false);
  ASSERT_FALSE(save_as_template_button->GetEnabled());
  ASSERT_FALSE(save_for_later_button->GetEnabled());

  // Press `Enter` when `Save desk for later` button is highlighted.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(save_for_later_button->IsViewHighlighted());
  ASSERT_FALSE(save_for_later_button->GetEnabled());
  SendKey(ui::VKEY_RETURN);
  SavedDeskPresenterTestApi(
      GetOverviewGridList()[0]->overview_session()->saved_desk_presenter())
      .MaybeWaitForModel();
  ASSERT_FALSE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());

  // Press `Enter` when `Save desk as a template` button is highlighted.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(save_as_template_button->IsViewHighlighted());
  ASSERT_FALSE(save_as_template_button->GetEnabled());
  SendKey(ui::VKEY_RETURN);
  SavedDeskPresenterTestApi(
      GetOverviewGridList()[0]->overview_session()->saved_desk_presenter())
      .MaybeWaitForModel();
  ASSERT_FALSE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
}

// Tests that clicking the save desk as template button shows the templates
// grid.
TEST_F(SavedDeskTest, SaveDeskAsTemplateButtonShowsDesksTemplatesGrid) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // The `save_desk_as_template_button` is visible when at least one window is
  // open.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_button);
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->IsSaveDeskAsTemplateButtonVisible());

  // Click on `save_desk_as_template_button`.
  ClickOnView(save_desk_as_template_button);
  ASSERT_EQ(1ul, GetAllEntries().size());

  WaitForDesksTemplatesUI();

  // Expect that the Desk Templates grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
}

// SaveDeskButtonContainerVisibilityObserver waits for the save desk buttons on
// the observed root window to become visible.
class SaveDeskButtonContainerVisibilityObserver : public aura::WindowObserver {
 public:
  explicit SaveDeskButtonContainerVisibilityObserver(aura::Window* root_window)
      : root_window_(root_window) {
    root_window_->AddObserver(this);
  }
  SaveDeskButtonContainerVisibilityObserver(
      const SaveDeskButtonContainerVisibilityObserver&) = delete;
  SaveDeskButtonContainerVisibilityObserver& operator=(
      const SaveDeskButtonContainerVisibilityObserver&) = delete;
  ~SaveDeskButtonContainerVisibilityObserver() override {
    DCHECK(root_window_);
    root_window_->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible && window->GetId() == kShellWindowId_SaveDeskButtonContainer)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<aura::Window> root_window_;
};

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

  SaveDeskButtonContainerVisibilityObserver button_container_observer(
      root_window);
  EnterOverview();
  button_container_observer.Wait();

  // Ensure we are in overview.
  auto* overview_controller = Shell::Get()->overview_controller();
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Check to see that the desks bar has been created. Previously, there was a
  // crash caused by the save desk buttons being clicked before the desks bar
  // was created and initialized.
  const auto* desks_bar_view =
      GetOverviewGridForRoot(root_window)->desks_bar_view();
  ASSERT_NE(desks_bar_view, nullptr);

  // Click on the button to save a desk. We should transition into the desk
  // library and there should be no crash.
  auto* save_for_later_button = GetSaveDeskForLaterButtonForRoot(root_window);
  ClickOnView(save_for_later_button);
  WaitForDesksTemplatesUI();
  WaitForDesksTemplatesUI();
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
}

// Tests that saving a template nudges the correct name view.
TEST_F(SavedDeskTest, SaveTemplateNudgesNameView) {
  // Other templates were added earlier.
  AddEntry(base::GUID::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateAppWindow();

  // Capture the current desk as a template, which is by default named "Desk 1".
  // We open overview and save template without clicking out of the newly
  // created template name view.
  ToggleOverview();
  ClickOnView(
      GetSaveDeskAsTemplateButtonForRoot(Shell::Get()->GetPrimaryRootWindow()));
  WaitForDesksTemplatesUI();
  ASSERT_EQ(3ul, GetAllEntries().size());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Expect that the last added template item name view has focus.
  EXPECT_TRUE(overview_grid->IsTemplateNameBeingModified());
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
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Launching a template creates and activates a new desk without exiting
  // overview mode, so we check that we're still in overview.
  EXPECT_TRUE(InOverviewSession());

  // This section tests clicking on the "Use template" button to launch the
  // template.
  ToggleOverview();
  OpenOverviewAndShowTemplatesGrid();
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  ClickOnView(SavedDeskItemViewTestApi(item_view).launch_button());

  EXPECT_EQ(3ul, desks_controller->desks().size());
  EXPECT_EQ(2, desks_controller->GetActiveDeskIndex());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that launching templates from the templates grid nudges the new desk
// name view.
TEST_F(SavedDeskTest, LaunchTemplateNudgesNewDeskName) {
  // Save an entry in the templates grid.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(1ul, desks_controller->desks().size());

  // Click on the "Use template" button to launch the template.
  OpenOverviewAndShowTemplatesGrid();
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  ClickOnView(SavedDeskItemViewTestApi(item_view).launch_button());

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

// Tests that the order of SavedDeskItemView is in order.
TEST_F(SavedDeskTest, IconsOrder) {
  // Create a `DeskTemplate` using which has 5 apps and each app has 1 window.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>(5, 1)));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The items previews should be ordered by activation index. Exclude the
  // final SavedDeskIconView since it will be the overflow counter.
  EXPECT_EQ(5u, icon_views.size());
  for (size_t i = 0; i < icon_views.size() - 2; ++i) {
    int current_id;
    ASSERT_TRUE(
        base::StringToInt(icon_views[i]->icon_identifier(), &current_id));

    int next_id;
    ASSERT_TRUE(
        base::StringToInt(icon_views[i + 1]->icon_identifier(), &next_id));

    EXPECT_TRUE(current_id < next_id);
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
  app_launch_info_1->active_tab_index = 1;
  app_launch_info_1->urls = kTabs1;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));

  // Add app launch info for the lacros browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->active_tab_index = 1;
  app_launch_info_2->urls = kTabs2;
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));

  // A non empty activation index is assumed by the icon placing logic.
  app_restore::WindowInfo window_info;
  window_info.activation_index = 0;
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info);

  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowTemplatesGrid();

  // Test that there is a total of 4 icons, one for each tab on each browser.
  // There is also the overflow icon, which is created but hidden.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
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
  app_launch_info_1->active_tab_index = kActiveTabIndex1;
  app_launch_info_1->urls = absl::make_optional(kTabs1);
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));
  app_restore::WindowInfo window_info_1;
  window_info_1.activation_index = absl::make_optional<int32_t>(kWindowId1);
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info_1);

  // Add app launch info for the second browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->active_tab_index = kActiveTabIndex2;
  app_launch_info_2->urls = absl::make_optional(kTabs2);
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));
  app_restore::WindowInfo window_info_2;
  window_info_2.activation_index = absl::make_optional<int32_t>(kWindowId2);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info_2);

  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // Check the icon views. The first two items should be the active tabs,
  // ordered by activation index. The next two items should be the inactive tabs
  // with the lowest activation indices, i.e. the rest of the tabs from the
  // first browser instance.
  ASSERT_EQ(5u, icon_views.size());
  EXPECT_EQ(kTabs1[kActiveTabIndex1].spec(), icon_views[0]->icon_identifier());
  EXPECT_EQ(kTabs2[kActiveTabIndex2].spec(), icon_views[1]->icon_identifier());
  EXPECT_EQ(kTabs1[0].spec(), icon_views[2]->icon_identifier());
  EXPECT_EQ(kTabs1[2].spec(), icon_views[3]->icon_identifier());
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
  app_launch_info->active_tab_index = kActiveTabIndex;
  app_launch_info->urls = absl::make_optional(kTabs);
  restore_data->AddAppLaunchInfo(std::move(app_launch_info));
  app_restore::WindowInfo window_info;
  window_info.activation_index = absl::make_optional<int32_t>(kWindowId);
  restore_data->ModifyWindowInfo(kAppId, kWindowId, window_info);

  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           std::move(restore_data));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should be one icon view for both the urls, and another icon view for
  // the overflow icon.
  ASSERT_EQ(2u, icon_views.size());
  // The first icon view should have the first url including the query parameter
  // as its identifier, and have a count of 2 because its representing both
  // urls.
  EXPECT_EQ(kTabs[0].spec(), icon_views[0]->icon_identifier());
  EXPECT_EQ(2, icon_views[0]->count());
  // The second icon view should have a count of 0, because there are no
  // overflow windows.
  EXPECT_EQ(0, icon_views[1]->count());
}

// Tests that the overflow count view is visible, in bounds, displays the right
// count when there is more than `SavedDeskIconContainer::kMaxIcons` icons.
TEST_F(SavedDeskTest, OverflowIconView) {
  // Create a `DeskTemplate` using which has 1 app more than the max and each
  // app has 1 window.
  const int kNumOverflowApps = 1;
  std::vector<int> window_info(
      kNumOverflowApps + SavedDeskIconContainer::kMaxIcons, 1);
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should only be the max number of icons plus the overflow icon.
  EXPECT_EQ(SavedDeskIconContainer::kMaxIcons + 1,
            static_cast<int>(icon_views.size()));

  // The overflow counter should have no identifier and its count should be
  // non-zero. It should also be visible and within the bounds of the host
  // SavedDeskItemView.
  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+1", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.desks_templates_icon_view()->GetVisible());
  EXPECT_TRUE(
      item_view->Contains(overflow_icon_view.desks_templates_icon_view()));
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
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // Even though there are more than `SavedDeskIconContainer::kMaxIcons`,
  // there should still be `SavedDeskIconContainer::kMaxIcons`+ 1
  // SavedDeskIconView's created.
  EXPECT_EQ(icon_views.size(), SavedDeskIconContainer::kMaxIcons + 1u);

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
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());

  // (3 + 4) * 2 = 14 windows were added to the desk template, from 7 apps with
  // 2 windows each. The first 4 icon views are created, each with a "+1" count
  // label. The last 2 of the 4 views did not fit, so they were made not visible
  // and were added to the overflow count. With 2 of the 7 apps being displayed,
  // having 2 windows each, the overflow count should be 14 - (2 * 2) = 10.
  EXPECT_EQ(u"+10", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.desks_templates_icon_view()->GetVisible());
  EXPECT_TRUE(
      item_view->Contains(overflow_icon_view.desks_templates_icon_view()));
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
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>{1, 1, 2, 2, 3}));

  // Enter overview and show the Desks Templates Grid.
  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should be 1 * 2 icon views for the 2 apps with 1 window, 2 * 2 icon
  // views for the 2 apps with multiple windows, and 1 overflow icon view.
  EXPECT_EQ(5u, icon_views.size());

  // Verify each of the apps' count labels are correct.
  SavedDeskIconViewTestApi icon_view_1(icon_views[0]);
  EXPECT_TRUE(icon_view_1.icon_view());
  EXPECT_FALSE(icon_view_1.count_label());

  SavedDeskIconViewTestApi icon_view_2(icon_views[1]);
  EXPECT_TRUE(icon_view_2.icon_view());
  EXPECT_FALSE(icon_view_2.count_label());

  SavedDeskIconViewTestApi icon_view_3(icon_views[2]);
  EXPECT_TRUE(icon_view_3.icon_view());
  EXPECT_TRUE(icon_view_3.count_label());
  EXPECT_EQ(u"+1", icon_view_3.count_label()->GetText());

  SavedDeskIconViewTestApi icon_view_4(icon_views[3]);
  EXPECT_TRUE(icon_view_4.icon_view());
  EXPECT_TRUE(icon_view_4.count_label());
  EXPECT_EQ(u"+1", icon_view_4.count_label()->GetText());

  // The overflow counter should display the number of excess windows.
  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+5", overflow_icon_view.count_label()->GetText());
}

// Tests that when an app has more than 99 windows, its label is changed to
// "+99".
TEST_F(SavedDeskTest, IconViewMoreThan99Windows) {
  // Create a `DeskTemplate` using which has 1 app with 101 windows.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(std::vector<int>{101}));

  // Enter overview and show the Desks Templates Grid.
  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // There should only be 1 icon view for the app and 1 icon view for the
  // overflow.
  EXPECT_EQ(2u, icon_views.size());

  // The app's icon view should have a "+99" label.
  SavedDeskIconViewTestApi icon_view(icon_views[0]);
  EXPECT_TRUE(icon_view.icon_view());
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
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
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
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // `CreateRestoreData` creates the windows with app ids of "0", "1", "2", etc.
  // Set 2 of those app ids to be unavailable.
  auto* delegate = static_cast<TestDesksTemplatesDelegate*>(
      Shell::Get()->desks_templates_delegate());
  delegate->set_unavailable_apps({"0", "1"});

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The 2 available app icons should be visible, and the overflow icon should
  // contain the hidden (0) + unavailable (2) app counts.
  EXPECT_EQ(3u, icon_views.size());

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+2", overflow_icon_view.count_label()->GetText());
}

// Test that the overflow icon counts unavailable icons when there are more than
// kMaxIcons visible in the container, and hidden icons are also added.
TEST_F(SavedDeskTest, OverflowUnavailableMoreThan5Icons) {
  // Create a `DeskTemplate` which has 8 apps and each app has 1 window. Set 2
  // of those app ids to be unavailable.
  std::vector<int> window_info(8, 1);
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // `CreateRestoreData` creates the windows with app ids of "0", "1", "2", etc.
  // Set 2 of those app ids to be unavailable.
  auto* delegate = static_cast<TestDesksTemplatesDelegate*>(
      Shell::Get()->desks_templates_delegate());
  delegate->set_unavailable_apps({"0", "1"});

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The 4 available app icons should be visible, and the overflow icon should
  // contain the hidden (2) + unavailable (2) app counts.
  EXPECT_EQ(5u, icon_views.size());

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+4", overflow_icon_view.count_label()->GetText());
}

// Test that the overflow icon displays the count without a plus when all icons
// are unavailable.
TEST_F(SavedDeskTest, OverflowUnavailableAllUnavailableIcons) {
  // Create a `DeskTemplate` which has 10 apps and each app has 1 window.
  std::vector<int> window_info(10, 1);
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kUser, DeskTemplateType::kTemplate,
           CreateRestoreData(window_info));

  // Set all 10 app ids to be unavailable.
  auto* delegate = static_cast<TestDesksTemplatesDelegate*>(
      Shell::Get()->desks_templates_delegate());
  delegate->set_unavailable_apps(
      {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"});

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<SavedDeskIconView*>& icon_views =
      SavedDeskItemViewTestApi(item_view).GetIconViews();

  // The only added icon view is the overflow icon, and it should have a "10"
  // label without the plus sign.
  EXPECT_EQ(1u, icon_views.size());

  SavedDeskIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
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
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  EnterTabletMode();

  // Test that the templates buttons are created but invisible. The save desk as
  // template button is not created.
  ToggleOverview();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* zero_state = GetDesksTemplatesButtonForRoot(root,
                                                    /*zero_state=*/true);
  auto* expanded_state = GetDesksTemplatesButtonForRoot(root,
                                                        /*zero_state=*/false);
  EXPECT_FALSE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_FALSE(GetSaveDeskButtonContainerForRoot(root));
}

// Tests that the desks templates and save desk template buttons are hidden when
// transitioning from clamshell to tablet mode.
TEST_F(SavedDeskTest, ClamshellToTabletMode) {
  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Test that on entering overview, the zero state desks templates button and
  // the save template button are visible.
  ToggleOverview();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* zero_state = GetDesksTemplatesButtonForRoot(root,
                                                    /*zero_state=*/true);
  auto* expanded_state = GetDesksTemplatesButtonForRoot(root,
                                                        /*zero_state=*/false);
  EXPECT_TRUE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_TRUE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());

  // Tests that after transitioning, we remain in overview mode and all the
  // buttons are invisible.
  EnterTabletMode();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_FALSE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
}

// Tests that the saved desk library gets hidden when transitioning to tablet
// mode.
TEST_F(SavedDeskTest, ShowingSavedDeskLibraryToTabletMode) {
  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ASSERT_TRUE(GetOverviewSession()
                  ->GetGridWithRootWindow(root_window)
                  ->saved_desk_library_widget()
                  ->IsVisible());

  // Tests that the templates button is in expanded state when the grid is
  // showing, even with one desk.
  auto* zero_state = GetDesksTemplatesButtonForRoot(root_window,
                                                    /*zero_state=*/true);
  auto* expanded_state = GetDesksTemplatesButtonForRoot(root_window,
                                                        /*zero_state=*/false);
  ASSERT_FALSE(zero_state->GetVisible());
  ASSERT_TRUE(expanded_state->GetVisible());

  // Tests that after transitioning, we remain in overview mode and the grid is
  // hidden.
  EnterTabletMode();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(GetOverviewSession()
                   ->GetGridWithRootWindow(root_window)
                   ->saved_desk_library_widget()
                   ->IsVisible());

  // Tests that the templates button is also hidden in tablet mode. Regression
  // test for https://crbug.com/1291777.
  EXPECT_FALSE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
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
  std::vector<const DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  // Tests that after transitioning into tablet mode, the activation and focus
  // is correct and we remain in overview mode.
  EnterTabletMode();
  ASSERT_TRUE(InOverviewSession());
}

TEST_F(SavedDeskTest, OverviewTabbing) {
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskItemView* first_item = GetItemViewFromTemplatesGrid(0);
  SavedDeskItemView* second_item = GetItemViewFromTemplatesGrid(1);

  // Testing that we first traverse the views of the first item.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(first_item, GetHighlightedView());

  // Testing that we traverse to the `name_view` of the first item.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(first_item->name_view(), GetHighlightedView());

  // When we're done with the first item, we'll go on to the second.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(second_item, GetHighlightedView());

  // Testing that we traverse to the `name_view` of the second item.
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(second_item->name_view(), GetHighlightedView());
}

// Tests that if the templates button is invisible, it is not part of the
// tabbing order. Regression test for https://crbug.com/1313761.
TEST_F(SavedDeskTest, TabbingInvisibleTemplatesButton) {
  // First test the case there are no templates.
  ToggleOverview();

  auto* overview_grid = GetOverviewSession()->GetGridWithRootWindow(
      Shell::GetPrimaryRootWindow());
  ZeroStateIconButton* button =
      overview_grid->desks_bar_view()->zero_state_desks_templates_button();
  ASSERT_TRUE(button);
  ASSERT_FALSE(button->GetVisible());

  // Test that we do not highlight the templates button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_NE(button, GetHighlightedView());

  // Test the case where it was visible at one point, but became invisible (last
  // template was deleted).
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());

  // Add an entry to delete later.
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);
  OpenOverviewAndShowTemplatesGrid();

  DeleteTemplate(uuid, /*expected_current_item_count=*/1);
  // `NativeWidgetAura::Close()`, which is the underlying class for a dialog
  // widget is on a post task so flush that task.
  base::RunLoop().RunUntilIdle();

  overview_grid = GetOverviewSession()->GetGridWithRootWindow(
      Shell::GetPrimaryRootWindow());
  button = overview_grid->desks_bar_view()->zero_state_desks_templates_button();
  ASSERT_TRUE(button);
  ASSERT_FALSE(button->GetVisible());

  // Test that we do not highlight the templates button.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_NE(button, GetHighlightedView());
}

// Tests that the desks bar does not return to zero state if the second-to-last
// desk is deleted while viewing the templates grid.
TEST_F(SavedDeskTest, DesksBarDoesNotReturnToZeroState) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now(), DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root_window);
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Close one of the desks. Test that we remain in expanded state.
  auto* mini_view = desks_bar_view->FindMiniViewForDesk(
      DesksController::Get()->active_desk());
  ClickOnView(mini_view->desk_action_view()->close_all_button());
  auto* expanded_new_desk_button =
      desks_bar_view->expanded_state_new_desk_button();
  auto* expanded_templates_button =
      desks_bar_view->expanded_state_desks_templates_button();
  EXPECT_TRUE(expanded_new_desk_button->GetVisible());
  EXPECT_TRUE(expanded_templates_button->GetVisible());
  EXPECT_FALSE(desks_bar_view->IsZeroState());

  // Delete the one and only template, we should remain in the desk library.
  DeleteTemplate(uuid, /*expected_current_item_count=*/1);
  EXPECT_TRUE(GetOverviewSession()
                  ->GetGridWithRootWindow(root_window)
                  ->saved_desk_library_widget()
                  ->IsVisible());

  // Test that we are not in zero state.
  auto* zero_new_desk_button = desks_bar_view->zero_state_new_desk_button();
  EXPECT_FALSE(zero_new_desk_button->GetVisible());
  EXPECT_FALSE(desks_bar_view->IsZeroState());
}

// Tests that the unsupported apps dialog is shown when a user attempts to save
// an active desk with unsupported apps.
TEST_F(SavedDeskTest, UnsupportedAppsDialog) {
  // Create a crostini window.
  auto crostini_window = CreateAppWindow();
  crostini_window->SetProperty(aura::client::kAppType,
                               static_cast<int>(AppType::CROSTINI_APP));

  // Create a normal window.
  auto test_window = CreateAppWindow();

  // Open overview and click on the save template button. The unsupported apps
  // dialog should show up.
  auto* root = Shell::Get()->GetPrimaryRootWindow();
  ToggleOverview();
  auto* save_template_button = GetSaveDeskAsTemplateButtonForRoot(root);
  ASSERT_TRUE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
  ClickOnView(save_template_button);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Decline the dialog. We should stay in overview and no template should have
  // been saved.
  auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
  dialog_controller->dialog_widget()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->CancelDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());

  // Click on the save template button again. The unsupported apps dialog should
  // show up.
  ClickOnView(save_template_button);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Accept the dialog. The template should have been saved and the templates
  // grid should now be shown.
  dialog_controller = saved_desk_util::GetSavedDeskDialogController();
  dialog_controller->dialog_widget()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->AcceptDialog();
  WaitForDesksTemplatesUI();
  EXPECT_TRUE(GetOverviewSession());
  EXPECT_TRUE(GetOverviewGridList()[0]->saved_desk_library_widget());

  ASSERT_EQ(1ul, GetAllEntries().size());
}

// Tests that the save desk as template button and save for later button are
// disabled when all windows on the desk are unsupported or there are no windows
// with Full Restore app ids. See crbug.com/1277763.
TEST_F(SavedDeskTest, AllUnsupportedAppsDisablesSaveDeskButtons) {
  SetDisableAppIdCheckForDeskTemplates(false);

  // Use `CreateTestWindow()` instead of `CreateAppWindow()`, which by default
  // creates a supported window.
  auto test_window = CreateTestWindow();

  // Also create an app window which should not have an app id, making it
  // "unsupported".
  auto no_app_id_window = CreateAppWindow();
  auto* delegate = Shell::Get()->desks_templates_delegate();
  ASSERT_TRUE(
      delegate->IsWindowSupportedForDeskTemplate(no_app_id_window.get()));
  ASSERT_TRUE(full_restore::GetAppId(no_app_id_window.get()).empty());

  // Open overview.
  ToggleOverview();

  EXPECT_EQ(0, GetOverviewGridList()[0]->num_incognito_windows());
  EXPECT_EQ(2, GetOverviewGridList()[0]->num_unsupported_windows());

  auto* save_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::Get()->GetPrimaryRootWindow());
  auto* save_for_later_button =
      GetSaveDeskForLaterButtonForRoot(Shell::Get()->GetPrimaryRootWindow());
  EXPECT_EQ(views::Button::STATE_DISABLED, save_as_template_button->GetState());
  EXPECT_EQ(views::Button::STATE_DISABLED, save_for_later_button->GetState());
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

// Tests the mouse and touch hover behavior on the template item view.
TEST_F(SavedDeskTest, HoverOnTemplateItemView) {
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskItemView* first_item = GetItemViewFromTemplatesGrid(0);
  SavedDeskItemView* second_item = GetItemViewFromTemplatesGrid(1);
  auto* hover_container_view1 =
      SavedDeskItemViewTestApi(first_item).hover_container();
  auto* hover_container_view2 =
      SavedDeskItemViewTestApi(second_item).hover_container();
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());

  // Move the mouse to hover over `first_item`.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());
  // Move the mouse to hover over `second_item`.
  event_generator->MoveMouseTo(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_TRUE(hover_container_view2->GetVisible());

  // Long press on the `first_item`.
  LongPressAt(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());
  // Long press on the `second_item`.
  LongPressAt(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_TRUE(hover_container_view2->GetVisible());

  // Move the mouse to hover over `first_item` again.
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());

  // Long press on the `second_item`.
  LongPressAt(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_TRUE(hover_container_view2->GetVisible());

  // Move the mouse but make it still remain on top of `first_item`.
  event_generator->MoveMouseTo(first_item->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());

  // Test to make sure hover is updated after dragging to another item.
  event_generator->DragMouseTo(second_item->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_TRUE(hover_container_view2->GetVisible());
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

  // Open overview and click on the save template button. The unsupported apps
  // dialog should not show up and an entry should be saved.
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
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

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
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

  EXPECT_TRUE(InOverviewSession());
}

// Tests that multiple feedback buttons aren't created when we transition
// between hiding and showing the templates grid without leaving overview.
// Regression test for https://crbug.com/1299114.
TEST_F(SavedDeskTest, HideAndShowTemplatesGridWithoutLeavingOverview) {
  // One window is needed to save a template.
  auto window = CreateAppWindow();

  // Open overview and save a template. This will also take us to the desks
  // templates grid view.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  SavedDeskLibraryView* library_view =
      GetOverviewGridList().front()->GetSavedDeskLibraryView();
  ASSERT_TRUE(library_view);

  // The library has two grids, two section labels, one feedback button and one
  // "no items" label.
  SavedDeskLibraryViewTestApi test_api(library_view);
  ASSERT_EQ(6ul, test_api.scroll_view()->contents()->children().size());

  // Click on the grid item to launch the template.
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));
  EXPECT_TRUE(InOverviewSession());

  // Go back to the library view and verify that we have the same amount of
  // expected children.
  ShowDesksTemplatesGrids();
  ASSERT_EQ(6ul, test_api.scroll_view()->contents()->children().size());
}

// Tests that the desks templates are organized in alphabetical order.
TEST_F(SavedDeskTest, ShowTemplatesInAlphabeticalOrder) {
  // Create a window and add three test entry in different names.
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "1_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "a_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "b_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(overview_grid);
  ASSERT_EQ(5ul, grid_items.size());

  // Tests that templates are sorted in alphabetical order.
  EXPECT_EQ(u"1_template", grid_items[0]->GetAccessibleName());
  EXPECT_EQ(u"a_template", grid_items[1]->GetAccessibleName());
  EXPECT_EQ(u"A_template", grid_items[2]->GetAccessibleName());
  EXPECT_EQ(u"b_template", grid_items[3]->GetAccessibleName());
  EXPECT_EQ(u"B_template", grid_items[4]->GetAccessibleName());
}

// Tests that the color of the desks templates button border is as expected.
// Regression test for https://crbug.com/1265003.
TEST_F(SavedDeskTest, DesksTemplatesButtonBorderColor) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  AddEntry(base::GUID::GenerateRandomV4(), "name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto* color_provider = AshColorProvider::Get();
  const SkColor active_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kCurrentDeskColor);
  const SkColor focused_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  ToggleOverview();

  views::View* button = GetDesksTemplatesButtonForRoot(
      Shell::GetPrimaryRootWindow(), /*zero_state=*/false);
  ASSERT_TRUE(button);

  // Helper to get the color of the border of the desks templates button.
  auto get_border_color = [button]() {
    // The inner button is the one where the border is applied to.
    DeskButtonBase* inner_button =
        static_cast<ExpandedDesksBarButton*>(button)->inner_button();
    views::Border* border = inner_button->GetBorder();
    DCHECK(border);
    return border->color();
  };

  // The templates button starts of neither focused nor active.
  EXPECT_EQ(SK_ColorTRANSPARENT, get_border_color());

  // Tests that when we are viewing the templates grid, the button border is
  // active.
  ClickOnView(button);
  EXPECT_EQ(active_color, get_border_color());

  // Tests that when focused, the templates button border has a focused color.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(focused_color, get_border_color());

  // Shift focus away from the templates button. The button border should be
  // active.
  SendKey(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(active_color, get_border_color());
}

// Tests that if we save a template (and get dropped into the templates grid),
// delete all the templates (and the templates grid gets hidden), the windows in
// overview get activated and restored when selected.
TEST_F(SavedDeskTest, WindowActivatableAfterSaveAndDeleteTemplate) {
  // Create a test window.
  auto test_window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<const DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  // Delete the one and only template, which should hide the templates grid but
  // remain in overview.
  DeleteTemplate(entries[0]->uuid(), /*expected_current_item_count=*/1);
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

TEST_F(SavedDeskTest, TemplateNameBounds) {
  AddEntry(base::GUID::GenerateRandomV4(), "template name", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(0);
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
  ClickOnView(name_view);
  for (int i = 0; i < 200; ++i)
    SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();
  EXPECT_EQ(
      name_view->GetBoundsInScreen().x() - item_view->GetBoundsInScreen().x(),
      item_view->GetBoundsInScreen().right() -
          name_view->GetBoundsInScreen().right());
}

// Tests that we are able to edit the template name.
TEST_F(SavedDeskTest, EditTemplateName) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Test that we can add characters to the name and press enter to save it.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_B);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(template_name) + u"ab", name_view->GetText());

  // Deleting characters and pressing enter saves the name.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_BACK);
  SendKey(ui::VKEY_BACK);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // The `name_view` defaults to select all, so typing a letter while all
  // selected replaces the text. Also, clicking anywhere outside of the text
  // field will try to save it.
  ClickOnView(name_view);
  SendKey(ui::VKEY_A);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  event_generator->ClickLeftButton();
  EXPECT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(u"a", name_view->GetText());

  // Test that clicking on the grid item (outside of the textfield) will save
  // it.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_B);
  ClickOnView(GetItemViewFromTemplatesGrid(0));
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(u"ab", name_view->GetText());

  // Pressing TAB also saves the name.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_C);
  SendKey(ui::VKEY_TAB);
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(u"abc", name_view->GetText());

  // There was a bug where a relayout could cause a revert of the name changes,
  // and lead to a crash if the name view had highlight focus. This is a
  // regression test for that. See https://crbug.com/1285113 for more details.
  GetItemViewFromTemplatesGrid(0)->SetBoundsRect(gfx::Rect(150, 40));
  EXPECT_EQ(u"abc", GetItemViewFromTemplatesGrid(0)->name_view()->GetText());
}

// Tests for checking that certain conditions will revert the template name to
// its original name, even if the text in the textfield has been updated.
TEST_F(SavedDeskTest, TemplateNameChangeAborted) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Pressing enter with no changes to the text.
  ClickOnView(name_view);
  EXPECT_TRUE(overview_grid->IsTemplateNameBeingModified());
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_TRUE(name_view->HasSelection());
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(name_view->HasFocus());
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // Pressing the escape key will revert the changes made to the name in the
  // textfield.
  ClickOnView(name_view);
  SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_B);
  SendKey(ui::VKEY_C);
  SendKey(ui::VKEY_ESCAPE);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // Empty text fields will also revert back to the original name.
  ClickOnView(name_view);
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
}

// Tests to verify that clicking the spacebar doesn't cause the name view to
// lose focus (since it's within a button), and that whitespaces are handled
// correctly.
TEST_F(SavedDeskTest, TemplateNameTestSpaces) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Pressing spacebar does not cause `name_view` to lose focus.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_SPACE);
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_EQ(base::UTF8ToUTF16(template_name) + u" ", name_view->GetText());

  // Extra whitespace should be trimmed.
  SendKey(ui::VKEY_HOME);
  SendKey(ui::VKEY_SPACE);
  SendKey(ui::VKEY_SPACE);
  EXPECT_EQ(u"  " + base::UTF8ToUTF16(template_name) + u" ",
            name_view->GetText());
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());

  // A string consisting of just spaces is considered an empty string, and the
  // name change is reverted.
  EXPECT_FALSE(name_view->HasFocus());
  ClickOnView(name_view);
  EXPECT_TRUE(name_view->HasFocus());
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_SPACE);
  EXPECT_EQ(u" ", name_view->GetText());
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
}

// Tests that there is no crash after we use the keyboard to change the name of
// a template. Regression test for https://crbug.com/1279649.
TEST_F(SavedDeskTest, EditTemplateNameWithKeyboardNoCrash) {
  AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "b", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Tab until we focus the name view of the first template item.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(name_view, GetHighlightedView());

  // Rename template "a" to template "d".
  SendKey(ui::VKEY_RETURN);
  SendKey(ui::VKEY_D);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();

  // Verify that there is no crash after we tab again.
  SendKey(ui::VKEY_TAB);
}

// Tests that there is no crash when leaving the template name view focused with
// a changed name during shutdown. Regression test for
// https://crbug.com/1281422.
TEST_F(SavedDeskTest, EditTemplateNameShutdownNoCrash) {
  // The fade out animation of the desks templates grid must be enabled for this
  // crash to have happened.
  animation_scale_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "b", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();

  // Tab until we focus the name view of the first template item.
  SendKey(ui::VKEY_TAB);
  SendKey(ui::VKEY_TAB);
  ASSERT_EQ(name_view, GetHighlightedView());

  // Rename template "a" to template "ddd".
  SendKey(ui::VKEY_RETURN);
  SendKey(ui::VKEY_D);
  SendKey(ui::VKEY_D);
  SendKey(ui::VKEY_D);

  // Verify that there is no crash while the test tears down.
}

// Tests that the hovering over the templates name shows the expected cursor.
TEST_F(SavedDeskTest, TemplatesNameHitTest) {
  auto* cursor_manager = Shell::Get()->cursor_manager();

  for (bool is_rtl : {true, false}) {
    SCOPED_TRACE(is_rtl ? "rtl" : "ltr");
    base::i18n::SetRTLForTesting(is_rtl);

    AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now(),
             DeskTemplateType::kTemplate);

    OpenOverviewAndShowTemplatesGrid();
    SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
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
// active status on clicking library button when we stay in templates grid.
TEST_F(SavedDeskTest, UnFocusNameChangeOnClickingLibrary) {
  // Save an entry in the templates grid.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();
  // Expect we stay in the templates grid.
  auto* overview_grid = GetOverviewSession()->GetGridWithRootWindow(
      Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());

  DeskNameView* desk_name_view =
      overview_grid->desks_bar_view()->mini_views().back()->desk_name_view();
  // Tests if the desk name view rename can work correctly.
  // Click the new desk name view which will be focused.
  ClickOnView(desk_name_view);
  EXPECT_TRUE(overview_grid->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());

  // Click Library button again to unfocus the desk name view when we stay in
  // the templates grid.
  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  ShowDesksTemplatesGrids();
  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_FALSE(desk_name_view->HasFocus());

  // Tests if the saved desk name view rename can work correctly.
  // Click the desk name view at first, and then click the saved desk name view,
  // and finally click the Library button.
  ClickOnView(desk_name_view);
  EXPECT_TRUE(overview_grid->IsDeskNameBeingModified());
  EXPECT_TRUE(desk_name_view->HasFocus());
  EXPECT_TRUE(desk_name_view->HasSelection());

  SavedDeskNameView* saved_name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();
  ClickOnView(saved_name_view);
  EXPECT_TRUE(overview_grid->IsTemplateNameBeingModified());
  EXPECT_TRUE(saved_name_view->HasFocus());
  EXPECT_TRUE(saved_name_view->HasSelection());

  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  ShowDesksTemplatesGrids();

  // Check if the desk name view and the saved desk name view are all unfocused.
  ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_FALSE(desk_name_view->HasFocus());
  EXPECT_FALSE(saved_name_view->HasFocus());
}

// Tests that accessibility overrides are set as expected.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorInOverview) {
  auto window = CreateTestWindow(gfx::Rect(100, 100));

  ToggleOverview();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  DCHECK(focus_widget);

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  auto* desk_widget = const_cast<views::Widget*>(grid->desks_widget());
  DCHECK(desk_widget);

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
// templates view.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorInViewingTemplate) {
  auto window = CreateTestWindow(gfx::Rect(100, 100));

  AddEntry(base::GUID::GenerateRandomV4(), "test_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  DCHECK(focus_widget);

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* desk_widget =
      const_cast<views::Widget*>(overview_grid->desks_widget());
  DCHECK(desk_widget);
  views::Widget* library_widget = overview_grid->saved_desk_library_widget();
  DCHECK(library_widget);

  // Order should be [focus_widget, library_widget, desk_widget].
  CheckA11yOverrides("focus", focus_widget, desk_widget, library_widget);
  CheckA11yOverrides("template", library_widget, focus_widget, desk_widget);
  CheckA11yOverrides("desk", desk_widget, library_widget, focus_widget);
}

// Tests that accessibility overrides are set as expected after entering
// templates view when no window opens.
TEST_F(SavedDeskTest, AccessibilityFocusAnnotatorWhenNoWindowOpen) {
  AddEntry(base::GUID::GenerateRandomV4(), "test_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  DCHECK(focus_widget);

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* desk_widget =
      const_cast<views::Widget*>(overview_grid->desks_widget());
  DCHECK(desk_widget);
  views::Widget* library_widget = overview_grid->saved_desk_library_widget();
  DCHECK(library_widget);

  // Order should be [focus_widget, template_widget, desk_widget].
  CheckA11yOverrides("focus", focus_widget, desk_widget, library_widget);
  CheckA11yOverrides("template", library_widget, focus_widget, desk_widget);
  CheckA11yOverrides("desk", desk_widget, library_widget, focus_widget);
}

// Tests that the children of the overview grid matches the order they are
// displayed so accessibility traverses it correctly.
TEST_F(SavedDeskTest, AccessibilityGridItemTraversalOrder) {
  auto window = CreateTestWindow(gfx::Rect(100, 100));

  AddEntry(base::GUID::GenerateRandomV4(), "template_4", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template_3", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template_2", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  SavedDeskLibraryView* library_view = overview_grid->GetSavedDeskLibraryView();
  ASSERT_FALSE(library_view->grid_views().empty());

  SavedDeskGridView* grid_view = library_view->grid_views().front();
  ASSERT_TRUE(grid_view);

  // The grid items are sorted and displayed alphabetically.
  std::vector<SavedDeskItemView*> grid_items = grid_view->grid_items();
  views::View::Views grid_child_views = grid_view->children();

  // Verifies the order of the children matches what is displayed in the grid.
  for (size_t i = 0; i < grid_items.size(); i++)
    ASSERT_EQ(grid_items[i], grid_child_views[i]);
}

TEST_F(SavedDeskTest, LayoutItemsInLandscape) {
  UpdateDisplay("800x600");

  // Create a window and add four test entries.
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "C_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "D_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

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
  AddEntry(base::GUID::GenerateRandomV4(), "A_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "B_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "C_template", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "D_template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

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
    AddEntry(base::GUID::GenerateRandomV4(), name, base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  OpenOverviewAndShowTemplatesGrid();

  SavedDeskLibraryView* library_view =
      GetOverviewGridList().front()->GetSavedDeskLibraryView();

  // The library has two grids, two section labels, one feedback button and one
  // "no items" label.
  SavedDeskLibraryViewTestApi test_api(library_view);
  views::View::Views library_child_views =
      test_api.scroll_view()->contents()->children();
  ASSERT_EQ(6ul, library_child_views.size());

  const gfx::Rect shelf_bounds =
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen();

  // Test that none of the grid items overlap with the shelf.
  for (views::View* view : library_child_views)
    EXPECT_FALSE(view->GetBoundsInScreen().Intersects(shelf_bounds));
}

// Tests that showing the overview records to the TemplateGrid histogram.
TEST_F(SavedDeskTest, RecordDesksTemplateGridShowMetric) {
  // Make sure that LoadTemplateHistogram is recorded.
  base::HistogramTester histogram_tester;

  // Entry needed so that overview is accessible
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  // Assert load grid histogram recorded.
  constexpr int kExpectedGridShows = 1;
  histogram_tester.ExpectTotalCount(kLoadTemplateGridHistogramName,
                                    kExpectedGridShows);
}

// Tests that deleting templates in the templates grid Records to the delete
// template histogram.
TEST_F(SavedDeskTest, DeleteTemplateRecordsMetric) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // This window should be hidden whenever the desk templates grid is open.
  auto test_window = CreateTestWindow();

  // This action should record deletions and grid shows in a UMA histogram.
  base::HistogramTester histogram_tester;

  OpenOverviewAndShowTemplatesGrid();

  // The window is hidden because the desk templates grid is open.
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_1`.
  DeleteTemplate(uuid_1, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());

  // Verifies that the template with `uuid_1`, doesn't exist anymore.
  DeleteTemplate(uuid_1, /*expected_current_item_count=*/0,
                 /*expect_template_exists=*/false);
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
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Assert load grid histogram and launch histogram recorded.
  constexpr int kExpectedLaunches = 1;
  histogram_tester.ExpectTotalCount(kLaunchTemplateHistogramName,
                                    kExpectedLaunches);
}

// Tests that clicking the save desk as template button records to the
// new template histogram.
TEST_F(SavedDeskTest, SaveDeskAsTemplateRecordsMetric) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // Record histogram
  base::HistogramTester histogram_tester;

  // The `save_desk_as_template_button` is visible when at least one window is
  // open.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_button);
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->IsSaveDeskAsTemplateButtonVisible());

  // Click on `save_desk_as_template_button`.
  ClickOnView(save_desk_as_template_button);
  WaitForDesksTemplatesUI();
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Expect that the Desk Templates grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());

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
  crostini_window->SetProperty(aura::client::kAppType,
                               static_cast<int>(AppType::CROSTINI_APP));

  // Create a normal window.
  auto test_window = CreateAppWindow();

  // Open overview and click on the save template button. The unsupported apps
  // dialog should show up.
  auto* root = Shell::Get()->GetPrimaryRootWindow();
  ToggleOverview();
  SaveDeskTemplateButton* save_template_button =
      GetSaveDeskAsTemplateButtonForRoot(root);
  ASSERT_TRUE(
      GetOverviewGridForRoot(root)->IsSaveDeskAsTemplateButtonVisible());
  ClickOnView(save_template_button);
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
  app_launch_info_1->active_tab_index = kActiveTabIndex1;
  app_launch_info_1->urls = absl::make_optional(kTabs1);
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_1));
  app_restore::WindowInfo window_info_1;
  window_info_1.activation_index = absl::make_optional<int32_t>(kWindowId1);
  restore_data->ModifyWindowInfo(kAppId1, kWindowId1, window_info_1);

  // Add app launch info for the second browser instance.
  auto app_launch_info_2 =
      std::make_unique<app_restore::AppLaunchInfo>(kAppId2, kWindowId2);
  app_launch_info_2->active_tab_index = kActiveTabIndex2;
  app_launch_info_2->urls = absl::make_optional(kTabs2);
  restore_data->AddAppLaunchInfo(std::move(app_launch_info_2));
  app_restore::WindowInfo window_info_2;
  window_info_2.activation_index = absl::make_optional<int32_t>(kWindowId2);
  restore_data->ModifyWindowInfo(kAppId2, kWindowId2, window_info_2);

  auto desk_template = std::make_unique<DeskTemplate>(
      base::GUID::GenerateRandomV4(), DeskTemplateSource::kUser, "template_1",
      base::Time::Now(), DeskTemplateType::kTemplate);
  desk_template->set_desk_restore_data(std::move(restore_data));

  // Record histogram.
  base::HistogramTester histogram_tester;

  ToggleOverview();

  // Mocks saving templates with some browsers.
  saved_desk_util::GetSavedDeskPresenter()->SaveOrUpdateDeskTemplate(
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

    // Toggle overview if there isn't currently an overview. This is needed
    // to save a template via the UI.
    if (!GetOverviewSession()) {
      ToggleOverview();
    }

    // The `save_desk_as_template_button` is visible when at least one window is
    // open.
    SaveDeskTemplateButton* save_desk_as_template_button =
        GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(save_desk_as_template_button);
    EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                    ->IsSaveDeskAsTemplateButtonVisible());

    // Click on `save_desk_as_template_button` button.
    ClickOnView(save_desk_as_template_button);
    WaitForDesksTemplatesUI();
    ASSERT_EQ(num_templates + 1, GetAllEntries().size());

    // Expect that the Desk Templates grid is visible.
    EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
  }

  OpenOverviewAndShowTemplatesGrid();

  // Delete one of the templates which will iterate the histogram's second
  // bucket.
  DeleteTemplate(GetAllEntries()[0]->uuid(), /*expected_current_item_count=*/3);

  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 1, 1);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 2, 2);
  histogram_tester.ExpectBucketCount(kUserTemplateCountHistogramName, 3, 1);
}

// Tests record metrics when current template being replaced.
TEST_F(SavedDeskTest, ReplaceTemplateMetric) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600,800x600");

  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  const std::string name_1 = "template_1";
  AddEntry(uuid_1, name_1, base::Time::Now(), DeskTemplateType::kTemplate);

  const base::GUID uuid_2 = base::GUID::GenerateRandomV4();
  const std::string name_2 = "template_2";
  AddEntry(uuid_2, name_2, base::Time::Now(), DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/1);
  // Show replace dialogs.
  auto* dialog_controller = saved_desk_util::GetSavedDeskDialogController();
  auto callback = base::BindLambdaForTesting([&]() {
    item_view->name_view()->SetText(base::UTF8ToUTF16(name_1));
    item_view->ReplaceTemplate(uuid_1.AsLowercaseString());
  });

  dialog_controller->ShowReplaceDialog(
      Shell::GetPrimaryRootWindow(), base::UTF8ToUTF16(name_1),
      DeskTemplateType::kTemplate, callback, base::DoNothing());
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  ASSERT_TRUE(GetOverviewSession());

  // Accepting the dialog will record metrics.
  dialog_controller->dialog_widget()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->AcceptDialog();

  WaitForDesksTemplatesUI();

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
  std::vector<const DeskTemplate*> entries = GetAllEntries();
  EXPECT_EQ(uuid_2, entries[0]->uuid());
  // Assert metrics being recorded.
  histogram_tester.ExpectTotalCount(kReplaceTemplateHistogramName, 1);

  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());
}

// Tests that there is no animation when removing a desk with windows while the
// grid is shown. Regression test for https://crbug.com/1291770.
TEST_F(SavedDeskTest, NoAnimationWhenRemovingDesk) {
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create and a new desk, and create a test window on the active desk.
  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  auto test_window = CreateAppWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  ASSERT_TRUE(desks_controller->BelongsToActiveDesk(test_window.get()));

  OpenOverviewAndShowTemplatesGrid();

  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Remove the active desk. Ensure that there are no animations on the overview
  // item, otherwise a flicker will be seen as they should be hidden when the
  // desks templates grid is shown.
  RemoveDesk(desks_controller->active_desk());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  OverviewItem* overview_item =
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
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
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

  OpenOverviewAndShowTemplatesGrid();

  // All the windows are hidden to show the templates grid.
  EXPECT_EQ(0.f, test_window1->layer()->opacity());
  EXPECT_EQ(0.f, test_window2->layer()->opacity());
  EXPECT_EQ(0.f, test_window3->layer()->opacity());

  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Activate the second desk which has no windows. Test that all the windows
  // have their opacity restored.
  ActivateDesk(desks_controller->desks()[1].get());
  EXPECT_EQ(1.f, test_window1->layer()->opacity());
  EXPECT_EQ(1.f, test_window2->layer()->opacity());
  EXPECT_EQ(1.f, test_window3->layer()->opacity());
}

// Tests that windows have their opacity reset after being hidden and then
// leaving overview. Regression test for https://crbug.com/1292773.
TEST_F(SavedDeskTest, WindowOpacityResetAfterLeavingOverview) {
  const base::GUID uuid = base::GUID::GenerateRandomV4();
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

  OpenOverviewAndShowTemplatesGrid();

  // The windows are hidden to show the templates grid.
  ASSERT_EQ(0.f, test_window1->layer()->opacity());
  ASSERT_EQ(0.f, test_window2->layer()->opacity());

  // The bug did not repro with zero duration as the animation callback to
  // reshow the windows would happen immediately.
  ui::ScopedAnimationDurationScaleMode animation(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Launch a new desk.
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

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

// Tests that the desks templates name view can accept touch events and get
// focused. Regression test for https://crbug.com/1291769.
TEST_F(SavedDeskTest, TouchForNameView) {
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  ASSERT_FALSE(name_view->HasFocus());

  // The name view should receive focus after getting a gesture tap.
  GetEventGenerator()->GestureTapAt(
      name_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(name_view->HasFocus());
}

// Tests that the desks templates use the right time string format. It's
// expected to align with the File App. More details can be found at:
// https://crbug.com/1268922.
TEST_F(SavedDeskTest, TimeStrFormat) {
  // Uses `01-01-2022 10:30 AM`, `Today 10:30 AM`, `Yesterday 10:30 AM`, and
  // ``Tomorrow 10:30 AM`` for test.
  base::Time time_long_ago, time_today, time_yesterday;

  // 01-01-2022 10:30 AM.
  base::Time::Exploded exploded_long_ago = {
      /*year=*/2022,
      /*month=*/1,
      /*day_of_week=*/6,
      /*day_of_month=*/1,
      /*hour=*/10,
      /*minute=*/30,
      /*second=*/0,
      /*millisecond=*/0,
  };
  ASSERT_TRUE(base::Time::FromLocalExploded(exploded_long_ago, &time_long_ago));

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

  const std::vector<base::GUID> uuid = {
      base::GUID::GenerateRandomV4(),
      base::GUID::GenerateRandomV4(),
      base::GUID::GenerateRandomV4(),
  };
  const std::vector<std::string> name = {
      "template_1",
      "template_2",
      "template_3",
  };
  // The expected time string for each template.
  const std::vector<std::u16string> expected_timestr = {
      u"Jan 1, 2022, 10:30 AM",
      u"Today 10:30 AM",
      u"Yesterday 10:30 AM",
  };
  std::vector<base::Time> time = {
      time_long_ago,
      time_today,
      time_yesterday,
  };

  for (size_t i = 0; i < 3; i++)
    AddEntry(uuid[i], name[i], time[i], DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  // Tests that each template comes with an expected time string format.
  std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
  for (size_t i = 0; i < 3; i++) {
    auto iter =
        std::find_if(grid_items.cbegin(), grid_items.cend(),
                     [uuid, i](const SavedDeskItemView* v) {
                       return SavedDeskItemViewTestApi(v).uuid() == uuid[i];
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

  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));

  // Test that overview is still active and there is no crash.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that we cap the number of template items shown, even if the backend has
// more saved.
TEST_F(SavedDeskTest, CapTemplateItemsShown) {
  desks_storage::LocalDeskDataManager::SetDisableMaxTemplateLimitForTesting(
      true);

  constexpr unsigned long kMaxTemplateCount = 6;
  // Create more than the maximum number of templates allowable.
  for (unsigned long i = 1; i < kMaxTemplateCount + 20; i++) {
    AddEntry(base::GUID::GenerateRandomV4(),
             "template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  OpenOverviewAndShowTemplatesGrid();

  // Check to make sure we are only showing up to the maximum number of items.
  const std::vector<SavedDeskItemView*> grid_items =
      GetItemViewsFromDeskLibrary(GetOverviewGridList().front().get());
  EXPECT_EQ(kMaxTemplateCount, grid_items.size());
}

// Tests that click or tap could exit grid view and commit name change when
// appropriate. Regression test for https://crbug.com/1290568.
TEST_F(SavedDeskTest, ClickOrTapToExitGridView) {
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template_2", base::Time::Now(),
           DeskTemplateType::kTemplate);
  AddEntry(base::GUID::GenerateRandomV4(), "template_3", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Test mouse click.
  {
    OpenOverviewAndShowTemplatesGrid();

    SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
    EXPECT_FALSE(name_view->HasFocus());

    // The name view should receive focus after getting a mouse click.
    ClickOnView(name_view);
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
    OpenOverviewAndShowTemplatesGrid();

    SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
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

// Tests that right clicking on the wallpaper while showing the saved desks grid
// does not exit overview.
TEST_F(SavedDeskTest, RightClickOnWallpaperStaysInOverview) {
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  // Find a point that doesn't touch the item view, but is still in the library
  // widget's window bounds. Right clicking this point should not close
  // overview, as it should open the wallpaper context menu.
  SavedDeskItemView* item_view =
      GetItemViewFromTemplatesGrid(/*grid_item_index=*/0);
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

  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Create a window which is shown on all desks.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  auto* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  widget->SetVisibleOnAllWorkspaces(true);
  ASSERT_TRUE(desks_util::IsWindowVisibleOnAllWorkspaces(window.get()));

  OpenOverviewAndShowTemplatesGrid();

  // Click on the template item to launch the new template.
  SavedDeskItemView* template_item =
      GetItemViewFromTemplatesGrid(/*grid_item_index=*/0);
  DCHECK(template_item);
  ClickOnView(template_item);
  ASSERT_EQ(2, controller->GetNumberOfDesks());

  // The visible on all desks window belongs to the active desk, and has an
  // associated overview item.
  EXPECT_TRUE(controller->BelongsToActiveDesk(window.get()));
  EXPECT_TRUE(GetOverviewItemForWindow(window.get()));
}

// Test save same desk as template won't create name with number on the template
// view for the second template.
TEST_F(SavedDeskTest, NoDuplicateDisplayedName) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();

  // The `save_desk_as_template_button` is visible when at least one window is
  // open.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_button);
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->IsSaveDeskAsTemplateButtonVisible());

  // Click on `save_desk_as_template_button`.
  ClickOnView(save_desk_as_template_button);
  ASSERT_EQ(1ul, GetAllEntries().size());
  WaitForDesksTemplatesUI();
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  EXPECT_EQ(u"Desk 1", GetItemViewFromTemplatesGrid(0)->name_view()->GetText());
  // The new template name still have name nudge to maintain it's uniqueness.
  EXPECT_EQ(u"Desk 1", GetAllEntries().back()->template_name());

  // Exit overview and save the same desk again.
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());
  ToggleOverview();

  save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_button);
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->IsSaveDeskAsTemplateButtonVisible());

  // Click on `save_desk_as_template_button`. At this point the template
  // name matches the desk name.
  ClickOnView(save_desk_as_template_button);
  ASSERT_EQ(2ul, GetAllEntries().size());
  WaitForDesksTemplatesUI();
  // Newly created template name_view.
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
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
            GetItemViewFromTemplatesGrid(0)->desk_template().template_name());

  // Set the second template to have a new unique name by updating the model
  // directly. This mimics updating the name on a different device and is the
  // only way to change the name without prompting the replace dialog.
  SavedDeskItemView* second_item = GetItemViewFromTemplatesGrid(1);
  auto new_desk_template = second_item->desk_template().Clone();
  new_desk_template->set_template_name(u"Desk 2");
  const base::GUID uuid = new_desk_template->uuid();

  base::RunLoop loop;
  desk_model()->AddOrUpdateEntry(
      std::move(new_desk_template),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();

  base::RunLoop loop1;
  desk_model()->GetEntryByUUID(
      uuid.AsLowercaseString(),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::GetEntryByUuidStatus status,
              std::unique_ptr<ash::DeskTemplate> entry) {
            EXPECT_EQ(desks_storage::DeskModel::GetEntryByUuidStatus::kOk,
                      status);
            // `LocalDeskStorage` does not support
            // `EntriesAddedOrUpdatedRemotely`, so
            // manually call it to simluate what the real model would do.
            saved_desk_util::GetSavedDeskPresenter()
                ->EntriesAddedOrUpdatedRemotely({entry.get()});
            ASSERT_EQ(u"Desk 2", second_item->name_view()->GetText());
            ASSERT_EQ(u"Desk 2", second_item->desk_template().template_name());
            loop1.Quit();
          }));
  loop1.Run();

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

  // Cancel on replace dialog will revert view name to template name.
  dialog_widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_EQ(u"Desk 1", name_view->GetText());
}

// Tests that if there is a duplicate template name, saving a new template will
// select all the text. Regression test for https://crbug.com/1303924.
TEST_F(SavedDeskTest, SelectAllAfterSavingDuplicateTemplate) {
  // First add a template that has the same name as the active desk.
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  AddEntry(base::GUID::GenerateRandomV4(), "Desk 1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  auto test_window = CreateAppWindow();
  ToggleOverview();

  // Click on `save_desk_as_template_button` button.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ClickOnView(save_desk_as_template_button);
  WaitForDesksTemplatesUI();

  // Expect that the entire text of the new template is selected.
  EXPECT_EQ(u"Desk 1", GetItemViewFromTemplatesGrid(0)->name_view()->GetText());
  EXPECT_EQ(u"Desk 1", GetItemViewFromTemplatesGrid(1)->name_view()->GetText());
  EXPECT_TRUE(GetItemViewFromTemplatesGrid(0)->name_view()->HasFocus());
  EXPECT_EQ(u"Desk 1",
            GetItemViewFromTemplatesGrid(0)->name_view()->GetSelectedText());
}

// Tests that a newly saved template will always show up on the top left corner
// regardless of its name and verify that it goes to its alphabetical order
// once the name is confirmed.
TEST_F(SavedDeskTest, NoSortBeforeNameConfirmed) {
  // Create a window to enable the save as template button.
  auto test_window = CreateAppWindow();

  // Add an entry with a low lexiconic value for the template name to test that
  // the new saved template is always preceding this one.
  AddEntry(base::GUID::GenerateRandomV4(), "aaaa", base::Time::Now(),
           DeskTemplateType::kTemplate);

  // Enter overview and save the same desk again.
  ToggleOverview();
  // The `save_desk_as_template_button` is visible when at least one window is
  // open.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_button);
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->IsSaveDeskAsTemplateButtonVisible());

  // Click on `save_desk_as_template_button`. The newly saved template
  // should be in the front, even though its name is not in alphabetical order.
  ClickOnView(save_desk_as_template_button);
  ASSERT_EQ(2ul, GetAllEntries().size());
  WaitForDesksTemplatesUI();

  // Newly created template name_view.
  SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_TRUE(name_view->HasFocus());
  ASSERT_EQ(u"Desk 1", DesksController::Get()->active_desk()->name());
  EXPECT_EQ(u"Desk 1", name_view->GetText());

  // Change the saved template name and save it.
  SendKey(ui::VKEY_Z);
  SendKey(ui::VKEY_Z);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();

  // Check that the name is saved and it's moved to its proper alphabetical
  // order. This should be the second entry in the templates grid.
  name_view = GetItemViewFromTemplatesGrid(1)->name_view();
  EXPECT_EQ(u"zz", name_view->GetText());
}

TEST_F(SavedDeskTest, NudgeOnTheCorrectDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto test_window = CreateAppWindow();
  ToggleOverview();

  // Click on `save_desk_as_template_button` button on the primary display.
  SaveDeskTemplateButton* save_desk_as_template_button =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetAllRootWindows()[0]);
  ClickOnView(save_desk_as_template_button);
  WaitForDesksTemplatesUI();

  // The desks templates widget associated with the primary display should be
  // active.
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            window_util::GetActiveWindow()->GetRootWindow());
}

// Tests that the save desk button container is properly placed after an
// overview item is closed via swipe.
TEST_F(SavedDeskTest, SaveDeskButtonContainerVisibleAfterSwipeToClose) {
  // Use a test widget so we can close it properly after swiping to close. The
  // order matters here; overview items are ordered by MRU order, so the most
  // recently created widget corresponds to the first overview item.
  auto widget2 = CreateTestWidget();
  auto widget1 = CreateTestWidget();

  ToggleOverview();

  OverviewItem* item1 = GetOverviewItemForWindow(widget1->GetNativeWindow());
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
  OverviewItem* item2 = GetOverviewItemForWindow(widget2->GetNativeWindow());
  ASSERT_TRUE(item2);
  SaveDeskTemplateButtonContainer* save_desk_button_container =
      GetSaveDeskButtonContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(item2->target_bounds().Intersects(
      gfx::RectF(save_desk_button_container->GetBoundsInScreen())));
}

TEST_F(SavedDeskTest, AdminTemplate) {
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now(),
           DeskTemplateSource::kPolicy, DeskTemplateType::kTemplate,
           std::make_unique<app_restore::RestoreData>());

  OpenOverviewAndShowTemplatesGrid();

  // Tests that the name is read only and not focusable.
  SavedDeskItemView* item_view =
      GetItemViewFromTemplatesGrid(/*grid_item_index=*/0);
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
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(item_view, GetHighlightedView());
  SendKey(ui::VKEY_TAB);
  EXPECT_NE(name_view, GetHighlightedView());
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
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 1",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 2",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 3",
             base::Time::Now(), DeskTemplateType::kTemplate);

    // Add 3 `kSaveAndRecall` entries.
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 1",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 2",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 3",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);

    OpenOverviewAndShowTemplatesGrid();

    SavedDeskLibraryView* library_view =
        GetOverviewGridList()[0]->GetSavedDeskLibraryView();
    SavedDeskLibraryViewTestApi test_api(library_view);
    EXPECT_FALSE(test_api.scroll_view()->vertical_scroll_bar()->GetVisible());

    ExitOverview();
  }

  // 6 templates and 6 save-and-recall desks would show the scroll bar.
  {
    // Add 3 more `kTemplate` entries.
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 4",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 5",
             base::Time::Now(), DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 6",
             base::Time::Now(), DeskTemplateType::kTemplate);

    // Add 3 more `kSaveAndRecall` entries.
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 4",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 5",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);
    AddEntry(base::GUID::GenerateRandomV4(), "desk_template 6",
             base::Time::Now(), DeskTemplateType::kSaveAndRecall);

    OpenOverviewAndShowTemplatesGrid();

    SavedDeskLibraryView* library_view =
        GetOverviewGridList()[0]->GetSavedDeskLibraryView();
    SavedDeskLibraryViewTestApi test_api(library_view);
    EXPECT_TRUE(test_api.scroll_view()->vertical_scroll_bar()->GetVisible());

    ExitOverview();
  }
}

// Tests that the save desk item view is fully visible with the highlight
// change.
TEST_F(SavedDeskTest, ScrollWithHighlightChange) {
  // Add 6 `kTemplate` entries and 6 `kSaveAndRecall` entries.
  for (size_t i = 1; i <= 6; i++) {
    AddEntry(base::GUID::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(),
             "saved_desk " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kSaveAndRecall);
  }

  OpenOverviewAndShowTemplatesGrid();

  for (size_t i = 0; i < 12; i++) {
    SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(i);

    // Verify item view is highlighted and fully visible.
    SendKey(ui::VKEY_TAB);
    EXPECT_TRUE(item_view->IsViewHighlighted());
    EXPECT_EQ(item_view->GetPreferredSize(),
              item_view->GetVisibleBounds().size());

    // Verify name view is highlighted and fully visible.
    SendKey(ui::VKEY_TAB);
    EXPECT_TRUE(item_view->name_view()->IsViewHighlighted());
    EXPECT_EQ(item_view->name_view()->GetPreferredSize(),
              item_view->name_view()->GetVisibleBounds().size());
  }

  // Verify feedback button is highlighted and fully visible.
  FeedbackButton* feedback_button = GetSavedDeskFeedbackButton();
  SendKey(ui::VKEY_TAB);
  EXPECT_TRUE(feedback_button->IsViewHighlighted());
  EXPECT_EQ(feedback_button->GetPreferredSize(),
            feedback_button->GetVisibleBounds().size());
}

// Tests that the scroll bar works with the keyboard.
TEST_F(SavedDeskTest, ScrollWithKeyboard) {
  // Add 6 `kTemplate` entries and 6 `kSaveAndRecall` entries.
  for (size_t i = 1; i <= 6; i++) {
    AddEntry(base::GUID::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
    AddEntry(base::GUID::GenerateRandomV4(),
             "saved_desk " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kSaveAndRecall);
  }

  OpenOverviewAndShowTemplatesGrid();

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
    SendKey(key);
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
    AddEntry(base::GUID::GenerateRandomV4(),
             "desk_template " + base::NumberToString(i), base::Time::Now(),
             DeskTemplateType::kTemplate);
  }

  // Create a window then save the current desk for later.
  CreateAppWindow().release();
  ToggleOverview();
  auto* save_desk_button =
      GetSaveDeskForLaterButtonForRoot(Shell::Get()->GetPrimaryRootWindow());
  ClickOnView(save_desk_button);
  WaitForDesksTemplatesUI();
  WaitForDesksTemplatesUI();

  // The newly saved desk item should be fully visible.
  SavedDeskItemView* item_view = GetItemViewFromTemplatesGrid(6);
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
  OverviewGrid* overview_grid = GetOverviewSession()->GetGridWithRootWindow(
      Shell::GetPrimaryRootWindow());

  // Pre-check whether the save desk button is in the correct state.
  EXPECT_TRUE(overview_grid->IsSaveDeskButtonContainerVisible());

  const DesksBarView* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_EQ(2u, desks_bar_view->mini_views().size());
  DeskMiniView* mini_view_to_be_removed =
      desks_bar_view->FindMiniViewForDesk(active_desk);
  ASSERT_TRUE(mini_view_to_be_removed);

  // Close the active desk and check that the save desk button updates
  // correctly.
  ClickOnView(mini_view_to_be_removed->desk_action_view()->close_all_button());
  EXPECT_FALSE(overview_grid->IsSaveDeskButtonContainerVisible());

  // Try undoing desk close to see if the save desk button returns to the right
  // state,
  views::Button* undo_button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  ASSERT_TRUE(undo_button);
  ClickOnView(undo_button);
  EXPECT_TRUE(overview_grid->IsSaveDeskButtonContainerVisible());
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
  std::vector<const DeskTemplate*> entries = GetAllEntries();
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
      GetItemViewFromTemplatesGrid(/*grid_item_index=*/0);
  ASSERT_TRUE(template_item);
  ClickOnView(template_item);

  // Verify that a new desk has been created and that it has the name of the
  // saved desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(kDeskName, desks_controller->desks()[1]->name());

  // Verify that the saved desk has been deleted.
  EXPECT_TRUE(GetAllEntries().empty());

  // Assert that histogram metrics were recorded.
  histogram_tester.ExpectTotalCount(kLaunchSaveAndRecallHistogramName, 1);
}

TEST_F(DeskSaveAndRecallTest, DeleteSaveAndRecallRecordsMetric) {
  base::HistogramTester histogram_tester;

  UpdateDisplay("800x600");

  const base::GUID uuid = base::GUID::GenerateRandomV4();
  AddEntry(uuid, "saved_desk", base::Time::Now(),
           DeskTemplateType::kSaveAndRecall);

  OpenOverviewAndShowTemplatesGrid();

  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the saved desk.
  DeleteTemplate(uuid, /*expected_current_item_count=*/1);
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
    ClickOnView(
        GetSaveDeskForLaterButtonForRoot(Shell::Get()->GetPrimaryRootWindow()));

    WaitForDesksTemplatesUI();
    WaitForDesksTemplatesUI();

    // Expect that the last added template item name view has focus, and verify
    // that we have a saved desk with the expected `name`.
    OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
    SavedDeskNameView* name_view = GetItemViewFromTemplatesGrid(0)->name_view();
    EXPECT_TRUE(overview_grid->IsTemplateNameBeingModified());
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

  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->spoken_feedback().SetEnabled(true);
  EXPECT_TRUE(accessibility_controller->spoken_feedback().enabled());

  // Ensure we have a desk saved so we can go into the library.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           DeskTemplateType::kSaveAndRecall);

  // Check that we can enter overview, and that there are no windows present.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
                  ->no_windows_widget());

  // Enter the desks library.
  ShowDesksTemplatesGrids();
  WaitForLibraryUI();

  // Simulate how ChromeVox would focus on the desk item view.
  SavedDeskItemView* first_item = GetItemViewFromTemplatesGrid(0);
  first_item->RequestFocus();

  ASSERT_FALSE(Shell::IsSystemModalWindowOpen());

  // Press "Ctrl + w" to show and focus on the delete dialog.
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(Shell::IsSystemModalWindowOpen());

  // Exit the dialog and wait for it to close.
  SendKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(Shell::IsSystemModalWindowOpen());
  ASSERT_TRUE(InOverviewSession());

  // We should be able to exit overview without a crash.
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());
}

// Tests that if the user somehow exits the overview session that the function
// call to `MaybeShowReplaceDialog` exits gracefully before calling
// `saved_desk_util::GetSavedDeskDialogController` which will cause a crash
// due to a DCHECK for the overview session. Regression test for
// https://crbug.com/1351521.
TEST_F(SavedDeskTest, MaybeShowReplaceDialogDoesNotCrashOutsideOverview) {
  UpdateDisplay("800x600,800x600");

  const base::GUID uuid = base::GUID::GenerateRandomV4();
  AddEntry(uuid, /*name=*/"template_1", base::Time::Now(),
           DeskTemplateType::kTemplate);

  OpenOverviewAndShowTemplatesGrid();

  // Get overview grid and added entry.
  const auto& overview_grid = GetOverviewGridList()[0];
  SavedDeskItemView* grid_item =
      GetItemViewsFromDeskLibrary(overview_grid.get())[0];

  // Exit Overview and ensure there is not an overview session
  ToggleOverview();
  EXPECT_FALSE(GetOverviewSession());

  // Call `MaybeShowReplaceDialog` to ensure there isn't a crash.
  grid_item->MaybeShowReplaceDialog(DeskTemplateType::kTemplate, uuid);
}

}  // namespace ash
