// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/desks/templates/desks_templates_test_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class DesksTemplatesTest : public OverviewTestBase {
 public:
  DesksTemplatesTest() = default;
  DesksTemplatesTest(const DesksTemplatesTest&) = delete;
  DesksTemplatesTest& operator=(const DesksTemplatesTest&) = delete;
  ~DesksTemplatesTest() override = default;

  // Adds an entry to the desks model directly without capturing a desk. Allows
  // for testing the names and times of the UI directly.
  void AddEntry(const base::GUID& uuid,
                const std::string& name,
                base::Time created_time,
                std::unique_ptr<app_restore::RestoreData> restore_data =
                    std::make_unique<app_restore::RestoreData>()) {
    auto desk_template = std::make_unique<DeskTemplate>(
        uuid.AsLowercaseString(), DeskTemplateSource::kUser, name,
        created_time);
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
  const std::vector<DeskTemplate*> GetAllEntries() {
    std::vector<DeskTemplate*> templates;

    base::RunLoop loop;
    desk_model()->GetAllEntries(base::BindLambdaForTesting(
        [&](desks_storage::DeskModel::GetAllEntriesStatus status,
            const std::vector<DeskTemplate*>& entries) {
          EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk, status);
          templates = entries;
          loop.Quit();
        }));
    loop.Run();

    return templates;
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

  views::Widget* GetSaveDeskAsTemplateButtonForRoot(aura::Window* root_window) {
    auto* overview_session = GetOverviewSession();
    if (!overview_session)
      return nullptr;

    const auto* overview_grid =
        overview_session->GetGridWithRootWindow(root_window);
    return overview_grid->save_desk_as_template_widget_.get();
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
    auto& grid_list = GetOverviewGridList();
    views::Widget* grid_widget = grid_list[0]->desks_templates_grid_widget();
    ASSERT_TRUE(grid_widget);
    const DesksTemplatesGridView* templates_grid_view =
        static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
    ASSERT_TRUE(templates_grid_view);

    std::vector<DesksTemplatesItemView*> grid_items =
        DesksTemplatesGridViewTestApi(templates_grid_view).grid_items();

    // Check the current grid item count.
    ASSERT_EQ(expected_current_item_count, grid_items.size());

    auto iter =
        std::find_if(grid_items.cbegin(), grid_items.cend(),
                     [&](const DesksTemplatesItemView* v) {
                       return DesksTemplatesItemViewTestApi(v).uuid() == uuid;
                     });

    if (!expect_template_exists) {
      ASSERT_EQ(grid_items.end(), iter);
      return;
    }

    ASSERT_NE(grid_items.end(), iter);

    ClickOnView(DesksTemplatesItemViewTestApi(*iter).delete_button());

    // Clicking on the delete button should bring up the delete dialog.
    ASSERT_TRUE(Shell::IsSystemModalWindowOpen());

    // Click the delete button on the delete dialog. Show delete dialog and
    // select accept.
    auto* dialog_controller = DesksTemplatesDialogController::Get();
    auto* dialog_delegate = dialog_controller->dialog_widget()
                                ->widget_delegate()
                                ->AsDialogDelegate();
    dialog_delegate->AcceptDialog();
    WaitForDesksTemplatesUI();
  }

  // Open overview mode if we're not in overview mode yet, and then show the
  // desks templates grid.
  void OpenOverviewAndShowTemplatesGrid() {
    if (!GetOverviewSession()) {
      ToggleOverview();
      WaitForDesksTemplatesUI();
    }

    ShowDesksTemplatesGrids();
    WaitForDesksTemplatesUI();
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
    return OverviewHighlightController::TestApi(
               GetOverviewSession()->highlight_controller())
        .GetHighlightView();
  }

  // Opens overview mode and then clicks the save template button. This should
  // save a new desk template and open the desks templates grid.
  void OpenOverviewAndSaveTemplate(aura::Window* root) {
    if (!GetOverviewSession()) {
      ToggleOverview();
      WaitForDesksTemplatesUI();
    }

    auto* save_template = GetSaveDeskAsTemplateButtonForRoot(root);
    ASSERT_TRUE(save_template->IsVisible());
    ClickOnView(save_template->GetContentsView());
    WaitForDesksTemplatesUI();
    for (auto& overview_grid : GetOverviewGridList())
      ASSERT_TRUE(overview_grid->IsShowingDesksTemplatesGrid());
  }

  void SetDisableAppIdCheckForDeskTemplates(bool disabled) {
    DesksController::Get()->set_disable_app_id_check_for_desk_templates(
        disabled);
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDesksTemplates);
    OverviewTestBase::SetUp();

    // The FullRestoreSaveHandler isn't setup during tests so every window we
    // create in tests doesn't have an app id associated with it. Since these
    // windows don't have app ids, Desk won't consider them supported windows so
    // we need to disable the app id check during tests.
    DesksController::Get()->set_disable_app_id_check_for_desk_templates(true);
  }

  void TearDown() override {
    DesksController::Get()->set_disable_app_id_check_for_desk_templates(true);
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
TEST_F(DesksTemplatesTest, AddDeleteEntry) {
  const base::GUID expected_uuid = base::GUID::GenerateRandomV4();
  const std::string expected_name = "desk name";
  base::Time expected_time = base::Time::Now();
  AddEntry(expected_uuid, expected_name, expected_time);

  std::vector<DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, entries.size());
  EXPECT_EQ(expected_uuid, entries[0]->uuid());
  EXPECT_EQ(base::UTF8ToUTF16(expected_name), entries[0]->template_name());
  EXPECT_EQ(expected_time, entries[0]->created_time());

  DeleteEntry(expected_uuid);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());
}

