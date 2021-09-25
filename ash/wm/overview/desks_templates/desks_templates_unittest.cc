// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_bar_view.h"
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

    OverviewTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The desks model for tests.
  std::unique_ptr<desks_storage::LocalDeskDataManager> desk_model_;

  // Temporary directory for the local desk model to store data.
  base::ScopedTempDir temp_dir_;
};

// Tests that the desks templates button is created when the feature is turned
// on.
// TODO(sammiequon): Update this test once more logic is added to the desks
// templates button.
TEST_F(DesksTemplatesTest, DesksTemplatesButtonVisibility) {
  ToggleOverview();
  auto* overview_session = GetOverviewSession();
  ASSERT_TRUE(overview_session);

  const auto* overview_grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  EXPECT_TRUE(desks_bar_view->desks_templates_button());
}

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

}  // namespace ash
