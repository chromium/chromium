// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/delayed_task_handle.h"

#include <utility>

#include "base/check.h"

namespace base {

DelayedTaskHandle::DelayedTaskHandle() = default;

DelayedTaskHandle::DelayedTaskHandle(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(IsValid());
}

DelayedTaskHandle::~DelayedTaskHandle() {
  // A task handle should never be destroyed in a valid state. It should either
  // have been executed, canceled or have had its task deleted.
  DCHECK(!IsValid());
}

DelayedTaskHandle::DelayedTaskHandle(DelayedTaskHandle&& other) = default;

DelayedTaskHandle& DelayedTaskHandle::operator=(DelayedTaskHandle&& other) {
  // A valid handle can't be overwritten by an assignment.
  DCHECK(!IsValid());
  delegate_ = std::move(other.delegate_);
  return *this;
}

bool DelayedTaskHandle::IsValid() const {
  return delegate_ && delegate_->IsValid();
}

void DelayedTaskHandle::CancelTask() {
  // The delegate is responsible for cancelling the task.
  if (delegate_) {
    delegate_->CancelTask();
    DCHECK(!delegate_->IsValid());
    delegate_.reset();
  }
}

}  // namespace base
