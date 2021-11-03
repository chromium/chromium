// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/button_style.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_delete_button.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// Wrapper for DesksTemplatesPresenter that exposes internal state to test
// functions.
class DesksTemplatesPresenterTestApi {
 public:
  explicit DesksTemplatesPresenterTestApi(DesksTemplatesPresenter* presenter)
      : presenter_(presenter) {
    DCHECK(presenter_);
  }
  DesksTemplatesPresenterTestApi(const DesksTemplatesPresenterTestApi&) =
      delete;
  DesksTemplatesPresenterTestApi& operator=(
      const DesksTemplatesPresenterTestApi&) = delete;
  ~DesksTemplatesPresenterTestApi() = default;

  void SetOnUpdateUiClosure(base::OnceClosure closure) {
    DCHECK(!presenter_->on_update_ui_closure_for_testing_);
    presenter_->on_update_ui_closure_for_testing_ = std::move(closure);
  }

 private:
  DesksTemplatesPresenter* const presenter_;
};

// Wrapper for DesksTemplatesGridView that exposes internal state to test
// functions.
class DesksTemplatesGridViewTestApi {
 public:
  explicit DesksTemplatesGridViewTestApi(
      const DesksTemplatesGridView* grid_view)
      : grid_view_(grid_view) {
    DCHECK(grid_view_);
  }
  DesksTemplatesGridViewTestApi(const DesksTemplatesGridViewTestApi&) = delete;
  DesksTemplatesGridViewTestApi& operator=(
      const DesksTemplatesGridViewTestApi&) = delete;
  ~DesksTemplatesGridViewTestApi() = default;

  const std::vector<DesksTemplatesItemView*>& grid_items() const {
    return grid_view_->grid_items_;
  }

 private:
  const DesksTemplatesGridView* grid_view_;
};

// Wrapper for DesksTemplatesItemView that exposes internal state to test
// functions.
class DesksTemplatesItemViewTestApi {
 public:
  explicit DesksTemplatesItemViewTestApi(
      const DesksTemplatesItemView* item_view)
      : item_view_(item_view) {
    DCHECK(item_view_);
  }
  DesksTemplatesItemViewTestApi(const DesksTemplatesItemViewTestApi&) = delete;
  DesksTemplatesItemViewTestApi& operator=(
      const DesksTemplatesItemViewTestApi&) = delete;
  ~DesksTemplatesItemViewTestApi() = default;

  const views::Textfield* name_view() const { return item_view_->name_view_; }

  const views::Label* time_view() const { return item_view_->time_view_; }

  const DesksTemplatesDeleteButton* delete_button() const {
    return item_view_->delete_button_;
  }

  const base::GUID uuid() const { return item_view_->uuid_; }

  const std::vector<DesksTemplatesIconView*>& icon_views() const {
    return item_view_->icon_container_view_->icon_views_;
  }

 private:
  const DesksTemplatesItemView* item_view_;
};

// Wrapper for DesksTemplatesIconView that exposes internal state to test
// functions.
class DesksTemplatesIconViewTestApi {
 public:
  explicit DesksTemplatesIconViewTestApi(
      const DesksTemplatesIconView* desks_templates_icon_view)
      : desks_templates_icon_view_(desks_templates_icon_view) {
    DCHECK(desks_templates_icon_view_);
  }
  DesksTemplatesIconViewTestApi(const DesksTemplatesIconViewTestApi&) = delete;
  DesksTemplatesIconViewTestApi& operator=(
      const DesksTemplatesIconViewTestApi&) = delete;
  ~DesksTemplatesIconViewTestApi() = default;

  const views::Label* count_label() const {
    return desks_templates_icon_view_->count_label_;
  }

  const RoundedImageView* icon_view() const {
    return desks_templates_icon_view_->icon_view_;
  }

  const DesksTemplatesIconView* desks_templates_icon_view() const {
    return desks_templates_icon_view_;
  }

