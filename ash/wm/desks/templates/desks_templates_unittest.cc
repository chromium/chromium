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
#include "ui/views/controls/button/image_button.h"

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

// Tests the helper `AddEntry()`, which will be used in different tests.
TEST_F(DesksTemplatesTest, AddEntry) {
  const base::GUID expected_uuid = base::GUID::GenerateRandomV4();
  const std::string expected_name = "desk name";
  base::Time expected_time = base::Time::Now();
  AddEntry(expected_uuid, expected_name, expected_time);

  base::RunLoop loop;
  desk_model()->GetAllEntries(base::BindLambdaForTesting(
      [&](desks_storage::DeskModel::GetAllEntriesStatus status,
          std::vector<ash::DeskTemplate*> entries) {
        EXPECT_EQ(desks_storage::DeskModel::GetAllEntriesStatus::kOk, status);
        ASSERT_EQ(1ul, entries.size());
        EXPECT_EQ(expected_uuid, entries[0]->uuid());
        EXPECT_EQ(base::UTF8ToUTF16(expected_name),
                  entries[0]->template_name());
        EXPECT_EQ(expected_time, entries[0]->created_time());
        loop.Quit();
      }));
  loop.Run();
}

// Tests the desks templates button visibility.
TEST_F(DesksTemplatesTest, DesksTemplatesButtonVisibility) {
  // The templates button should appear on all root windows.
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  ToggleOverview();
  for (auto* root_window : Shell::GetAllRootWindows()) {
    const auto* overview_grid =
        GetOverviewSession()->GetGridWithRootWindow(root_window);
    const auto* desks_templates_button =
        overview_grid->desks_bar_view()->desks_templates_button();
    ASSERT_TRUE(desks_templates_button);
    EXPECT_TRUE(desks_templates_button->GetVisible());
  }
}

}  // namespace ash
