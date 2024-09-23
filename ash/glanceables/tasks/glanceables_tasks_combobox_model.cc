// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_combobox_model.h"

#include <algorithm>
#include <optional>
#include <string>

#include "ash/api/tasks/tasks_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

const char kLastSelectedTaskListIdPref[] =
    "ash.glanceables.tasks.last_selected_task_list_id";
const char kLastSelectedTaskListTimePref[] =
    "ash.glanceables.tasks.last_selected_task_list_time";

}  // namespace

GlanceablesTasksComboboxModel::GlanceablesTasksComboboxModel(
    const ui::ListModel<api::TaskList>* task_lists)
    : task_lists_(task_lists) {}

GlanceablesTasksComboboxModel::~GlanceablesTasksComboboxModel() = default;

// static
void GlanceablesTasksComboboxModel::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kLastSelectedTaskListIdPref, "");
  registry->RegisterTimePref(kLastSelectedTaskListTimePref, base::Time());
}

// static
void GlanceablesTasksComboboxModel::ClearUserStatePrefs(
    PrefService* pref_service) {
  pref_service->ClearPref(kLastSelectedTaskListIdPref);
  pref_service->ClearPref(kLastSelectedTaskListTimePref);
}

void GlanceablesTasksComboboxModel::UpdateTaskLists(
    const ui::ListModel<api::TaskList>* task_lists) {
  task_lists_ = task_lists;
}

size_t GlanceablesTasksComboboxModel::GetItemCount() const {
  return task_lists_->item_count();
}

std::u16string GlanceablesTasksComboboxModel::GetItemAt(size_t index) const {
  return base::UTF8ToUTF16(task_lists_->GetItemAt(index)->title);
}

std::optional<size_t> GlanceablesTasksComboboxModel::GetDefaultIndex() const {
  const auto most_recently_updated_task_list_iter = std::max_element(
      task_lists_->begin(), task_lists_->end(),
      [](const auto& x1, const auto& x2) { return x1->updated < x2->updated; });
  const size_t most_recently_updated_task_list_index =
      most_recently_updated_task_list_iter - task_lists_->begin();

  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  const auto& last_selected_task_list_id =
      pref_service->GetString(kLastSelectedTaskListIdPref);
  const auto& last_selected_task_list_time =
      pref_service->GetTime(kLastSelectedTaskListTimePref);

  if (!last_selected_task_list_id.empty() &&
      last_selected_task_list_time >
          most_recently_updated_task_list_iter->get()->updated) {
    const auto last_selected_task_list_iter =
        std::find_if(task_lists_->begin(), task_lists_->end(),
                     [&last_selected_task_list_id](const auto& x) {
                       return x->id == last_selected_task_list_id;
                     });
    if (last_selected_task_list_iter != task_lists_->end()) {
      return last_selected_task_list_iter - task_lists_->begin();
    }
  }

  if (!last_selected_task_list_id.empty()) {
    pref_service->ClearPref(kLastSelectedTaskListIdPref);
    pref_service->ClearPref(kLastSelectedTaskListTimePref);
  }

  return most_recently_updated_task_list_index;
}

const api::TaskList* GlanceablesTasksComboboxModel::GetTaskListAt(
    size_t index) const {
  return task_lists_->GetItemAt(index);
}

void GlanceablesTasksComboboxModel::SaveLastSelectedTaskList(
    const std::string& task_list_id) {
  CHECK(!task_list_id.empty());

  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString(kLastSelectedTaskListIdPref, task_list_id);
  pref_service->SetTime(kLastSelectedTaskListTimePref, base::Time::Now());
}

}  // namespace ash
