// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "ui/aura/window.h"

namespace ash {

class CustomTestShellDelegate : public TestShellDelegate {
 public:
  explicit CustomTestShellDelegate(desks_storage::DeskModel* desk_model)
      : desk_model_(desk_model) {}
  CustomTestShellDelegate(const CustomTestShellDelegate&) = delete;
  CustomTestShellDelegate& operator=(const CustomTestShellDelegate&) = delete;
  ~CustomTestShellDelegate() override = default;

  // TestShellDelegate:
  desks_storage::DeskModel* GetDeskModel() override { return desk_model_; }

 private:
  // The desk model for the desks templates feature.
  desks_storage::DeskModel* const desk_model_;
};

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

class DesksTemplatesTest : public OverviewTestBase {
 public:
  DesksTemplatesTest() = default;
  DesksTemplatesTest(const DesksTemplatesTest&) = delete;
  DesksTemplatesTest& operator=(const DesksTemplatesTest&) = delete;
  ~DesksTemplatesTest() override = default;

  desks_storage::LocalDeskDataManager* desk_model() {
    return desk_model_.get();
  }

  // Adds an entry to the desks model directly without capturing a desk. Allows
  // for testing the names and times of the UI directly.
  void AddEntry(const base::GUID& uuid,
                const std::string& name,
                base::Time created_time) {
    auto desk_template = std::make_unique<DeskTemplate>(
        uuid.AsLowercaseString(), name, created_time);
    desk_template->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());

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
    const auto* desks_bar_view = overview_grid->desks_bar_view();
    if (zero_state)
      return desks_bar_view->zero_state_desks_templates_button();
    return desks_bar_view->expanded_state_desks_templates_button();
  }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDesksTemplates);

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    desk_model_ = std::make_unique<desks_storage::LocalDeskDataManager>(
        temp_dir_.GetPath());

    // This will call `AshTestBase::SetUp()`.
    SetUpInternal(std::make_unique<CustomTestShellDelegate>(desk_model_.get()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The desks model for tests.
  std::unique_ptr<desks_storage::LocalDeskDataManager> desk_model_;

  // Temporary directory for the local desk model to store data.
  base::ScopedTempDir temp_dir_;
};

// Tests the helpers `AddEntry()` and `DeleteEntry()`, which will be used in
// different tests.
TEST_F(DesksTemplatesTest, AddDeleteEntry) {
  const base::GUID expected_uuid = base::GUID::GenerateRandomV4();
  const std::string expected_name = "desk name";
  base::Time expected_time = base::Time::Now();
  AddEntry(expected_uuid, expected_name, expected_time);

  base::RunLoop add_loop;
  desk_model()->GetAllEntries(base::BindLambdaForTesting(
      [&](desks_storage::DeskModel::GetAllEntriesStatus status,
          std::vector<ash::DeskTemplate*> entries) {
        EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk, status);
        ASSERT_EQ(1ul, entries.size());
        EXPECT_EQ(expected_uuid, entries[0]->uuid());
        EXPECT_EQ(base::UTF8ToUTF16(expected_name),
                  entries[0]->template_name());
        EXPECT_EQ(expected_time, entries[0]->created_time());
        add_loop.Quit();
      }));
  add_loop.Run();

  DeleteEntry(expected_uuid);
  base::RunLoop delete_loop;
  desk_model()->GetAllEntries(base::BindLambdaForTesting(
      [&](desks_storage::DeskModel::GetAllEntriesStatus status,
          std::vector<ash::DeskTemplate*> entries) {
        EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk, status);
        EXPECT_EQ(0ul, entries.size());
        delete_loop.Quit();
      }));
  delete_loop.Run();
}

// Tests the desks templates button visibility.
TEST_F(DesksTemplatesTest, DesksTemplatesButtonVisibility) {
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

}  // namespace ash