// Tests the desks templates button visibility in clamshell mode.
TEST_F(DesksTemplatesTest, DesksTemplatesButtonVisibilityClamshell) {
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
  WaitForDesksTemplatesUI();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*trace_string=*/"one-desk-zero-entries");

  // Exit overview and add an entry.
  ToggleOverview();
  const base::GUID guid = base::GUID::GenerateRandomV4();
  AddEntry(guid, "template", base::Time::Now());

  // Reenter overview and verify the zero state desks templates buttons are
  // visible since there is one entry to view.
  ToggleOverview();
  WaitForDesksTemplatesUI();
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
  WaitForDesksTemplatesUI();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/true,
                             /*trace_string=*/"two-desk-one-entry");

  // Exit overview and delete the entry.
  ToggleOverview();
  DeleteEntry(guid);

  // Reenter overview and verify neither of the buttons are shown.
  ToggleOverview();
  WaitForDesksTemplatesUI();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/false,
                             /*trace_string=*/"two-desk-zero-entries");
}

// Tests that the no windows widget is hidden when the desk templates grid is
// shown.
TEST_F(DesksTemplatesTest, NoWindowsLabelOnTemplateGridShow) {
  UpdateDisplay("400x300,400x300");

  // At least one entry is required for the templates grid to be shown.
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  // Start overview mode. The no windows widget should be visible.
  ToggleOverview();
  WaitForDesksTemplatesUI();
  auto& grid_list = GetOverviewGridList();
  ASSERT_EQ(2u, grid_list.size());
  EXPECT_TRUE(grid_list[0]->no_windows_widget());
  EXPECT_TRUE(grid_list[1]->no_windows_widget());

  // Open the desk templates grid. The no windows widget should now be hidden.
  ShowDesksTemplatesGrids();
  EXPECT_FALSE(grid_list[0]->no_windows_widget());
  EXPECT_FALSE(grid_list[1]->no_windows_widget());
}

