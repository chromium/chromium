// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/task_ui/actor_task_ui_model.h"

namespace actor {

ActorTaskUiModel::TaskView::TaskView() = default;
ActorTaskUiModel::TaskView::TaskView(const TaskView&) = default;
ActorTaskUiModel::TaskView& ActorTaskUiModel::TaskView::operator=(
    const TaskView&) = default;
ActorTaskUiModel::TaskView::TaskView(TaskView&&) noexcept = default;
ActorTaskUiModel::TaskView& ActorTaskUiModel::TaskView::operator=(
    TaskView&&) noexcept = default;
ActorTaskUiModel::TaskView::~TaskView() = default;

}  // namespace actor
