// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <ostream>

#include "base/logging.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/borealis_task.h"

namespace {

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisContextManager::Status status) {
  switch (status) {
    case borealis::BorealisContextManager::kSuccess:
      return stream << "Success";
    case borealis::BorealisContextManager::kMountFailed:
      return stream << "Mount Failed";
    case borealis::BorealisContextManager::kDiskImageFailed:
      return stream << "Disk Image Failed";
    case borealis::BorealisContextManager::kStartVmFailed:
      return stream << "Start VM Failed";
  }
}

}  // namespace

namespace borealis {

BorealisContextManagerImpl::BorealisContextManagerImpl(Profile* profile)
    : profile_(profile), context_(profile_) {}

BorealisContextManagerImpl::~BorealisContextManagerImpl() = default;

void BorealisContextManagerImpl::StartBorealis(ResultCallback callback) {
  if (is_borealis_running_) {
    std::move(callback).Run(GetResult());
    return;
  }
  AddCallback(std::move(callback));
  if (!is_borealis_starting_) {
    is_borealis_starting_ = true;
    task_queue_ = GetTasks();
    NextTask();
  }
}

base::queue<std::unique_ptr<BorealisTask>>
BorealisContextManagerImpl::GetTasks() {
  base::queue<std::unique_ptr<BorealisTask>> task_queue;
  task_queue.push(std::make_unique<MountDlc>());
  task_queue.push(std::make_unique<CreateDiskImage>());
  task_queue.push(std::make_unique<StartBorealisVm>());
  task_queue.push(std::make_unique<AwaitBorealisStartup>());
  return task_queue;
}

void BorealisContextManagerImpl::AddCallback(ResultCallback callback) {
  callback_queue_.push(std::move(callback));
}

void BorealisContextManagerImpl::NextTask() {
  if (task_queue_.empty()) {
    context_.set_borealis_running(true);
    is_borealis_running_ = true;
    startup_status_ = kSuccess;
    OnQueueComplete();
    return;
  }
  current_task_ = std::move(task_queue_.front());
  task_queue_.pop();
  current_task_->Run(&context_,
                     base::BindOnce(&BorealisContextManagerImpl::TaskCallback,
                                    weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::TaskCallback(Status status,
                                              std::string error) {
  startup_status_ = status;
  if (startup_status_ == kSuccess) {
    NextTask();
    return;
  }
  startup_error_ = error;
  LOG(ERROR) << "Startup failed: failure=" << startup_status_
             << " message=" << startup_error_;
  OnQueueComplete();
}

void BorealisContextManagerImpl::OnQueueComplete() {
  is_borealis_starting_ = false;
  while (!callback_queue_.empty()) {
    ResultCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(GetResult());
  }
}

BorealisContextManager::Result BorealisContextManagerImpl::GetResult() {
  if (startup_status_ == kSuccess) {
    return BorealisContextManager::Result(&context_);
  }
  return BorealisContextManager::Result(startup_status_, startup_error_);
}

}  // namespace borealis