// Tests that overview items are hidden when the desk templates grid is shown.
TEST_F(DesksTemplatesTest, HideOverviewItemsOnTemplateGridShow) {
  UpdateDisplay("800x600,800x600");

  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now());

  auto test_window = CreateAppWindow();

  // Start overview mode. The window is visible in the overview mode.
  ToggleOverview();
  WaitForDesksTemplatesUI();
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
TEST_F(DesksTemplatesTest, OverviewItemsStayHiddenInTemplateGridOnDeskClose) {
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now());

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
  WaitForDesksTemplatesUI();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_EQ(1.0f, test_window_2->layer()->opacity());
  auto& overview_grid = GetOverviewGridList()[0];
  EXPECT_FALSE(overview_grid->GetOverviewItemContaining(test_window_1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(test_window_2.get()));

  // Open the desk templates grid. This should hide `test_window_2`.
  ShowDesksTemplatesGrids();
  WaitForDesksTemplatesUI();
  EXPECT_EQ(0.0f, test_window_2->layer()->opacity());

  // While in the desk templates grid, delete the active desk by clicking on the
  // mini view close button.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  auto* mini_view =
      desks_bar_view->FindMiniViewForDesk(desks_controller->active_desk());
  ClickOnView(mini_view->close_desk_button());

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
TEST_F(DesksTemplatesTest, DialogSystemModal) {
  UpdateDisplay("800x600,800x600");

  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  // Show one of the dialogs. Activating the dialog keeps us in overview mode.
  auto* dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->ShowReplaceDialog(Shell::GetPrimaryRootWindow(), u"Bento");
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
TEST_F(DesksTemplatesTest, DesksTemplatesGridItems) {
  UpdateDisplay("800x600,800x600");

  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  const std::string name_1 = "template_1";
  base::Time time_1 = base::Time::Now();
  AddEntry(uuid_1, name_1, time_1);

  const base::GUID uuid_2 = base::GUID::GenerateRandomV4();
  const std::string name_2 = "template_2";
  base::Time time_2 = time_1 + base::Hours(13);
  AddEntry(uuid_2, name_2, time_2);

  OpenOverviewAndShowTemplatesGrid();

  // Check that the grid is populated with the correct number of items, as
  // well as with the correct name and timestamp.
  for (auto& overview_grid : GetOverviewGridList()) {
    views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
    ASSERT_TRUE(grid_widget);
    const DesksTemplatesGridView* templates_grid_view =
        static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
    ASSERT_TRUE(templates_grid_view);
    std::vector<DesksTemplatesItemView*> grid_items =
        DesksTemplatesGridViewTestApi(templates_grid_view).grid_items();

    ASSERT_EQ(2ul, grid_items.size());

    // The grid item order is currently not guaranteed, so need to
    // verify that each template exists by looking them up by their
    // UUID.
    auto verify_template_grid_item = [grid_items](const base::GUID& uuid,
                                                  const std::string& name) {
      auto iter =
          std::find_if(grid_items.cbegin(), grid_items.cend(),
                       [uuid](const DesksTemplatesItemView* v) {
                         return DesksTemplatesItemViewTestApi(v).uuid() == uuid;
                       });
      ASSERT_NE(grid_items.end(), iter);

      DesksTemplatesItemView* item_view = *iter;
      EXPECT_EQ(base::UTF8ToUTF16(name), item_view->name_view()->GetText());
      EXPECT_FALSE(DesksTemplatesItemViewTestApi(item_view)
                       .time_view()
                       ->GetText()
                       .empty());
    };

    verify_template_grid_item(uuid_1, name_1);
    verify_template_grid_item(uuid_2, name_2);
  }
}

// Tests that deleting templates in the templates grid functions correctly.
TEST_F(DesksTemplatesTest, DeleteTemplate) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now());

  const base::GUID uuid_2 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_2, "template_2", base::Time::Now() + base::Hours(13));

  // This window should be hidden whenever the desk templates grid is open.
  auto test_window = CreateAppWindow();

  // This action should record deletions and grid shows in a UMA histogram.
  base::HistogramTester histogram_tester;

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

  // After all templates have been deleted, check to ensure we have exited the
  // Desks Templates Grid on all displays. Also check to make sure the hidden
  // windows and the save template button are shown again.
  EXPECT_EQ(1.0f, test_window->layer()->opacity());
  for (auto& overview_grid : GetOverviewGridList())
    EXPECT_FALSE(overview_grid->IsShowingDesksTemplatesGrid());
  auto* save_template =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(save_template->IsVisible());

  // Assert that histogram metrics were recorded.
  // note: deleting a template that doesn't exist counts as a delete according
  // to the desks model hence expected_deletes being set to two.
  const int expected_deletes = 2;
  histogram_tester.ExpectTotalCount(kDeleteTemplateHistogramName,
                                    expected_deletes);
}

// Tests that the save desk as template button is disabled when the maximum
// number of templates has been reached.
TEST_F(DesksTemplatesTest, SaveDeskAsTemplateButtonDisabled) {
  // Create a test window in the current desk.
  auto test_window = CreateAppWindow();

  aura::Window* root = Shell::GetPrimaryRootWindow();
  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(root);
  // The desks templates grid is now visible and `save_template` is no longer
  // visible, so exit overview to be able to click on it again.
  ToggleOverview();
  // Verify that the entry has been added.
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Verify that the button is disabled after the maximum number of templates
  // have been added.
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template3", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template4", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template5", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template6", base::Time::Now());
  ToggleOverview();
  ASSERT_EQ(6ul, GetAllEntries().size());
  auto* button = static_cast<PillButton*>(
      GetSaveDeskAsTemplateButtonForRoot(root)->GetContentsView());
  EXPECT_EQ(views::Button::STATE_DISABLED, button->GetState());
}

// Tests that clicking the save desk as template button shows the templates
// grid.
TEST_F(DesksTemplatesTest, SaveDeskAsTemplateButtonShowsDesksTemplatesGrid) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();
  WaitForDesksTemplatesUI();

  // The `save_desk_as_template_widget` is visible when at least one window is
  // open.
  views::Widget* save_desk_as_template_widget =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_widget);
  EXPECT_TRUE(save_desk_as_template_widget->GetContentsView()->GetVisible());

  // Click on `save_desk_as_template_widget` button.
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Expect that the Desk Templates grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());
}

// Tests that launching templates from the templates grid functions correctly.
// We test both clicking on the card, as well as clicking on the "Use template"
// button that shows up on hover. Both should do the same thing.
TEST_F(DesksTemplatesTest, LaunchTemplate) {
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateAppWindow();

  // Capture the current desk and open the templates grid.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Click on the grid item to launch the template.
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));
  WaitForDesksTemplatesUI();

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
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  ClickOnView(DesksTemplatesItemViewTestApi(item_view).launch_button());
  WaitForDesksTemplatesUI();

  EXPECT_EQ(3ul, desks_controller->desks().size());
  EXPECT_EQ(2, desks_controller->GetActiveDeskIndex());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that the order of DesksTemplatesItemView is in order.
