// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pre_freeze_background_memory_trimmer.h"

#include <optional>
#include <string>

#include "base/android/build_info.h"
#include "base/android/pmf_utils.h"
#include "base/cancelable_callback.h"
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

uint64_t BytesToMiB(uint64_t v) {
  return v / 1024 / 1024;
}

const char* GetProcessType() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  const std::string type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII("type");
  const char* process_type = type == ""              ? "Browser"
                             : type == "renderer"    ? "Renderer"
                             : type == "gpu-process" ? "GPU"
                             : type == "utility"     ? "Utility"
                                                     : "Unknown";
  return process_type;
}

std::string GetMetricName(const std::string& name, const char* suffix) {
  CHECK(base::CommandLine::InitializedForCurrentProcess());
  const char* process_type = GetProcessType();
  return StrCat({"Memory.PreFreeze2.", process_type, ".", name, ".", suffix});
}

class PrivateMemoryFootprintMetric
    : public PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric {
 public:
  PrivateMemoryFootprintMetric()
      : PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric(
            "PrivateMemoryFootprint") {}
  std::optional<uint64_t> Measure() const override {
    return PmfUtils::GetPrivateMemoryFootprintForCurrentProcess();
  }
};

void MaybeRecordMetric(const std::string metric_name,
                       std::optional<uint64_t> value_bytes) {
  // Skip recording the metric if we failed to get the PMF.
  if (!value_bytes.has_value()) {
    return;
  }
  UmaHistogramMemoryMB(metric_name,
                       static_cast<int>(BytesToMiB(value_bytes.value())));
}

std::optional<uint64_t> Diff(std::optional<uint64_t> before,
                             std::optional<uint64_t> after) {
  if (!before.has_value() || !before.has_value()) {
    return std::nullopt;
  }

  const uint64_t before_value = before.value();
  const uint64_t after_value = after.value();

  return after_value < before_value ? before_value - after_value : 0;
}

void RecordMetrics(
    const PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric* metric,
    std::optional<uint64_t> value_before) {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  std::string before_name = GetMetricName(metric->name(), "Before");
  std::string after_name = GetMetricName(metric->name(), "After");
  std::string diff_name = GetMetricName(metric->name(), "Diff");

  std::optional<uint64_t> value_after = metric->Measure();

  MaybeRecordMetric(before_name, value_before);
  MaybeRecordMetric(after_name, value_after);
  MaybeRecordMetric(diff_name, Diff(value_before, value_after));
}

}  // namespace

BASE_FEATURE(kOnPreFreezeMemoryTrim,
             "OnPreFreezeMemoryTrim",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIsTrimMemoryBackgroundCritical,
             "IsTrimMemoryBackgroundCritical",
             FEATURE_DISABLED_BY_DEFAULT);

PreFreezeBackgroundMemoryTrimmer::PreFreezeBackgroundMemoryTrimmer()
    : supports_modern_trim_(BuildInfo::GetInstance()->sdk_int() >=
                            SDK_VERSION_U) {}

// static
PreFreezeBackgroundMemoryTrimmer& PreFreezeBackgroundMemoryTrimmer::Instance() {
  static base::NoDestructor<PreFreezeBackgroundMemoryTrimmer> instance;
  return *instance;
}

void PreFreezeBackgroundMemoryTrimmer::PostMetricsTask(
    const PreFreezeMetric* metric) {
  // PreFreeze is only for Android U and greater, so no need to record metrics
  // for older versions.
  if (!SupportsModernTrim()) {
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
  //
  // base::Unretained(metric) is safe here because we never unregister |metric|.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, MayBlock()},
      base::BindOnce(&RecordMetrics, base::Unretained(metric),
                     metric->Measure()),
      kDelayForMetrics);
}

// static
void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
    base::TimeDelta delay) {
  // Preserve previous behaviour on versions before Android U.
  if (!SupportsModernTrim()) {
    task_runner->PostDelayedTask(
        from_here,
        BindOnce(std::move(task), MemoryReductionTaskContext::kDelayExpired),
        delay);
    return;
  }

  Instance().PostDelayedBackgroundTaskInternal(task_runner, from_here,
                                               std::move(task), delay);
}

void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskInternal(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
    base::TimeDelta delay) {
  DCHECK(SupportsModernTrim());

  SetDidRegisterTaskInternal();

  if (!base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim)) {
    task_runner->PostDelayedTask(
        from_here,
        BindOnce(std::move(task), MemoryReductionTaskContext::kDelayExpired),
        delay);
    return;
  }

  PostDelayedBackgroundTaskModern(task_runner, from_here, std::move(task),
                                  delay);
}

void PreFreezeBackgroundMemoryTrimmer::PostDelayedBackgroundTaskModern(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
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
    OnceCallback<void(MemoryReductionTaskContext)> task,
    TimeDelta delay) {
  std::unique_ptr<BackgroundTask> background_task =
      BackgroundTask::Create(task_runner, from_here, std::move(task), delay);
  auto* ptr = background_task.get();
  background_tasks_.push_back(std::move(background_task));
  return ptr;
}

