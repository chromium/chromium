// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_task.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_handle.h"

namespace glic {

SequentialTaskGroup::SequentialTaskGroup() = default;
SequentialTaskGroup::SequentialTaskGroup(
    std::vector<std::unique_ptr<GlicInvokeTask>> tasks)
    : tasks_(std::move(tasks)) {}
SequentialTaskGroup::~SequentialTaskGroup() = default;

void SequentialTaskGroup::Start(base::OnceClosure done_callback) {
  CHECK_EQ(current_task_index_, 0u);
  done_callback_ = std::move(done_callback);
  RunNextTask();
}

void SequentialTaskGroup::RunNextTask() {
  if (current_task_index_ >= tasks_.size()) {
    std::move(done_callback_).Run();
    return;
  }
  auto& task = tasks_[current_task_index_++];
  task->Start(base::BindOnce(&SequentialTaskGroup::RunNextTask,
                             weak_ptr_factory_.GetWeakPtr()));
}

ParallelTaskGroup::ParallelTaskGroup() = default;
ParallelTaskGroup::ParallelTaskGroup(
    std::vector<std::unique_ptr<GlicInvokeTask>> tasks)
    : tasks_(std::move(tasks)) {}
ParallelTaskGroup::~ParallelTaskGroup() = default;

void ParallelTaskGroup::Start(base::OnceClosure done_callback) {
  if (tasks_.empty()) {
    std::move(done_callback).Run();
    return;
  }
  base::RepeatingClosure barrier =
      base::BarrierClosure(tasks_.size(), std::move(done_callback));
  for (auto& task : tasks_) {
    task->Start(barrier);
  }
}

WaitForNavigationTask::WaitForNavigationTask(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

WaitForNavigationTask::~WaitForNavigationTask() = default;

void WaitForNavigationTask::Start(base::OnceClosure done_callback) {
  if (!web_contents() ||
      !web_contents()->HasUncommittedNavigationInPrimaryMainFrame()) {
    std::move(done_callback).Run();
    return;
  }
  done_callback_ = std::move(done_callback);
}

void WaitForNavigationTask::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  Observe(nullptr);
  if (done_callback_) {
    std::move(done_callback_).Run();
  }
}

ShowInstanceTask::ShowInstanceTask(GlicInstanceImpl* instance,
                                   ShowOptions options)
    : instance_(instance), options_(options) {}

ShowInstanceTask::~ShowInstanceTask() = default;

void ShowInstanceTask::Start(base::OnceClosure done_callback) {
  instance_->Show(options_);
  std::move(done_callback).Run();
}

WaitForClientConnectedTask::WaitForClientConnectedTask(Host* host)
    : host_(host) {
  observation_.Observe(host_);
}

WaitForClientConnectedTask::~WaitForClientConnectedTask() = default;

void WaitForClientConnectedTask::Start(base::OnceClosure done_callback) {
  if (host_->IsWebClientConnected()) {
    std::move(done_callback).Run();
    return;
  }
  done_callback_ = std::move(done_callback);
}

void WaitForClientConnectedTask::WebClientConnected() {
  observation_.Reset();
  if (done_callback_) {
    std::move(done_callback_).Run();
  }
}

StabilizationTask::StabilizationTask(content::WebContents* web_contents) {
  Observe(web_contents);
}

StabilizationTask::~StabilizationTask() = default;

void StabilizationTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);
  stabilization_timer_.Start(
      FROM_HERE, base::Milliseconds(300),
      base::BindOnce(&StabilizationTask::OnStabilized, base::Unretained(this)));
}

void StabilizationTask::PrimaryMainFrameWasResized(bool width_changed) {
  if (stabilization_timer_.IsRunning()) {
    stabilization_timer_.Reset();
  }
}

void StabilizationTask::OnStabilized() {
  Observe(nullptr);
  std::move(done_callback_).Run();
}

WaitForFreCompletionTask::WaitForFreCompletionTask(
    ::Profile* profile,
    mojom::FreOverride fre_override)
    : profile_(profile), fre_override_(fre_override) {}

WaitForFreCompletionTask::~WaitForFreCompletionTask() = default;

void WaitForFreCompletionTask::Start(base::OnceClosure done_callback) {
  done_callback_ = std::move(done_callback);
  if (!ShouldWaitForFreCompletion()) {
    std::move(done_callback_).Run();
    return;
  }

  subscription_ = GlicKeyedService::Get(profile_)
                      ->enabling()
                      .RegisterProfileReadyStateChanged(base::BindRepeating(
                          &WaitForFreCompletionTask::OnProfileReadyStateChanged,
                          base::Unretained(this)));
}

void WaitForFreCompletionTask::OnProfileReadyStateChanged() {
  if (GlicEnabling::HasConsentedForProfile(profile_)) {
    subscription_ = {};
    std::move(done_callback_).Run();
  }
}

bool WaitForFreCompletionTask::ShouldWaitForFreCompletion() const {
  if (GlicEnabling::HasConsentedForProfile(profile_)) {
    return false;
  }
  return fre_override_ == mojom::FreOverride::kTrustFirstClick ||
         fre_override_ == mojom::FreOverride::kUnspecified;
}

}  // namespace glic