TEST_F(DesksTemplatesTest, IconsOrder) {
  // Create a `DeskTemplate` using which has 5 apps and each app has 1 window.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(std::vector<int>(5, 1)));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

  // The items previews should be ordered by activation index. Exclude the
  // final DesksTemplatesIconView since it will be the overflow counter.
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

// Tests that icons are ordered such that active tabs and windows are ordered
// before inactive tabs.
TEST_F(DesksTemplatesTest, IconsOrderWithInactiveTabs) {
  const std::string kAppId1 = extension_misc::kChromeAppId;
  constexpr int kWindowId1 = 1;
  constexpr int kActiveTabIndex1 = 1;
  const std::vector<GURL> kTabs1{GURL("http://a.com"), GURL("http://b.com"),
                                 GURL("http://c.com")};

  const std::string kAppId2 = extension_misc::kChromeAppId;
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
           std::move(restore_data));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

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

// Tests that the overflow count view is visible, in bounds, displays the right
// count when there is more than `DesksTemplatesIconContainer::kMaxIcons` icons.
TEST_F(DesksTemplatesTest, OverflowIconView) {
  // Create a `DeskTemplate` using which has 1 app more than the max and each
  // app has 1 window.
  const int kNumOverflowApps = 1;
  std::vector<int> window_info(
      kNumOverflowApps + DesksTemplatesIconContainer::kMaxIcons, 1);
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

  // There should only be the max number of icons plus the overflow icon.
  EXPECT_EQ(DesksTemplatesIconContainer::kMaxIcons + 1,
            static_cast<int>(icon_views.size()));

  // The overflow counter should have no identifier and its count should be
  // non-zero. It should also be visible and within the bounds of the host
  // DesksTemplatesItemView.
  DesksTemplatesIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+1", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.desks_templates_icon_view()->GetVisible());
  EXPECT_TRUE(
      item_view->Contains(overflow_icon_view.desks_templates_icon_view()));
}

// Tests that when there isn't enough space to display
// `DesksTemplatesIconContainer::kMaxIcons` icons and the overflow
// icon view, the overflow icon view is visible and its count incremented by the
// number of icons that had to be hidden.
TEST_F(DesksTemplatesTest, OverflowIconViewIncrementsForHiddenIcons) {
  // Create a `DeskTemplate` using which has 3 apps more than
  // `DesksTemplatesIconContainer::kMaxIcons` and each app has 2 windows.
  const int kNumOverflowApps = 3;
  std::vector<int> window_info(
      kNumOverflowApps + DesksTemplatesIconContainer::kMaxIcons, 2);
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

  // Even though there are more than `DesksTemplatesIconContainer::kMaxIcons`,
  // there should still be `DesksTemplatesIconContainer::kMaxIcons`+ 1
  // DesksTemplatesIconView's created.
  EXPECT_EQ(icon_views.size(), DesksTemplatesIconContainer::kMaxIcons + 1u);

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
  // non-zero, accounting for the number of hidden app icons. It should also be
  // visible and within the bounds of the host DesksTemplatesItemView.
  DesksTemplatesIconViewTestApi overflow_icon_view{icon_views.back()};
  EXPECT_FALSE(overflow_icon_view.icon_view());
  EXPECT_TRUE(overflow_icon_view.count_label());
  EXPECT_EQ(u"+5", overflow_icon_view.count_label()->GetText());
  EXPECT_TRUE(overflow_icon_view.desks_templates_icon_view()->GetVisible());
  EXPECT_TRUE(
      item_view->Contains(overflow_icon_view.desks_templates_icon_view()));
}

// Tests that when an app has more than 9 windows, its label is changed to "9+".
TEST_F(DesksTemplatesTest, IconViewMoreThan9Windows) {
  // Create a `DeskTemplate` using which has 1 app with 10 windows.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(std::vector<int>{10}));

  // Enter overview and show the Desks Templates Grid.
  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

  // There should only be 1 icon view for the app and 1 icon view for the
  // overflow.
  EXPECT_EQ(2u, icon_views.size());

  // The app's icon view should have a "9+" label.
  DesksTemplatesIconViewTestApi icon_view(icon_views[0]);
  EXPECT_TRUE(icon_view.icon_view());
  EXPECT_TRUE(icon_view.count_label());
  EXPECT_EQ(u"9+", icon_view.count_label()->GetText());

  // The overflow counter should not be visible.
  EXPECT_FALSE(icon_views.back()->GetVisible());
}

// Tests that when there are less than `DesksTemplatesIconContainer::kMaxIcons`
// the overflow icon is not visible.
TEST_F(DesksTemplatesTest, OverflowIconViewHiddenOnNoOverflow) {
  // Create a `DeskTemplate` using which has
  // `DesksTemplatesIconContainer::kMaxIcons` apps and each app has 1 window.
  std::vector<int> window_info(DesksTemplatesIconContainer::kMaxIcons, 1);
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(window_info));

  OpenOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromTemplatesGrid(
      /*grid_item_index=*/0);
  const std::vector<DesksTemplatesIconView*>& icon_views =
      DesksTemplatesItemViewTestApi(item_view).icon_views();

  // All the icon views should be visible and the overflow icon view should be
  // invisible.
  for (size_t i = 0; i < icon_views.size() - 1; ++i)
    EXPECT_TRUE(icon_views[i]->GetVisible());
  EXPECT_FALSE(icon_views.back()->GetVisible());
}

