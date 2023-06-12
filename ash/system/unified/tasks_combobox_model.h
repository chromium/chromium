// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_
#define ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_

#include <string>

#include "ash/glanceables/tasks/glanceables_tasks_types.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/list_model.h"

namespace ash {

// A simple data model for the glanceables tasks combobox. This is used to
// switch between different available tasks lists in the glanceable.
class TasksComboboxModel : public ui::ComboboxModel {
 public:
  explicit TasksComboboxModel(ui::ListModel<GlanceablesTaskList>* tasks_list);

  TasksComboboxModel(const TasksComboboxModel&) = delete;
  TasksComboboxModel& operator=(const TasksComboboxModel&) = delete;

  ~TasksComboboxModel() override;

 private:
  void SetTaskLists();

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  absl::optional<size_t> GetDefaultIndex() const override;

 private:
  // Owned by `GlanceableTasksClientImpl`.
  const raw_ptr<ui::ListModel<GlanceablesTaskList>, ExperimentalAsh> task_list_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TASKS_COMBOBOX_MODEL_H_
