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
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace {

// We use a hard-coded name. When multi-instance becomes a feature we'll
// need to determine the name instead.
constexpr char kBorealisVmName[] = "borealis";

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
    startup_start_tick_ = base::TimeTicks::Now();
    RecordBorealisStartupNumAttemptsHistogram();
    NextTask();
  }
}

void BorealisContextManagerImpl::ShutDownBorealis() {
  // TODO(b/172178036): This could have been a task-sequence but that
  // abstraction is proving insufficient.
  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_name(context_->vm_name());
  chromeos::DBusThreadManager::Get()->GetConciergeClient()->StopVm(
      std::move(request),
      base::BindOnce(
          [](base::Optional<vm_tools::concierge::StopVmResponse> response) {
            // We don't have a good way to deal with a vm failing to stop (and
            // this would be a very rare occurrence anyway). We log an error if
            // it actually wasn't successful.
            if (!response.has_value()) {
              LOG(ERROR) << "Failed to stop Borealis VM: No response";
            } else if (!response.value().success()) {
              LOG(ERROR) << "Failed to stop Borealis VM: "
                         << response.value().failure_reason();
            }
          }));
  Complete(BorealisStartupResult::kCancelled, "shut down");
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
    RecordBorealisStartupOverallTimeHistogram(base::TimeTicks::Now() -
                                              startup_start_tick_);
    Complete(BorealisStartupResult::kSuccess, "");
    return;
  }
  task_queue_.front()->Run(
      context_.get(), base::BindOnce(&BorealisContextManagerImpl::TaskCallback,
                                     weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::TaskCallback(BorealisStartupResult result,
                                              std::string error) {
  task_queue_.pop();
  if (result == BorealisStartupResult::kSuccess) {
    NextTask();
    return;
  }
  LOG(ERROR) << "Startup failed: failure=" << result << " message=" << error;
  Complete(result, std::move(error));
}

void BorealisContextManagerImpl::Complete(BorealisStartupResult result,
                                          std::string error_or_empty) {
  startup_result_ = result;
  startup_error_ = error_or_empty;
  RecordBorealisStartupResultHistogram(result);

  while (!callback_queue_.empty()) {
    ResultCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(GetResult());
  }

  if (startup_result_ == BorealisStartupResult::kSuccess)
    return;

  task_queue_ = {};
  // TODO(b/172178467): handle races better when doing this.
  context_.reset();
}

BorealisContextManager::Result BorealisContextManagerImpl::GetResult() {
  if (startup_result_ == BorealisStartupResult::kSuccess) {
    return BorealisContextManager::Result(context_.get());
  }
  return BorealisContextManager::Result(startup_result_, startup_error_);
}

}  // namespace borealis
