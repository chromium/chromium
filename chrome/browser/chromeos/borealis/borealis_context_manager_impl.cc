// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <memory>
#include <ostream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/borealis/borealis_context.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "chrome/browser/chromeos/borealis/borealis_task.h"
#include "chrome/browser/chromeos/borealis/infra/described.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace {

// We use a hard-coded name. When multi-instance becomes a feature we'll
// need to determine the name instead.
constexpr char kBorealisVmName[] = "borealis";

}  // namespace

namespace borealis {

BorealisContextManagerImpl::Startup::Startup(
    Profile* profile,
    base::queue<std::unique_ptr<BorealisTask>> task_queue)
    : profile_(profile),
      context_(nullptr),
      task_queue_(std::move(task_queue)),
      weak_factory_(this) {}

BorealisContextManagerImpl::Startup::~Startup() = default;

std::unique_ptr<BorealisContext> BorealisContextManagerImpl::Startup::Abort() {
  while (!task_queue_.empty())
    task_queue_.pop();
  Fail({BorealisStartupResult::kCancelled, "Startup aborted by user"});
  return std::move(context_);
}

void BorealisContextManagerImpl::Startup::NextTask() {
  if (task_queue_.empty()) {
    RecordBorealisStartupResultHistogram(BorealisStartupResult::kSuccess);
    RecordBorealisStartupOverallTimeHistogram(base::TimeTicks::Now() -
                                              start_tick_);
    Succeed(std::move(context_));
    return;
  }
  task_queue_.front()->Run(
      context_.get(),
      base::BindOnce(&BorealisContextManagerImpl::Startup::TaskCallback,
                     weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::Startup::TaskCallback(
    BorealisStartupResult result,
    std::string error) {
  task_queue_.pop();
  if (result == BorealisStartupResult::kSuccess) {
    NextTask();
    return;
  }
  RecordBorealisStartupResultHistogram(result);
  Fail({result, std::move(error)});
}

void BorealisContextManagerImpl::Startup::Start(
    std::unique_ptr<NotRunning> current_state) {
  context_ = base::WrapUnique(new BorealisContext(profile_));
  context_->set_vm_name(kBorealisVmName);
  start_tick_ = base::TimeTicks::Now();
  RecordBorealisStartupNumAttemptsHistogram();
  NextTask();
}

BorealisContextManagerImpl::BorealisContextManagerImpl(Profile* profile)
    : profile_(profile), weak_factory_(this) {}

BorealisContextManagerImpl::~BorealisContextManagerImpl() = default;

void BorealisContextManagerImpl::StartBorealis(ResultCallback callback) {
  if (context_) {
    std::move(callback).Run(
        BorealisContextManager::ContextOrFailure(context_.get()));
    return;
  }
  AddCallback(std::move(callback));
  if (!in_progress_startup_) {
    in_progress_startup_ = std::make_unique<Startup>(profile_, GetTasks());
    in_progress_startup_->Begin(
        std::make_unique<NotRunning>(),
        base::BindOnce(&BorealisContextManagerImpl::Complete,
                       weak_factory_.GetWeakPtr()));
  }
}

bool BorealisContextManagerImpl::IsRunning() {
  return context_.get();
}

void BorealisContextManagerImpl::ShutDownBorealis() {
  // Get the context we are shutting down, either from an in-progress startup or
  // from the running one.
  std::unique_ptr<BorealisContext> shutdown_context;
  std::swap(shutdown_context, context_);
  if (in_progress_startup_)
    shutdown_context = in_progress_startup_->Abort();
  if (!shutdown_context)
    return;

  // TODO(b/172178036): This could have been a task-sequence but that
  // abstraction is proving insufficient.
  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_name(shutdown_context->vm_name());
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

void BorealisContextManagerImpl::Complete(Startup::Result completion_result) {
  DCHECK(!context_);
  DCHECK(in_progress_startup_);
  in_progress_startup_.reset();

  BorealisContextManager::ContextOrFailure completion_result_for_clients =
      completion_result.Handle(
          base::BindOnce(
              [](std::unique_ptr<BorealisContext>* out_context,
                 std::unique_ptr<BorealisContext>& success) {
                std::swap(*out_context, success);
                return BorealisContextManager::ContextOrFailure(
                    out_context->get());
              },
              &context_),
          base::BindOnce([](Described<BorealisStartupResult>& failure) {
            LOG(ERROR) << "Startup failed: failure=" << failure.error()
                       << " message=" << failure.description();
            return BorealisContextManager::ContextOrFailure::Unexpected(
                Described<BorealisStartupResult>{failure.error(),
                                                 failure.description()});
          }));

  while (!callback_queue_.empty()) {
    ResultCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(completion_result_for_clients);
  }
}

}  // namespace borealis
