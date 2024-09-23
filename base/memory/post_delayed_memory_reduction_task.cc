// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/post_delayed_memory_reduction_task.h"

#include "base/timer/timer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/pre_freeze_background_memory_trimmer.h"
#endif

namespace base {

void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
#if BUILDFLAG(IS_ANDROID)
  android::PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      std::move(task_runner), from_here, std::move(task), delay);
#else
  task_runner->PostDelayedTask(from_here, std::move(task), delay);
#endif
}

void PostDelayedMemoryReductionTask(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
    base::TimeDelta delay) {
#if BUILDFLAG(IS_ANDROID)
  android::PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
      std::move(task_runner), from_here, std::move(task), delay);
#else
  task_runner->PostDelayedTask(
      from_here,
      BindOnce(std::move(task), MemoryReductionTaskContext::kDelayExpired),
      delay);
#endif  // BUILDFLAG(IS_ANDROID)
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                OneShotDelayedBackgroundTimer::TimerImpl                   *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// This implementation is just a small wrapper around a |base::OneShotTimer|.
class OneShotDelayedBackgroundTimer::TimerImpl final
    : public OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimerImpl {
 public:
  ~TimerImpl() override = default;
  void Start(const Location& from_here,
             TimeDelta delay,
             OnceCallback<void(MemoryReductionTaskContext)> task) override {
    timer_.Start(
        from_here, delay,
        BindOnce(std::move(task), MemoryReductionTaskContext::kDelayExpired));
  }
  void Stop() override { timer_.Stop(); }
  bool IsRunning() const override { return timer_.IsRunning(); }
  void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) override {
    timer_.SetTaskRunner(std::move(task_runner));
  }

 private:
  OneShotTimer timer_;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                                TaskImpl                                   *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if BUILDFLAG(IS_ANDROID)
class OneShotDelayedBackgroundTimer::TaskImpl final
    : public OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimerImpl {
 public:
  ~TaskImpl() override = default;
  void Start(const Location& from_here,
             TimeDelta delay,
             OnceCallback<void(MemoryReductionTaskContext)> task) override {
    this->StartInternal(
        from_here, delay,
        BindOnce(
            [](TaskImpl* timer,
               OnceCallback<void(MemoryReductionTaskContext)> task,
               MemoryReductionTaskContext in_pre_freeze) {
              std::move(task).Run(in_pre_freeze);
              timer->task_ = nullptr;
            },
            // |base::Unretained(this)| is safe here because destroying this
            // will cancel the task. We do not need to worry about race
            // conditions here because destruction should always happen on the
            // same thread that the task is started on.
            base::Unretained(this), std::move(task)));
  }
  void StartInternal(const Location& from_here,
                     TimeDelta delay,
                     OnceCallback<void(MemoryReductionTaskContext)> task) {
    if (IsRunning()) {
      Stop();
    }
    DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
    base::AutoLock locker(
        android::PreFreezeBackgroundMemoryTrimmer::Instance().lock_);
    task_ = android::PreFreezeBackgroundMemoryTrimmer::Instance()
                .PostDelayedBackgroundTaskModernHelper(
                    GetTaskRunner(), from_here, std::move(task), delay);
  }
  void Stop() override {
    if (IsRunning()) {
      task_.ExtractAsDangling()->CancelTask();
    }
  }
  bool IsRunning() const override { return task_ != nullptr; }
  void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) override {
    task_runner_ = task_runner;
  }

 private:
  scoped_refptr<SequencedTaskRunner> GetTaskRunner() {
    // This matches the semantics of |OneShotTimer::GetTaskRunner()|.
    return task_runner_ ? task_runner_
                        : SequencedTaskRunner::GetCurrentDefault();
  }

  raw_ptr<android::PreFreezeBackgroundMemoryTrimmer::BackgroundTask> task_ =
      nullptr;
  scoped_refptr<SequencedTaskRunner> task_runner_ = nullptr;
};
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                           *
 *                       OneShotDelayedBackgroundTimer                       *
 *                                                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimer() {
#if BUILDFLAG(IS_ANDROID)
  if (android::PreFreezeBackgroundMemoryTrimmer::ShouldUseModernTrim()) {
    impl_ = std::make_unique<TaskImpl>();
  } else {
    impl_ = std::make_unique<TimerImpl>();
  }
#else
  impl_ = std::make_unique<TimerImpl>();
#endif
}

OneShotDelayedBackgroundTimer::~OneShotDelayedBackgroundTimer() {
  Stop();
}

void OneShotDelayedBackgroundTimer::Stop() {
  impl_->Stop();
}

bool OneShotDelayedBackgroundTimer::IsRunning() const {
  return impl_->IsRunning();
}

void OneShotDelayedBackgroundTimer::SetTaskRunner(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  impl_->SetTaskRunner(std::move(task_runner));
}

void OneShotDelayedBackgroundTimer::Start(
    const Location& from_here,
    TimeDelta delay,
    OnceCallback<void(MemoryReductionTaskContext)> task) {
#if BUILDFLAG(IS_ANDROID)
  android::PreFreezeBackgroundMemoryTrimmer::
      RegisterPrivateMemoryFootprintMetric();
#endif
  impl_->Start(from_here, delay, std::move(task));
}

}  // namespace base
