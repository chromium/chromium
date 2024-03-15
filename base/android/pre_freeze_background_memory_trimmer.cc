// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pre_freeze_background_memory_trimmer.h"

#include <optional>
#include <string>

#include "base/android/build_info.h"
#include "base/android/pmf_utils.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"

namespace base::android {
namespace {

// This constant is chosen arbitrarily, to allow time for the background tasks
// to finish running BEFORE collecting metrics.
const base::TimeDelta kDelayForMetrics = base::Seconds(2);

std::optional<uint64_t> GetPrivateMemoryFootprint() {
  return PmfUtils::GetPrivateMemoryFootprintForCurrentProcess();
}

uint64_t BytesToMiB(uint64_t v) {
  return v / 1024 / 1024;
}

const char* GetProcessType() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  const std::string type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");
  const char* process_type = type == ""              ? "Browser"
                             : type == "renderer"    ? "Renderer"
                             : type == "gpu-process" ? "Gpu"
                             : type == "utility"     ? "Utility"
                                                     : "Unknown";
  return process_type;
}

std::string GetMetricName(const char* suffix) {
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  const char* process_type = GetProcessType();
  return StrCat(
      {"Memory.PreFreeze2.", process_type, ".PrivateMemoryFootprint.", suffix});
}

void MaybeRecordMetric(const std::string metric_name,
                       std::optional<uint64_t> value_bytes) {
  // Skip recording the metric if we failed to get the PMF.
  if (!value_bytes.has_value()) {
    return;
  }
  UmaHistogramMemoryMB(metric_name,
                       static_cast<int>(BytesToMiB(value_bytes.value())));
}

std::optional<uint64_t> PmfDiff(std::optional<uint64_t> pmf_before,
                                std::optional<uint64_t> pmf_after) {
  if (!pmf_before.has_value() || !pmf_before.has_value()) {
    return std::nullopt;
  }

  const uint64_t pmf_before_value = pmf_before.value();
  const uint64_t pmf_after_value = pmf_after.value();

  return pmf_after_value < pmf_before_value ? pmf_before_value - pmf_after_value
                                            : 0;
}

void RecordMetrics(std::optional<uint64_t> pmf_before) {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  std::string before_name = GetMetricName("Before");
  std::string after_name = GetMetricName("After");
  std::string diff_name = GetMetricName("Diff");

  std::optional<uint64_t> pmf_after = GetPrivateMemoryFootprint();

  MaybeRecordMetric(before_name, pmf_before);
  MaybeRecordMetric(after_name, pmf_after);
  MaybeRecordMetric(diff_name, PmfDiff(pmf_before, pmf_after));
}

}  // namespace

BASE_FEATURE(kOnPreFreezeMemoryTrim,
             "OnPreFreezeMemoryTrim",
             FEATURE_DISABLED_BY_DEFAULT);

PreFreezeBackgroundMemoryTrimmer::PreFreezeBackgroundMemoryTrimmer()
    : is_respecting_modern_trim_(BuildInfo::GetInstance()->sdk_int() >=
                                 SDK_VERSION_U) {}

// static
PreFreezeBackgroundMemoryTrimmer& PreFreezeBackgroundMemoryTrimmer::Instance() {
  static base::NoDestructor<PreFreezeBackgroundMemoryTrimmer> instance;
  return *instance;
}

void PreFreezeBackgroundMemoryTrimmer::PostMetricsTask(
    std::optional<uint64_t> pmf_before) {
  // PreFreeze is only for Android U and greater, so no need to record metrics
  // for older versions.
  if (!IsRespectingModernTrim()) {
    return;
  }

  // We need the process type to record the metrics below, which we get from
  // the command line. We cannot post the task below if the thread pool is not
  // initialized yet.
  if (!base::CommandLine::InitializedForCurrentProcess() ||
      !base::ThreadPoolInstance::Get()) {
    return;
  }

  // The posted task will be more likely to survive background killing in
  // experiments that change the memory trimming behavior. Run as USER_BLOCKING
  // to reduce this sample imbalance in experiment groups. Normally tasks
  // collecting metrics should use BEST_EFFORT, but when running in background a
  // number of subtle effects may influence the real delay of those tasks. The
  // USER_BLOCKING will allow to estimate the number of better-survived tasks
  // more precisely.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, MayBlock()},
      base::BindOnce(&RecordMetrics, pmf_before), kDelayForMetrics);
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
  // Preserve previous behaviour on versions before Android U.
  if (!IsRespectingModernTrim()) {
    task_runner->PostDelayedTask(from_here, std::move(task), delay);
    return;
  }

  {
    base::AutoLock locker(lock_);
    did_register_task_ = true;
  }
  if (!base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim)) {
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
  PostDelayedBackgroundTaskModernHelper(std::move(task_runner), from_here,
                                        std::move(task), delay);
}