void PreFreezeBackgroundMemoryTrimmer::RegisterMemoryMetric(
    std::unique_ptr<const PreFreezeMetric> metric) {
  base::AutoLock locker(Instance().lock_);
  Instance().metrics_.push_back(std::move(metric));
}

void PreFreezeBackgroundMemoryTrimmer::PostMetricsTasksIfModern() {
  if (!SupportsModernTrim()) {
    return;
  }
  for (const auto& callback : metrics_) {
    PostMetricsTask(callback.get());
  }
}

// static
void PreFreezeBackgroundMemoryTrimmer::OnPreFreeze() {
  Instance().OnPreFreezeInternal();
}

void PreFreezeBackgroundMemoryTrimmer::OnPreFreezeInternal() {
  base::AutoLock locker(lock_);
  PostMetricsTasksIfModern();

  if (!ShouldUseModernTrim()) {
    return;
  }

  // We check |num_pending_tasks-- > 0| so that we have an upper limit on the
  // number of tasks that we run.
  // We check |!background_tasks_.empty()| so that we exit as soon as we have
  // no more tasks to run.
  //
  // This handles both the case where we have tasks that post other tasks (we
  // won't run endlessly because of the upper limit), and the case where tasks
  // cancel other tasks (we exit as soon as the queue is empty).
  //
  // Note that the current implementation may run some tasks that were posted
  // by earlier tasks, if some other tasks are also cancelled, but we
  // stop eventually due to the upper limit.
  size_t num_pending_tasks = background_tasks_.size();
  while (num_pending_tasks-- > 0 && !background_tasks_.empty()) {
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
void PreFreezeBackgroundMemoryTrimmer::SetDidRegisterTask() {
  Instance().SetDidRegisterTaskInternal();
}

void PreFreezeBackgroundMemoryTrimmer::SetDidRegisterTaskInternal() {
  base::AutoLock locker(lock_);
  if (!did_register_task_) {
    did_register_task_ = true;
    metrics_.push_back(std::make_unique<PrivateMemoryFootprintMetric>());
  }
}

// static
bool PreFreezeBackgroundMemoryTrimmer::SupportsModernTrim() {
  return Instance().supports_modern_trim_;
}

// static
bool PreFreezeBackgroundMemoryTrimmer::ShouldUseModernTrim() {
  return SupportsModernTrim() &&
         base::FeatureList::IsEnabled(kOnPreFreezeMemoryTrim);
}

// static
bool PreFreezeBackgroundMemoryTrimmer::IsTrimMemoryBackgroundCritical() {
  return SupportsModernTrim() &&
         base::FeatureList::IsEnabled(kIsTrimMemoryBackgroundCritical);
}

// static
void PreFreezeBackgroundMemoryTrimmer::SetSupportsModernTrimForTesting(
    bool is_supported) {
  Instance().supports_modern_trim_ = is_supported;
}

// static
void PreFreezeBackgroundMemoryTrimmer::ClearMetricsForTesting() {
  base::AutoLock locker(Instance().lock_);
  Instance().metrics_.clear();
  Instance().did_register_task_ = false;
}

bool PreFreezeBackgroundMemoryTrimmer::DidRegisterTasksForTesting() const {
  base::AutoLock locker(lock_);
  return metrics_.size() != 0;
}

size_t
PreFreezeBackgroundMemoryTrimmer::GetNumberOfPendingBackgroundTasksForTesting()
    const {
  base::AutoLock locker(lock_);
  return background_tasks_.size();
}

size_t PreFreezeBackgroundMemoryTrimmer::GetNumberOfKnownMetricsForTesting()
    const {
  base::AutoLock locker(lock_);
  return metrics_.size();
}

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

  background_task->Run(MemoryReductionTaskContext::kProactive);
}

void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::CancelTask() {
  if (task_handle_.IsValid()) {
    task_handle_.CancelTask();
    PreFreezeBackgroundMemoryTrimmer::UnregisterBackgroundTask(this);
  }
}

// static
std::unique_ptr<PreFreezeBackgroundMemoryTrimmer::BackgroundTask>
PreFreezeBackgroundMemoryTrimmer::BackgroundTask::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::Location& from_here,
    OnceCallback<void(MemoryReductionTaskContext)> task,
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

void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::Run(
    MemoryReductionTaskContext from_pre_freeze) {
  DCHECK(!task_handle_.IsValid());
  std::move(task_).Run(from_pre_freeze);
}

void PreFreezeBackgroundMemoryTrimmer::BackgroundTask::Start(
    const base::Location& from_here,
    base::TimeDelta delay,
    OnceCallback<void(MemoryReductionTaskContext)> task) {
  task_ = std::move(task);
  task_handle_ = task_runner_->PostCancelableDelayedTask(
      subtle::PostDelayedTaskPassKey(), from_here,
      base::BindOnce(
          [](BackgroundTask* p) {
            p->Run(MemoryReductionTaskContext::kDelayExpired);
            UnregisterBackgroundTask(p);
          },
          this),
      delay);
}

PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric::PreFreezeMetric(
    const std::string& name)
    : name_(name) {}

PreFreezeBackgroundMemoryTrimmer::PreFreezeMetric::~PreFreezeMetric() = default;

}  // namespace base::android