// Tests that the desks templates and save desk template buttons are hidden when
// entering overview in tablet mode.
TEST_F(DesksTemplatesTest, EnteringInTabletMode) {
  // Create a desk before entering tablet mode, otherwise the desks bar will not
  // show up.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up in clamshell mode either.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Test that the templates buttons are created but invisible. The save desk as
  // template button is not created.
  ToggleOverview();
  WaitForDesksTemplatesUI();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* zero_state = GetDesksTemplatesButtonForRoot(root,
                                                    /*zero_state=*/true);
  auto* expanded_state = GetDesksTemplatesButtonForRoot(root,
                                                        /*zero_state=*/false);
  EXPECT_FALSE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_FALSE(GetSaveDeskAsTemplateButtonForRoot(root));
}

// Tests that the desks templates and save desk template buttons are hidden when
// transitioning from clamshell to tablet mode.
TEST_F(DesksTemplatesTest, ClamshellToTabletMode) {
  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  // Test that on entering overview, the zero state desks templates button and
  // the save template button are visible.
  ToggleOverview();
  WaitForDesksTemplatesUI();
  aura::Window* root = Shell::GetPrimaryRootWindow();
  auto* zero_state = GetDesksTemplatesButtonForRoot(root,
                                                    /*zero_state=*/true);
  auto* expanded_state = GetDesksTemplatesButtonForRoot(root,
                                                        /*zero_state=*/false);
  auto* save_template = GetSaveDeskAsTemplateButtonForRoot(root);
  EXPECT_TRUE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_TRUE(save_template->IsVisible());

  // Tests that after transitioning, we remain in overview mode and all the
  // buttons are invisible.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(zero_state->GetVisible());
  EXPECT_FALSE(expanded_state->GetVisible());
  EXPECT_FALSE(save_template->IsVisible());
}

// Tests that the desks templates grid gets hidden when transitioning to tablet
// mode.
TEST_F(DesksTemplatesTest, ShowingTemplatesGridToTabletMode) {
  // Create a window and add a test entry. Otherwise the templates UI wouldn't
  // show up.
  auto test_window_1 = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ASSERT_TRUE(GetOverviewSession()
                  ->GetGridWithRootWindow(root_window)
                  ->desks_templates_grid_widget()
                  ->IsVisible());

  // Tests that after transitioning, we remain in overview mode and the grid is
  // hidden.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_FALSE(GetOverviewSession()
                   ->GetGridWithRootWindow(root_window)
                   ->desks_templates_grid_widget()
                   ->IsVisible());
}

TEST_F(DesksTemplatesTest, OverviewTabbing) {
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template1", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesItemView* first_item = GetItemViewFromTemplatesGrid(0);
  DesksTemplatesItemView* second_item = GetItemViewFromTemplatesGrid(1);

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

// Tests that the desks bar returns to zero state if the second-to-last desk is
// deleted while viewing the templates grid. Also verifies that the zero state
// buttons are visible. Regression test for https://crbug.com/1264989.
TEST_F(DesksTemplatesTest, DesksBarReturnsToZeroState) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const base::GUID uuid = base::GUID::GenerateRandomV4();
  AddEntry(uuid, "template", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(root_window);
  const auto* desks_bar_view = overview_grid->desks_bar_view();

  // Close one of the desks. Test that we remain in expanded state.
  auto* mini_view = desks_bar_view->FindMiniViewForDesk(
      DesksController::Get()->active_desk());
  ClickOnView(mini_view->close_desk_button());
  auto* expanded_new_desk_button =
      desks_bar_view->expanded_state_new_desk_button();
  auto* expanded_templates_button =
      desks_bar_view->expanded_state_desks_templates_button();
  EXPECT_TRUE(expanded_new_desk_button->GetVisible());
  EXPECT_TRUE(expanded_templates_button->GetVisible());
  EXPECT_FALSE(desks_bar_view->IsZeroState());

  // Delete the one and only template, which should hide the templates grid.
  DeleteTemplate(uuid, /*expected_current_item_count=*/1);
  EXPECT_FALSE(GetOverviewSession()
                   ->GetGridWithRootWindow(root_window)
                   ->desks_templates_grid_widget()
                   ->IsVisible());

  // Test that we are now in zero state.
  auto* zero_new_desk_button = desks_bar_view->zero_state_new_desk_button();
  EXPECT_TRUE(zero_new_desk_button->GetVisible());
  EXPECT_TRUE(desks_bar_view->IsZeroState());
}

// Tests that the unsupported apps dialog is shown when a user attempts to save
// an active desk with unsupported apps.
TEST_F(DesksTemplatesTest, UnsupportedAppsDialog) {
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
  WaitForDesksTemplatesUI();
  auto* save_template = GetSaveDeskAsTemplateButtonForRoot(root);
  ASSERT_TRUE(save_template->IsVisible());
  ClickOnView(save_template->GetContentsView());
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Decline the dialog. We should stay in overview and no template should have
  // been saved.
  auto* dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->dialog_widget()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->CancelDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(Shell::IsSystemModalWindowOpen());
  EXPECT_TRUE(GetOverviewSession());

  // Click on the save template button again. The unsupported apps dialog should
  // show up.
  ClickOnView(save_template->GetContentsView());
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());

  // Accept the dialog. The template should have been saved and the templates
  // grid should now be shown.
  dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->dialog_widget()
      ->widget_delegate()
      ->AsDialogDelegate()
      ->AcceptDialog();
  WaitForDesksTemplatesUI();
  EXPECT_TRUE(GetOverviewSession());
  EXPECT_TRUE(GetOverviewGridList()[0]->desks_templates_grid_widget());

  ASSERT_EQ(1ul, GetAllEntries().size());
}

