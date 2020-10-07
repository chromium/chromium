// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include "base/logging.h"

namespace borealis {

BorealisContextManagerImpl::BorealisContextManagerImpl(Profile* profile)
    : profile_(profile), context_(profile_) {}

BorealisContextManagerImpl::~BorealisContextManagerImpl() = default;

void BorealisContextManagerImpl::StartBorealis(
    BorealisContextCallback callback) {
  if (is_borealis_running_) {
    std::move(callback).Run(context_);
    return;
  }
  AddCallback(std::move(callback));
  if (!is_borealis_starting_) {
    is_borealis_starting_ = true;
    // TODO(b/168425531): add actual startup tasks here.
    NextTask(/*should_continue=*/true);
  }
}

void BorealisContextManagerImpl::AddTaskForTesting(
    std::unique_ptr<BorealisTask> task) {
  AddTask(std::move(task));
}

void BorealisContextManagerImpl::AddTask(std::unique_ptr<BorealisTask> task) {
  task_queue_.push(std::move(task));
}

void BorealisContextManagerImpl::AddCallback(BorealisContextCallback callback) {
  callback_queue_.push(std::move(callback));
}

void BorealisContextManagerImpl::NextTask(bool should_continue) {
  if (!should_continue) {
    // TODO(b/168425531): Error handling should be expanded to give more
    // information about which task failed, why it failed and what should happen
    // as a result.
    LOG(ERROR) << "A task failed when trying to start Borealis.";
    OnQueueComplete();
    return;
  }
  if (task_queue_.empty()) {
    context_.set_borealis_running(true);
    is_borealis_running_ = true;
    OnQueueComplete();
    return;
  }
  std::unique_ptr<BorealisTask> next_task = std::move(task_queue_.front());
  task_queue_.pop();
  next_task->Run(&context_,
                 base::BindOnce(&BorealisContextManagerImpl::NextTask,
                                weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::OnQueueComplete() {
  is_borealis_starting_ = false;
  while (!callback_queue_.empty()) {
    BorealisContextCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(context_);
  }
}

}  // namespace borealis