 private:
  const DesksTemplatesIconView* desks_templates_icon_view_;
};

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

  // Return the `grid_item_index`th DesksTemplatesItemView from the first
  // OverviewGrid in `GetOverviewGridList()`.
  DesksTemplatesItemView* GetItemViewFromOverviewGrid(int grid_item_index) {
    views::Widget* grid_widget =
        GetOverviewGridList().front()->desks_templates_grid_widget();
    DCHECK(grid_widget);

    const DesksTemplatesGridView* templates_grid_view =
        static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
    DCHECK(templates_grid_view);

    std::vector<DesksTemplatesItemView*> grid_items =
        DesksTemplatesGridViewTestApi(templates_grid_view).grid_items();
    DesksTemplatesItemView* item_view = grid_items.at(grid_item_index);
    DCHECK(item_view);

    return item_view;
  }

  // Gets the current list of template entries from the desk model directly
  // without updating the UI.
  const std::vector<DeskTemplate*> GetAllEntries() {
    std::vector<DeskTemplate*> templates;

    base::RunLoop loop;
    desk_model()->GetAllEntries(base::BindLambdaForTesting(
        [&](desks_storage::DeskModel::GetAllEntriesStatus status,
            std::vector<DeskTemplate*> entries) {
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

  // A lot of the UI relies on calling into the local desk data manager to
  // update, which sends callbacks via posting tasks. Call `WaitForUI()` if
  // testing a piece of the UI which calls into the desk model.
  void WaitForUI() {
    auto* overview_session = GetOverviewSession();
    DCHECK(overview_session);

    base::RunLoop run_loop;
    DesksTemplatesPresenterTestApi(
        overview_session->desks_templates_presenter())
        .SetOnUpdateUiClosure(run_loop.QuitClosure());
    run_loop.Run();
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

  // Toggles overview mode and then shows the desks templates grid.
  void ToggleOverviewAndShowTemplatesGrid() {
    ToggleOverview();
    WaitForUI();
    ShowDesksTemplatesGrids();
    WaitForUI();
  }

  void ClickOnView(const views::View* view) {
    DCHECK(view);

    const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(view_center);
    event_generator->ClickLeftButton();
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& GetOverviewGridList() {
    auto* overview_session = GetOverviewSession();
    DCHECK(overview_session);

    return overview_session->grid_list();
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDesksTemplates);
    OverviewTestBase::SetUp();
  }

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
  WaitForUI();
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
  WaitForUI();
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
  WaitForUI();
  verify_button_visibilities(/*zero_state_shown=*/false,
                             /*expanded_state_shown=*/true,
                             /*trace_string=*/"two-desk-one-entry");

  // Exit overview and delete the entry.
  ToggleOverview();
  DeleteEntry(guid);

  // Reenter overview and verify neither of the buttons are shown.
  ToggleOverview();
  WaitForUI();
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
  WaitForUI();
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

  auto test_window = CreateTestWindow();

  // Start overview mode. The window is visible in the overview mode.
  ToggleOverview();
  WaitForUI();
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
  auto test_window_1 = CreateTestWindow();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(1ul, desks_controller->active_desk()->windows().size());

  // Create and activate a new desk, and create a test window there.
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  Desk* desk = desks_controller->desks().back().get();
  ActivateDesk(desk);
  auto test_window_2 = CreateTestWindow();
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
  WaitForUI();
  ASSERT_TRUE(GetOverviewSession());
  EXPECT_EQ(1.0f, test_window_2->layer()->opacity());
  auto& overview_grid = GetOverviewGridList()[0];
  EXPECT_FALSE(overview_grid->GetOverviewItemContaining(test_window_1.get()));
  EXPECT_TRUE(overview_grid->GetOverviewItemContaining(test_window_2.get()));

  // Open the desk templates grid. This should hide `test_window_2`.
  ShowDesksTemplatesGrids();
  WaitForUI();
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

  ToggleOverviewAndShowTemplatesGrid();

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
      DesksTemplatesItemViewTestApi test_api(*iter);
      EXPECT_EQ(base::UTF8ToUTF16(name), test_api.name_view()->GetText());
      EXPECT_FALSE(test_api.time_view()->GetText().empty());
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
  auto test_window = CreateTestWindow();

  ToggleOverviewAndShowTemplatesGrid();

  // The window is hidden because the desk templates grid is open.
  EXPECT_EQ(0.0f, test_window->layer()->opacity());

  auto& grid_list = GetOverviewGridList();
  ASSERT_EQ(2u, grid_list.size());
  views::Widget* grid_widget = grid_list[0]->desks_templates_grid_widget();
  ASSERT_TRUE(grid_widget);
  const DesksTemplatesGridView* templates_grid_view =
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
  ASSERT_TRUE(templates_grid_view);

  // Helper function for attempting to delete a template based on its uuid. Also
  // checks if the grid item count is as expected before deleting.
  auto delete_template = [&](const base::GUID uuid,
                             const size_t expected_current_item_count,
                             bool expect_template_exists = true) {
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
    WaitForUI();
  };

  EXPECT_EQ(2ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_1`.
  delete_template(uuid_1, /*expected_current_item_count=*/2);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Verifies that the template with `uuid_1`, doesn't exist anymore.
  delete_template(uuid_1, /*expected_current_item_count=*/1,
                  /*expect_template_exists=*/false);
  EXPECT_EQ(0.0f, test_window->layer()->opacity());
  EXPECT_EQ(1ul, desk_model()->GetEntryCount());

  // Delete the template with `uuid_2`.
  delete_template(uuid_2, /*expected_current_item_count=*/1);
  EXPECT_EQ(0ul, desk_model()->GetEntryCount());

  // After all templates have been deleted, check to ensure we have exited the
  // Desks Templates Grid on all displays. Also check to make sure the hidden
  // windows are shown again.
  EXPECT_EQ(1.0f, test_window->layer()->opacity());
  for (auto& overview_grid : GetOverviewGridList())
    EXPECT_FALSE(overview_grid->IsShowingDesksTemplatesGrid());
}

// Tests that the SaveDeskAsTemplate button is disabled when the max number of
// templates has been reached.
TEST_F(DesksTemplatesTest, SaveDeskAsTemplateButtonDisabledOnMaxTemplates) {
  // There are no saved template entries and one test window initially.
  auto test_window = CreateTestWindow();
  ToggleOverview();
  WaitForUI();

  // The `save_desk_as_template_widget` is visible when at least one window is
  // open.
  views::Widget* save_desk_as_template_widget =
      GetSaveDeskAsTemplateButtonForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(save_desk_as_template_widget);
  EXPECT_TRUE(save_desk_as_template_widget->GetContentsView()->GetVisible());

  // Verify that the entry has been added.
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ASSERT_EQ(1ul, GetAllEntries().size());

  // Verify that the button is disabled after 5 more entries are added.
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ClickOnView(save_desk_as_template_widget->GetContentsView());
  ASSERT_EQ(6ul, GetAllEntries().size());
  auto* button =
      static_cast<PillButton*>(save_desk_as_template_widget->GetContentsView());
  EXPECT_EQ(views::Button::STATE_DISABLED, button->GetState());
}

// Tests that launching templates from the templates grid functions correctly.
TEST_F(DesksTemplatesTest, LaunchTemplate) {
  DesksController* desks_controller = DesksController::Get();
  ASSERT_EQ(0, desks_controller->GetActiveDeskIndex());

  auto test_window = CreateTestWindow();

  // Capture the current desk .
  // TODO: Change this once the save desk button is implemented.
  std::unique_ptr<DeskTemplate> desk_template =
      desks_controller->CaptureActiveDeskAsTemplate();
  AddEntry(std::move(desk_template));

  ToggleOverviewAndShowTemplatesGrid();

  // Click on the grid item to launch the template.
  DeskSwitchAnimationWaiter waiter;
  ClickOnView(GetItemViewFromOverviewGrid(/*grid_item_index=*/0));
  WaitForUI();
  waiter.Wait();

  // Verify that we have created and activated a new desk.
  EXPECT_EQ(2ul, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());

  // Launching a template creates and activates a new desk, which also results
  // in exiting overview mode, so we check to make sure overview is closed.
  EXPECT_FALSE(InOverviewSession());
}

// Tests that the order of DesksTemplatesItemView is in order.
// TODO(chinsenj): Once ordering is finalized, update this comment to reflect
// final ordering. Also this test case should use Browsers as well.
TEST_F(DesksTemplatesTest, IconsOrder) {
  // Create a `DeskTemplate` using which has 5 apps and each app has 1 window.
  AddEntry(base::GUID::GenerateRandomV4(), "template_1", base::Time::Now(),
           CreateRestoreData(std::vector<int>(5, 1)));

  ToggleOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromOverviewGrid(
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

  ToggleOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromOverviewGrid(
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

  ToggleOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromOverviewGrid(
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
  ToggleOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromOverviewGrid(
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

  ToggleOverviewAndShowTemplatesGrid();

  // Get the icon views.
  DesksTemplatesItemView* item_view = GetItemViewFromOverviewGrid(
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
  auto test_window_1 = CreateTestWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // Test that the templates buttons are created but invisible. The save desk as
  // template button is not created.
  ToggleOverview();
  WaitForUI();
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
  auto test_window_1 = CreateTestWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  // Test that on entering overview, the zero state desks templates button and
  // the save template button are visible.
  ToggleOverview();
  WaitForUI();
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
  auto test_window_1 = CreateTestWindow();
  AddEntry(base::GUID::GenerateRandomV4(), "template", base::Time::Now());

  // Enter overview and show the templates grid.
  ToggleOverview();
  WaitForUI();
  ShowDesksTemplatesGrids();
  WaitForUI();
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

}  // namespace ash