// Tests that the save desk as template button is disabled when all windows on
// the desk are unsupported or there are no windows with Full Restore app ids.
// See crbug.com/1277763.
TEST_F(DesksTemplatesTest, AllUnsupportedAppsDisablesSaveTemplates) {
  SetDisableAppIdCheckForDeskTemplates(false);

  // Use `CreateTestWindow()` instead of `CreateAppWindow()`, which by default
  // creates a supported window.
  auto test_window = CreateTestWindow();

  // Also create an app window which should be supported and not have an app id.
  auto no_app_id_window = CreateAppWindow();
  auto* delegate = Shell::Get()->desks_templates_delegate();
  ASSERT_TRUE(
      delegate->IsWindowSupportedForDeskTemplate(no_app_id_window.get()));
  ASSERT_TRUE(full_restore::GetAppId(no_app_id_window.get()).empty());

  EXPECT_EQ(0, DesksController::Get()->active_desk()->num_supported_windows());

  ToggleOverview();

  auto* save_template = static_cast<PillButton*>(
      GetSaveDeskAsTemplateButtonForRoot(Shell::Get()->GetPrimaryRootWindow())
          ->GetContentsView());
  EXPECT_EQ(views::Button::STATE_DISABLED, save_template->GetState());
}

TEST_F(DesksTemplatesTest, AddRemoveSupportedWindows) {
  auto* controller = DesksController::Get();

  // Create a desk other than the default initial desk.
  NewDesk();

  Desk* desk_1 = controller->desks()[0].get();

  // Create 3 supported windows on desk_1.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto win1 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));
  auto win2 = CreateAppWindow(gfx::Rect(50, 50, 200, 200));

  // Expect `num_supported_windows_` to be 3.
  EXPECT_EQ(3, desk_1->num_supported_windows());

  // Close the supported windows.
  win0.reset();
  win1.reset();
  win2.reset();

  // Expect `num_supported_windows_` to be 0.
  EXPECT_EQ(0, desk_1->num_supported_windows());
}

// Tests the mouse and touch hover behavior on the template item view.
TEST_F(DesksTemplatesTest, HoverOnTemplateItemView) {
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template1", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "template2", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesItemView* first_item = GetItemViewFromTemplatesGrid(0);
  DesksTemplatesItemView* second_item = GetItemViewFromTemplatesGrid(1);
  auto* hover_container_view1 =
      DesksTemplatesItemViewTestApi(first_item).hover_container();
  auto* hover_container_view2 =
      DesksTemplatesItemViewTestApi(second_item).hover_container();
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

  // Test to make sure hover is hidden on all items when dragging to off the
  // grid.
  event_generator->DragMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(hover_container_view1->GetVisible());
  EXPECT_FALSE(hover_container_view2->GetVisible());
}

