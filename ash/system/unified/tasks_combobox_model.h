// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_
#define ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/models/combobox_model.h"

class PrefRegistrySimple;

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
class ASH_EXPORT TasksComboboxModel : public ui::ComboboxModel {
 public:
  explicit TasksComboboxModel(ui::ListModel<api::TaskList>* tasks_lists);
  TasksComboboxModel(const TasksComboboxModel&) = delete;
  TasksComboboxModel& operator=(const TasksComboboxModel&) = delete;
  ~TasksComboboxModel() override;

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears tasks glanceables state saved in user prefs.
  static void ClearUserStatePrefs(PrefService* pref_service);

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  absl::optional<size_t> GetDefaultIndex() const override;

  api::TaskList* GetTaskListAt(size_t index) const;

  // Saves the last selected `task_list_id` in user profile prefs.
  void SaveLastSelectedTaskList(const std::string& task_list_id);

 private:
  // Owned by `GlanceableTasksClientImpl`.
  const raw_ptr<ui::ListModel<api::TaskList>, ExperimentalAsh> task_lists_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_
