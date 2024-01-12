// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pre_freeze_background_memory_trimmer.h"
#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

BASE_FEATURE(kOnPreFreezeMemoryTrim,
             "OnPreFreezeMemoryTrim",
             FEATURE_DISABLED_BY_DEFAULT);

PreFreezeBackgroundMemoryTrimmer::PreFreezeBackgroundMemoryTrimmer()
    : is_respecting_modern_trim_(
          base::android::BuildInfo::GetInstance()->sdk_int() >=
          base::android::SDK_VERSION_U) {}

// static
PreFreezeBackgroundMemoryTrimmer& PreFreezeBackgroundMemoryTrimmer::Instance() {
  static base::NoDestructor<PreFreezeBackgroundMemoryTrimmer> instance;
  return *instance;
}

// static
void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  Instance().PostDelayedBackgroundTaskInternal(task_runner, from_here,
                                               std::move(task), delay);
}

void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskInternal(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  if (!IsRespectingModernTrim() ||
      !base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim)) {
    task_runner->PostDelayedTask(from_here, std::move(task), delay);
    return;
  }

  PostDelayedBackgroundTaskModern(task_runner, from_here, std::move(task),
                                  delay);
}

void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskModern(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  // We create a cancellable delayed task (below), which must be done on the
  // same TaskRunner that will run the task eventually, so we may need to
  // repost this on the correct TaskRunner.
  if (!task_runner->RunsTasksInCurrentSequence()) {
    // |base::Unretained(this)| is safe here because we never destroy |this|.
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskModern,
            base::Unretained(this), task_runner, from_here, std::move(task),
            delay));
    return;
  }

  base::AutoLock locker(lock_);
  std::unique_ptr<BackgroundTask> background_task =
      BackgroundTask::Create(task_runner, from_here, std::move(task), delay);
  background_tasks_.push_back(std::move(background_task));
}

// static
void PreFreezeBackgroundMemoryTrimmer::OnPreFreeze() {
  Instance().OnPreFreezeInternal();
}

void PreFreezeBackgroundMemoryTrimmer::OnPreFreezeInternal() {
  if (!IsRespectingModernTrim() ||
      !base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim)) {
    return;
  }

  base::AutoLock locker(lock_);
  while (!background_tasks_.empty()) {
    auto background_task = std::move(background_tasks_.front());
    background_tasks_.pop_front();
    // We release the lock here for two reasons:
    // (1) To avoid holding it too long while running all the background tasks.
    // (2) To prevent a deadlock if the |background_task| needs to acquire the
    //     lock (e.g. to post another task).
    base::AutoUnlock unlocker(lock_);
    BackgroundTask::RunNow(std::move(background_task));
  }
}

// static
void PreFreezeBackgroundMemoryTrimmer::UnregisterBackgroundTask(
    BackgroundTask* timer) {
  return Instance().UnregisterBackgroundTaskInternal(timer);
}

void PreFreezeBackgroundMemoryTrimmer::UnregisterBackgroundTaskInternal(
    BackgroundTask* timer) {
  base::AutoLock locker(lock_);
  std::erase_if(background_tasks_, [&](auto& t) { return t.get() == timer; });
}

// static
bool PreFreezeBackgroundMemoryTrimmer::IsRespectingModernTrim() {
  return Instance().is_respecting_modern_trim_;
}

// static
void PreFreezeBackgroundMemoryTrimmer::SetIsRespectingModernTrimForTesting(
    bool is_respecting) {
  Instance().is_respecting_modern_trim_ = is_respecting;
}

size_t PreFreezeBackgroundMemoryTrimmer::
    GetNumberOfPendingBackgroundTasksForTesting() {
  base::AutoLock locker(lock_);
  return background_tasks_.size();
}

// static
std::unique_ptr<PreFreezeBackgroundMemoryTrimmer::BackgroundTask>
PreFreezeBackgroundMemoryTrimmer::BackgroundTask::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  auto background_task = std::make_unique<BackgroundTask>(task_runner);
  background_task->Start(from_here, delay, std::move(task));
  return background_task;
}

PreFreezeBackgroundMemoryTrimmer::BackgroundTask::BackgroundTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}
PreFreezeBackgroundMemoryTrimmer::BackgroundTask::~BackgroundTask() = default;

// static
void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::RunNow(
    std::unique_ptr<PreFreezeBackgroundMemoryTrimmer::BackgroundTask>
        background_task) {
  if (!background_task->task_runner_->RunsTasksInCurrentSequence()) {
    background_task->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundTask::RunNow, std::move(background_task)));
    return;
  }

  // We check that the task has not been run already. If it has, we do not run
  // it again.
  if (background_task->task_handle_.IsValid()) {
    background_task->task_handle_.CancelTask();
  } else {
    return;
  }

  std::move(background_task->task_).Run();
}

void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::Start(
    const base::Location& from_here,
    base::TimeDelta delay,
    base::OnceClosure task) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  task_ = std::move(task);
  task_handle_ = task_runner_->PostCancelableDelayedTask(
      subtle::PostDelayedTaskPassKey(), from_here,
      base::BindOnce(
          [](BackgroundTask* p) {
            std::move(p->task_).Run();
            UnregisterBackgroundTask(p);
          },
          this),
      delay);
}

}  // namespace base