// Tests that when a supported app doesn't have any app launch info and a
// template is saved, the unsupported apps dialog isn't shown. See
// crbug.com/1269466.
TEST_F(DesksTemplatesTest, DialogDoesntShowForSupportedAppsWithoutLaunchInfo) {
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
TEST_F(DesksTemplatesTest, LaunchTemplateWithMinimizedOverviewWindow) {
  // Create a test minimized window.
  auto window = CreateAppWindow();
  WindowState::Get(window.get())->Minimize();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Click on the grid item to launch the template. We should remain in overview
  // and there should be no crash.
  ClickOnView(GetItemViewFromTemplatesGrid(/*grid_item_index=*/0));
  // Launching a template fetches it from the desk model asynchronously.
  WaitForDesksTemplatesUI();

  EXPECT_TRUE(InOverviewSession());
}

// Tests that there is no crash if we launch a template after deleting the
// active desk. Regression test for https://crbug.com/1277203.
TEST_F(DesksTemplatesTest, LaunchTemplateAfterClosingActiveDesk) {
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
  // Launching a template fetches it from the desk model asynchronously.
  WaitForDesksTemplatesUI();

  EXPECT_TRUE(InOverviewSession());
}

// Tests that if we open the desks templates grid a second time during an
// overview session, we can still see the template items. Opening a second time
// can be done after deleting all the templates from the first open. Regression
// test for https://crbug.com/1275179.
TEST_F(DesksTemplatesTest, TemplatesAreVisibleAfterSecondSave) {
  // One window is needed to save a template.
  auto window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<DeskTemplate*> entries = GetAllEntries();
  ASSERT_EQ(1ul, entries.size());

  // Delete the one and only template, which should hide the templates grid but
  // remain in overview.
  DeleteTemplate(entries[0]->uuid(), /*expected_current_item_count=*/1);
  ASSERT_TRUE(InOverviewSession());

  // Open overview and save a template again.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
  ASSERT_TRUE(grid_widget);
  const DesksTemplatesGridView* templates_grid_view =
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
  ASSERT_TRUE(templates_grid_view);

  std::vector<DesksTemplatesItemView*> grid_items =
      DesksTemplatesGridViewTestApi(templates_grid_view).grid_items();
  ASSERT_EQ(1ul, grid_items.size());

  // Tests that bounds of the views are not empty.
  EXPECT_FALSE(templates_grid_view->bounds().IsEmpty());
  EXPECT_FALSE(grid_items[0]->bounds().IsEmpty());
}

// Tests that the desks templates are organized in alphabetical order.
TEST_F(DesksTemplatesTest, ShowTemplatesInAlphabeticalOrder) {
  // Create a window and add three test entry in different names.
  auto test_window = CreateAppWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "B_template", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "1_template", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "A_template", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();

  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
  ASSERT_TRUE(grid_widget);
  const DesksTemplatesGridView* templates_grid_view =
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
  ASSERT_TRUE(templates_grid_view);

  views::View::Views grid_views = templates_grid_view->children();
  ASSERT_EQ(3ul, grid_views.size());

  // Tests that templates are sorted in alphabetical order.
  EXPECT_EQ(
      u"1_template",
      static_cast<DesksTemplatesItemView*>(grid_views[0])->GetAccessibleName());
  EXPECT_EQ(
      u"A_template",
      static_cast<DesksTemplatesItemView*>(grid_views[1])->GetAccessibleName());
  EXPECT_EQ(
      u"B_template",
      static_cast<DesksTemplatesItemView*>(grid_views[2])->GetAccessibleName());
}

// Tests that the color of the desks templates button border is as expected.
// Regression test for https://crbug.com/1265003.
TEST_F(DesksTemplatesTest, DesksTemplatesButtonBorderColor) {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kKeyboard);
  AddEntry(base::GUID::GenerateRandomV4(), "name", base::Time::Now());

  auto* color_provider = AshColorProvider::Get();
  const SkColor active_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kCurrentDeskColor);
  const SkColor focused_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor);

  ToggleOverview();
  WaitForDesksTemplatesUI();

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
TEST_F(DesksTemplatesTest, WindowActivatableAfterSaveAndDeleteTemplate) {
  // Create a test window.
  auto test_window = CreateAppWindow();

  // Open overview and save a template.
  OpenOverviewAndSaveTemplate(Shell::Get()->GetPrimaryRootWindow());
  std::vector<DeskTemplate*> entries = GetAllEntries();
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

// Tests that we are able to edit the template name.
TEST_F(DesksTemplatesTest, EditTemplateName) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

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
}

// Tests for checking that certain conditions will revert the template name to
// its original name, even if the text in the textfield has been updated.
TEST_F(DesksTemplatesTest, TemplateNameChangeAborted) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  OverviewGrid* overview_grid = GetOverviewGridList()[0].get();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

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
  SendKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  SendKey(ui::VKEY_BACK);
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
}

