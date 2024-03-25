// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_COMBOBOX_MODEL_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_COMBOBOX_MODEL_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/models/combobox_model.h"

class PrefRegistrySimple;
class PrefService;

namespace ui {
template <class ItemType>
class ListModel;
}

namespace ash {

namespace api {
struct TaskList;
}  // namespace api

// A simple data model for the glanceables tasks combobox. This is used to
// switch between different available tasks lists in the glanceable.
class ASH_EXPORT GlanceablesTasksComboboxModel : public ui::ComboboxModel {
 public:
  explicit GlanceablesTasksComboboxModel(
      const ui::ListModel<api::TaskList>* tasks_lists);
  GlanceablesTasksComboboxModel(const GlanceablesTasksComboboxModel&) = delete;
  GlanceablesTasksComboboxModel& operator=(
      const GlanceablesTasksComboboxModel&) = delete;
  ~GlanceablesTasksComboboxModel() override;

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears tasks glanceables state saved in user prefs.
  static void ClearUserStatePrefs(PrefService* pref_service);

  // Updates `task_lists_` with `task_list`.
  void UpdateTaskLists(const ui::ListModel<api::TaskList>* task_lists);

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

  const api::TaskList* GetTaskListAt(size_t index) const;

  // Saves the last selected `task_list_id` in user profile prefs.
  void SaveLastSelectedTaskList(const std::string& task_list_id);

 private:
  // Owned by `TasksClientImpl`.
  raw_ptr<const ui::ListModel<api::TaskList>> task_lists_;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASKS_COMBOBOX_MODEL_H_
