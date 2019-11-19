// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/installable/installable_task_queue.h"

InstallableTask::InstallableTask() = default;

InstallableTask::InstallableTask(const InstallableParams& params,
                                 InstallableCallback callback)
    : params(params), callback(std::move(callback)) {}

InstallableTask::~InstallableTask() = default;

InstallableTask::InstallableTask(InstallableTask&& other) = default;

InstallableTask& InstallableTask::operator=(InstallableTask&& other) = default;

InstallableTaskQueue::InstallableTaskQueue() = default;

InstallableTaskQueue::~InstallableTaskQueue() = default;

void InstallableTaskQueue::Add(InstallableTask task) {
  tasks_.push_back(std::move(task));
}

void InstallableTaskQueue::PauseCurrent() {
  DCHECK(HasCurrent());
  paused_tasks_.push_back(std::move(Current()));
  Next();
}

void InstallableTaskQueue::UnpauseAll() {
  while (!paused_tasks_.empty()) {
    Add(std::move(paused_tasks_.front()));
    paused_tasks_.pop_front();
  }
}

bool InstallableTaskQueue::HasCurrent() const {
  return !tasks_.empty();
}

bool InstallableTaskQueue::HasPaused() const {
  return !paused_tasks_.empty();
}

InstallableTask& InstallableTaskQueue::Current() {
  DCHECK(HasCurrent());
  return tasks_.front();
}

void InstallableTaskQueue::Next() {
  DCHECK(HasCurrent());
  tasks_.pop_front();
}

void InstallableTaskQueue::Reset() {
  tasks_.clear();
  paused_tasks_.clear();
}
