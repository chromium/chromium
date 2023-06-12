// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/system/unified/tasks_combobox_model.h"

#include "base/strings/utf_string_conversions.h"

namespace ash {

TasksComboboxModel::TasksComboboxModel(
    ui::ListModel<GlanceablesTaskList>* task_list)
    : task_list_(task_list) {}

TasksComboboxModel::~TasksComboboxModel() = default;

GlanceablesTaskList* TasksComboboxModel::GetTaskListAt(size_t index) const {
  return task_list_->GetItemAt(index);
}

size_t TasksComboboxModel::GetItemCount() const {
  return task_list_->item_count();
}

std::u16string TasksComboboxModel::GetItemAt(size_t index) const {
  return base::UTF8ToUTF16(task_list_->GetItemAt(index)->title);
}

absl::optional<size_t> TasksComboboxModel::GetDefaultIndex() const {
  return 0;
}

}  // namespace ash
