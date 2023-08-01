// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_types.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// ----------------------------------------------------------------------------
// GlanceablesTaskList:

GlanceablesTaskList::GlanceablesTaskList(const std::string& id,
                                         const std::string& title,
                                         const base::Time& updated)
    : id(id), title(title), updated(updated) {}

GlanceablesTaskList::~GlanceablesTaskList() = default;

// ----------------------------------------------------------------------------
// GlanceablesTask:

GlanceablesTask::GlanceablesTask(const std::string& id,
                                 const std::string& title,
                                 bool completed,
                                 const absl::optional<base::Time>& due,
                                 bool has_subtasks,
                                 bool has_email_link,
                                 bool has_notes)
    : id(id),
      title(title),
      completed(completed),
      due(due),
      has_subtasks(has_subtasks),
      has_email_link(has_email_link),
      has_notes(has_notes) {}

GlanceablesTask::~GlanceablesTask() = default;

}  // namespace ash
