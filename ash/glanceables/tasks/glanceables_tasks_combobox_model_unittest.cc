// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_combobox_model.h"

#include <memory>
#include <string>

#include "ash/api/tasks/tasks_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

std::unique_ptr<ui::ListModel<api::TaskList>> CreateTaskListsModel() {
  auto model = std::make_unique<ui::ListModel<api::TaskList>>();
  model->Add(
      std::make_unique<api::TaskList>("id1", "Task List 1", base::Time::Now()));
  model->Add(std::make_unique<api::TaskList>(
      "id2", "Task List 2 (the most recently updated)",
      base::Time::Now() + base::Days(15)));
  model->Add(std::make_unique<api::TaskList>(
      "id3", "Task List 3", base::Time::Now() + base::Days(1)));
  return model;
}

}  // namespace

class GlanceablesTasksComboboxModelTest : public AshTestBase {
 public:
  ui::ListModel<api::TaskList>* task_list_model() const {
    return task_lists_model_.get();
  }

 private:
  base::subtle::ScopedTimeClockOverrides time_override{
      []() {
        base::Time now;
        EXPECT_TRUE(base::Time::FromString("2023-08-31T00:00:00.000Z", &now));
        return now;
      },
      nullptr, nullptr};
  std::unique_ptr<ui::ListModel<api::TaskList>> task_lists_model_ =
      CreateTaskListsModel();
};

TEST_F(GlanceablesTasksComboboxModelTest, SavesLastSelectedTaskListToPrefs) {
  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  combobox_model.SaveLastSelectedTaskList("id3");

  const auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(pref_service->GetString(
                "ash.glanceables.tasks.last_selected_task_list_id"),
            "id3");
  EXPECT_EQ(pref_service->GetTime(
                "ash.glanceables.tasks.last_selected_task_list_time"),
            base::Time::Now());
}

TEST_F(GlanceablesTasksComboboxModelTest,
       SelectsMostRecentlyUpdatedTaskListByDefault) {
  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  EXPECT_EQ(combobox_model.GetDefaultIndex().value(), 1u);
}

TEST_F(GlanceablesTasksComboboxModelTest,
       SelectsMostRecentlyUpdatedTaskListIfItIsNewerThanLastSelected) {
  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString("ash.glanceables.tasks.last_selected_task_list_id",
                          "id1");
  pref_service->SetTime("ash.glanceables.tasks.last_selected_task_list_time",
                        base::Time::Now() - base::Days(100));

  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  EXPECT_EQ(combobox_model.GetDefaultIndex().value(), 1u);
}

TEST_F(GlanceablesTasksComboboxModelTest,
       SelectsLastSelectedTaskListByDefault) {
  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString("ash.glanceables.tasks.last_selected_task_list_id",
                          "id1");
  pref_service->SetTime("ash.glanceables.tasks.last_selected_task_list_time",
                        base::Time::Now() + base::Days(100));

  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  EXPECT_EQ(combobox_model.GetDefaultIndex().value(), 0u);
}

TEST_F(
    GlanceablesTasksComboboxModelTest,
    FallsBackToMostRecentlyUpdatedTaskListInCaseLastSelectedIdIsNoLongerValid) {
  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString("ash.glanceables.tasks.last_selected_task_list_id",
                          "unknown_id");
  pref_service->SetTime("ash.glanceables.tasks.last_selected_task_list_time",
                        base::Time::Now() + base::Days(100));

  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  EXPECT_EQ(combobox_model.GetDefaultIndex().value(), 1u);
}

TEST_F(
    GlanceablesTasksComboboxModelTest,
    FallsBackToMostRecentlyUpdatedTaskListInCaseLastSelectedIdHasMissingTime) {
  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString("ash.glanceables.tasks.last_selected_task_list_id",
                          "id1");

  auto combobox_model = GlanceablesTasksComboboxModel(task_list_model());
  EXPECT_EQ(combobox_model.GetDefaultIndex().value(), 1u);
}

}  // namespace ash
