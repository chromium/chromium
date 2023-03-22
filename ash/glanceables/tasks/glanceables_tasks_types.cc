// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_tasks_types.h"

namespace ash {

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
                                 const std::vector<GlanceablesTask>& subtasks)
    : id(id), title(title), completed(completed), subtasks(subtasks) {}

GlanceablesTask::GlanceablesTask(const GlanceablesTask&) = default;

GlanceablesTask::~GlanceablesTask() = default;

}  // namespace ash
