// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context_manager_impl.h"

#include <memory>
#include <ostream>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/borealis/infra/described.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"

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
  while (!task_queue_.empty()) {
    task_queue_.pop();
  }
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
    : profile_(profile), weak_factory_(this) {
  // ConciergeClient may not be initialized in tests.
  if (ash::ConciergeClient::Get()) {
    ShutDownBorealisIfRunning();
    ash::ConciergeClient::Get()->AddVmObserver(this);
  }
}

BorealisContextManagerImpl::~BorealisContextManagerImpl() {
  // Even if initialized, DBusThreadManager or ConciergeClient may be destroyed
  // prior to BorealisService/BorealisContextManagerImpl in tests. Therefore we
  // must not keep a pointer to the observed ConciergeClient, either directly or
  // via ScopedObservation or similar.
  if (ash::ConciergeClient::Get()) {
    ash::ConciergeClient::Get()->RemoveVmObserver(this);
  }
}

// Note that this method gets called in the constructor.
// If Borealis was running when chrome crashed/restarted then Borealis
// may still be running and should be shut down.
void BorealisContextManagerImpl::ShutDownBorealisIfRunning() {
  vm_tools::concierge::GetVmInfoRequest request;
  request.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_name(kBorealisVmName);
  ash::ConciergeClient::Get()->GetVmInfo(
      std::move(request),
      base::BindOnce(
          [](base::WeakPtr<BorealisContextManagerImpl> weak_this,
             std::optional<vm_tools::concierge::GetVmInfoResponse> reply) {
            if (reply.has_value() && reply->success() && weak_this) {
              weak_this->SendShutdownRequest(base::DoNothing(),
                                             kBorealisVmName);
            }
          },
          weak_factory_.GetWeakPtr()));
}

void BorealisContextManagerImpl::SendShutdownRequest(
    base::OnceCallback<void(BorealisShutdownResult)> on_shutdown_callback,
    const std::string& vm_name) {
  // TODO(b/172178036): This could have been a task-sequence but that
  // abstraction is proving insufficient.
  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_name(vm_name);
  ash::ConciergeClient::Get()->StopVm(
      std::move(request),
      base::BindOnce(
          [](base::OnceCallback<void(BorealisShutdownResult)>
                 on_shutdown_callback,
             std::optional<vm_tools::concierge::StopVmResponse> response) {
            // We don't have a good way to deal with a vm failing to stop (and
            // this would be a very rare occurrence anyway). We log an error if
            // it actually wasn't successful.
            BorealisShutdownResult result = BorealisShutdownResult::kSuccess;
            if (!response.has_value()) {
              LOG(ERROR) << "Failed to stop Borealis VM: No response";
              result = BorealisShutdownResult::kFailed;
            } else if (!response.value().success()) {
              LOG(ERROR) << "Failed to stop Borealis VM: "
                         << response.value().failure_reason();
              result = BorealisShutdownResult::kFailed;
            }
            RecordBorealisShutdownResultHistogram(result);
            std::move(on_shutdown_callback).Run(result);
          },
          std::move(on_shutdown_callback)));
}

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

void BorealisContextManagerImpl::ShutDownBorealis(
    base::OnceCallback<void(BorealisShutdownResult)> on_shutdown_callback) {
  CloseBorealisSplashScreenView();
  // Get the context we are shutting down, either from an in-progress startup or
  // from the running one.
  std::unique_ptr<BorealisContext> shutdown_context;
  std::swap(shutdown_context, context_);
  if (in_progress_startup_) {
    shutdown_context = in_progress_startup_->Abort();
  }
  if (!shutdown_context) {
    // TODO(b/172178036): There could be an operation in progress but we can't
    // tell because we don't record that state. Fix this by adding proper state
    // tracking for the context_manager.
    std::move(on_shutdown_callback).Run(BorealisShutdownResult::kSuccess);
    return;
  }
  RecordBorealisShutdownNumAttemptsHistogram();

  SendShutdownRequest(std::move(on_shutdown_callback),
                      shutdown_context->vm_name());
}

base::queue<std::unique_ptr<BorealisTask>>
BorealisContextManagerImpl::GetTasks() {
  base::queue<std::unique_ptr<BorealisTask>> task_queue;
  task_queue.push(std::make_unique<CheckAllowed>());
  task_queue.push(std::make_unique<GetLaunchOptions>());
  task_queue.push(std::make_unique<MountDlc>());
  task_queue.push(std::make_unique<CreateDiskImage>());
  task_queue.push(std::make_unique<StartBorealisVm>());
  task_queue.push(std::make_unique<AwaitBorealisStartup>());
  task_queue.push(std::make_unique<UpdateChromeFlags>(profile_));
  return task_queue;
}

void BorealisContextManagerImpl::AddCallback(ResultCallback callback) {
  callback_queue_.push(std::move(callback));
}

void BorealisContextManagerImpl::Complete(Startup::Result completion_result) {
  DCHECK(!context_);
  DCHECK(in_progress_startup_);
  in_progress_startup_.reset();

  BorealisContextManager::ContextOrFailure completion_result_for_clients;

  if (completion_result.has_value()) {
    context_ = std::move(completion_result).value();
    completion_result_for_clients = base::ok(context_.get());
  } else {
    LOG(ERROR) << "Startup failed: failure="
               << completion_result.error().error()
               << " message=" << completion_result.error().description();
    completion_result_for_clients = base::unexpected(completion_result.error());
  }

  while (!callback_queue_.empty()) {
    ResultCallback callback = std::move(callback_queue_.front());
    callback_queue_.pop();
    std::move(callback).Run(completion_result_for_clients);
  }
}

// TODO(b/179620544): Move handling of unexpected shutdowns to
// BorealisLaunchWatcher.
void BorealisContextManagerImpl::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {}

void BorealisContextManagerImpl::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  if (context_ && context_->vm_name() == signal.name() &&
      signal.owner_id() ==
          ash::ProfileHelper::GetUserIdHashFromProfile(profile_)) {
    CloseBorealisSplashScreenView();
    // If |context_| exists, it's a "running" Borealis instance which we didn't
    // request to shut down.
    context_->NotifyUnexpectedVmShutdown();

    // Update our state to reflect the unexpected VM exit.
    context_.reset();
  }
}

}  // namespace borealis
