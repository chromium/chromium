// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/default_delayed_task_handle_delegate.h"

#include <utility>

#include "base/functional/bind.h"

namespace base {

DefaultDelayedTaskHandleDelegate::DefaultDelayedTaskHandleDelegate() = default;

DefaultDelayedTaskHandleDelegate::~DefaultDelayedTaskHandleDelegate() = default;

bool DefaultDelayedTaskHandleDelegate::IsValid() const {
  return weak_ptr_factory_.HasWeakPtrs();
}

void DefaultDelayedTaskHandleDelegate::CancelTask() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

OnceClosure DefaultDelayedTaskHandleDelegate::BindCallback(
    OnceClosure callback) {
  DCHECK(!IsValid());
  return BindOnce(&DefaultDelayedTaskHandleDelegate::RunTask,
                  weak_ptr_factory_.GetWeakPtr(), std::move(callback));
}

void DefaultDelayedTaskHandleDelegate::RunTask(OnceClosure user_task) {
  // Invalidate the weak pointer first so that the task handle is considered
  // invalid while running the task.
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(user_task).Run();
}

}  // namespace base
