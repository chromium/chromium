// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <ostream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/borealis_task.h"

namespace {

// We use a hard-coded name. When multi-instance becomes a feature we'll
// need to determine the name instead.
constexpr char kBorealisVmName[] = "borealis";

std::ostream& operator<<(std::ostream& stream,
                         borealis::BorealisContextManager::Status status) {
  switch (status) {
    case borealis::BorealisContextManager::Status::kSuccess:
      return stream << "Success";
    case borealis::BorealisContextManager::Status::kMountFailed:
      return stream << "Mount Failed";
    case borealis::BorealisContextManager::Status::kDiskImageFailed:
      return stream << "Disk Image Failed";
    case borealis::BorealisContextManager::Status::kStartVmFailed:
      return stream << "Start VM Failed";
    case borealis::BorealisContextManager::Status::kAwaitBorealisStartupFailed:
      return stream << "Await Borealis Startup Failed";
  }
}

}  // namespace

namespace borealis {

BorealisContextManagerImpl::BorealisContextManagerImpl(Profile* profile)
    : profile_(profile) {}

BorealisContextManagerImpl::~BorealisContextManagerImpl() = default;

void BorealisContextManagerImpl::StartBorealis(ResultCallback callback) {
  if (context_ && task_queue_.empty()) {
    std::move(callback).Run(GetResult());
    return;
  }
  AddCallback(std::move(callback));
  if (!context_) {
    context_ = base::WrapUnique(new BorealisContext(profile_));
    context_->set_vm_name(kBorealisVmName);
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
  task_queue.push(
      std::make_unique<AwaitBorealisStartup>(profile_, kBorealisVmName));
  return task_queue;
}

void BorealisContextManagerImpl::AddCallback(ResultCallback callback) {
  callback_queue_.push(std::move(callback));
}

void BorealisContextManagerImpl::NextTask() {
  if (task_queue_.empty()) {
    startup_status_ = Status::kSuccess;
    OnQueueComplete();
    return;
  }
  task_queue_.front()->Run(
      context_.get(), base::BindOnce(&BorealisContextManagerImpl::TaskCallback,
                                     weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::TaskCallback(Status status,
                                              std::string error) {
  task_queue_.pop();
  startup_status_ = status;
  if (startup_status_ == Status::kSuccess) {
    NextTask();
    return;
  }
  startup_error_ = error;
  LOG(ERROR) << "Startup failed: failure=" << startup_status_
             << " message=" << startup_error_;
  OnQueueComplete();
}

void BorealisContextManagerImpl::OnQueueComplete() {
  while (!callback_queue_.empty()) {
    ResultCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(GetResult());
  }
}

BorealisContextManager::Result BorealisContextManagerImpl::GetResult() {
  if (startup_status_ == Status::kSuccess) {
    return BorealisContextManager::Result(context_.get());
  }
  return BorealisContextManager::Result(startup_status_, startup_error_);
}

}  // namespace borealis