PreFreezeBackgroundMemoryTrimmer::BackgroundTask*
PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskModernHelper(
    scoped_refptr<SequencedTaskRunner> task_runner,
    const Location& from_here,
    OnceClosure task,
    TimeDelta delay) {
  std::unique_ptr<BackgroundTask> background_task =
      BackgroundTask::Create(task_runner, from_here, std::move(task), delay);
  auto* ptr = background_task.get();
  background_tasks_.push_back(std::move(background_task));
  return ptr;
}

// static
void PreFreezeBackgroundMemoryTrimmer::OnPreFreeze() {
  Instance().OnPreFreezeInternal();
}

void PreFreezeBackgroundMemoryTrimmer::OnPreFreezeInternal() {
  base::AutoLock locker(lock_);
  if (did_register_task_) {
    PostMetricsTask(GetPrivateMemoryFootprint());
  }

  if (!IsRespectingModernTrim() ||
      !base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim)) {
    return;
  }

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
    BackgroundTask* task) {
  return Instance().UnregisterBackgroundTaskInternal(task);
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
bool PreFreezeBackgroundMemoryTrimmer::ShouldUseModernTrim() {
  return IsRespectingModernTrim() &&
         base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim);
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

void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::CancelTask() {
  if (task_handle_.IsValid()) {
    task_handle_.CancelTask();
    PreFreezeBackgroundMemoryTrimmer::UnregisterBackgroundTask(this);
  }
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

class OneShotDelayedBackgroundTimer::TimerImpl final
    : public OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimerImpl {
 public:
  ~TimerImpl() override = default;
  void Start(const Location& from_here,
             TimeDelta delay,
             OnceClosure task) override {
    timer_.Start(from_here, delay, std::move(task));
  }
  void Stop() override { timer_.Stop(); }
  bool IsRunning() const override { return timer_.IsRunning(); }
  void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner) override {
    timer_.SetTaskRunner(std::move(task_runner));
  }

 private:
  OneShotTimer timer_;
};

class OneShotDelayedBackgroundTimer::TaskImpl final
    : public OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimerImpl {
 public:
  ~TaskImpl() override = default;
  void Start(const Location& from_here,
             TimeDelta delay,
             OnceClosure task) override {
    DCHECK(!IsRunning());
    DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
    base::AutoLock locker(PreFreezeBackgroundMemoryTrimmer::Instance().lock_);
    task_ = PreFreezeBackgroundMemoryTrimmer::Instance()
                .PostDelayedBackgroundTaskModernHelper(
                    GetTaskRunner(), from_here,
                    BindOnce(
                        [](TaskImpl* timer, OnceClosure task) {
                          std::move(task).Run();
                          timer->task_ = nullptr;
                        },
                        // Unretained is fine here, since (1) this is always
                        // called on the same thread we destroy |this| on, and
                        // (2) destroying this will cancel the task.
                        base::Unretained(this), std::move(task)),
                    delay);
  }
  void Stop() override {
    if (IsRunning()) {
      task_->CancelTask();
      task_ = nullptr;
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

  raw_ptr<PreFreezeBackgroundMemoryTrimmer::BackgroundTask> task_ = nullptr;
  scoped_refptr<SequencedTaskRunner> task_runner_ = nullptr;
};

OneShotDelayedBackgroundTimer::OneShotDelayedBackgroundTimer() {
  if (PreFreezeBackgroundMemoryTrimmer::ShouldUseModernTrim()) {
    impl_ = std::make_unique<TaskImpl>();
  } else {
    impl_ = std::make_unique<TimerImpl>();
  }
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

void OneShotDelayedBackgroundTimer::Start(const Location& from_here,
                                          TimeDelta delay,
                                          OnceClosure task) {
  impl_->Start(from_here, delay, std::move(task));
}

}  // namespace base::android