// When template names exceed the width of the textfield, we elide the text
// (truncate the string add "..." to the end) for display purposese. The full
// template name is still stored as the `full_text()`. This test checks to
// ensure that the elided text is displayed when the textfield is not focused,
// but the full text is populated into the textfield when it is focused.
TEST_F(DesksTemplatesTest, TemplateNameEllipsis) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk template name that is very long";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

  // Verify that the template name field has an ellipsis.
  EXPECT_NE(base::UTF8ToUTF16(template_name), name_view->GetText());
  EXPECT_NE(std::string::npos, name_view->GetText().find(u"\x2026"));

  // Verify that the full text stored by the textfield is the `template_name`.
  EXPECT_EQ(base::UTF8ToUTF16(template_name),
            DesksTemplatesNameViewTestApi(name_view).full_text());

  // Test that when we click on and focus the template name, that the ellipsized
  // text is replaced with the full template name. Also tests that it's hidden
  // again when we lose focus.
  ClickOnView(name_view);
  EXPECT_TRUE(name_view->HasFocus());
  EXPECT_EQ(base::UTF8ToUTF16(template_name), name_view->GetText());
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(name_view->HasFocus());
  EXPECT_NE(base::UTF8ToUTF16(template_name), name_view->GetText());
  EXPECT_NE(std::string::npos, name_view->GetText().find(u"\x2026"));

  // Test that adding onto a long `template_name` will still update and
  // ellipsize correctly.
  ClickOnView(name_view);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_B);
  SendKey(ui::VKEY_RETURN);
  WaitForDesksTemplatesUI();
  name_view = GetItemViewFromTemplatesGrid(0)->name_view();
  EXPECT_EQ(base::UTF8ToUTF16(template_name) + u"ab",
            DesksTemplatesNameViewTestApi(name_view).full_text());
}

// Tesets that Showing the overview records to the TemplateGrid histogram.
TEST_F(DesksTemplatesTest, RecordDesksTemplateGridShows) {
  // Make sure that LoadTemplateHistogram is recorded.
  base::HistogramTester histogram_tester;

  // Entry needed so that overview is accessible
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();

  // Assert load grid histogram recorded.
  constexpr int kExpectedGridShows = 1;
  histogram_tester.ExpectTotalCount(kLoadTemplateGridHistogramName,
                                    kExpectedGridShows);
}

// Tests that deleting templates in the templates grid Records to the delete
// template histogram.
TEST_F(DesksTemplatesTest, DeleteTemplateRecordsMetric) {
  UpdateDisplay("800x600,800x600");

  // Populate with several entries.
  const base::GUID uuid_1 = base::GUID::GenerateRandomV4();
  AddEntry(uuid_1, "template_1", base::Time::Now());

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

  // There is only one desk to delete in this test so we should have
  // exited the overview.
  EXPECT_EQ(1.0f, test_window->layer()->opacity());

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
TEST_F(DesksTemplatesTest, LaunchTemplateRecordsMetric) {
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
  WaitForDesksTemplatesUI();

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
TEST_F(DesksTemplatesTest, SaveDeskAsTemplateRecordsMetric) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateAppWindow();
  ToggleOverview();
  WaitForDesksTemplatesUI();

  // Record histogram
  base::HistogramTester histogram_tester;

  // The `save_desk_as_template_widget` is visible when at least one window is
  // open.
  views::Widget* save_desk_as_template_widget =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_widget);
  EXPECT_TRUE(save_desk_as_template_widget->GetContentsView()->GetVisible());

  // Click on `save_desk_as_template_widget` button.
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Expect that the Desk Templates grid is visible.
  EXPECT_TRUE(GetOverviewGridList()[0]->IsShowingDesksTemplatesGrid());

  // Assert that there was a new template event recorded to the proper
  // histogram.
  constexpr int kExpectedNewTemplates = 1;
  histogram_tester.ExpectTotalCount(kNewTemplateHistogramName,
                                    kExpectedNewTemplates);
}

// Tests to verify that clicking the spacebar doesn't cause the name view to
// lose focus (since it's within a button), and that whitespaces are handled
// correctly.
TEST_F(DesksTemplatesTest, TemplateNameTestSpaces) {
  auto test_window = CreateAppWindow();

  const std::string template_name = "desk name";
  AddEntry(base::GUID::GenerateRandomV4(), template_name, base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

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
TEST_F(DesksTemplatesTest, EditTemplateNameWithKeyboardNoCrash) {
  AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "b", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

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
TEST_F(DesksTemplatesTest, EditTemplateNameShutdownNoCrash) {
  // The fade out animation of the desks templates grid must be enabled for this
  // crash to have happened.
  animation_scale_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now());
  AddEntry(base::GUID::GenerateRandomV4(), "b", base::Time::Now());

  OpenOverviewAndShowTemplatesGrid();
  DesksTemplatesNameView* name_view =
      GetItemViewFromTemplatesGrid(0)->name_view();

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
TEST_F(DesksTemplatesTest, TemplatesNameHitTest) {
  auto* cursor_manager = Shell::Get()->cursor_manager();

  for (bool is_rtl : {true, false}) {
    SCOPED_TRACE(is_rtl ? "rtl" : "ltr");
    base::i18n::SetRTLForTesting(is_rtl);

    AddEntry(base::GUID::GenerateRandomV4(), "a", base::Time::Now());

    OpenOverviewAndShowTemplatesGrid();
    DesksTemplatesNameView* name_view =
        GetItemViewFromTemplatesGrid(0)->name_view();
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

}  // namespace ash
