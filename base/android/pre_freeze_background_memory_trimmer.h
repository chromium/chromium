// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
#define BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_

#include <deque>

#include "base/byte_count.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/no_destructor.h"
#include "base/profiler/sample_metadata.h"
#include "base/sequence_checker.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"

namespace base::android {
class MemoryPurgeManagerAndroid;

// TODO(thiabaud): Remove these once we fix the include in about/flags
BASE_EXPORT BASE_DECLARE_FEATURE(kShouldFreezeSelf);
BASE_EXPORT BASE_DECLARE_FEATURE(kUseRunningCompact);

// Starting from Android U, apps are frozen shortly after being backgrounded
// (with some exceptions). This causes some background tasks for reclaiming
// resources in Chrome to not be run until Chrome is foregrounded again (which
// completely defeats their purpose).
//
// To try to avoid this problem, we use the |PostDelayedBackgroundTask| found
// below. Prior to Android U, this will simply post the task in the background
// with the given delay. From Android U onwards, this will post the task in the
// background with the given delay, but will run it sooner if we are about to
// be frozen.
class BASE_EXPORT PreFreezeBackgroundMemoryTrimmer {
 public:
  static PreFreezeBackgroundMemoryTrimmer& Instance();
  ~PreFreezeBackgroundMemoryTrimmer() = delete;

  // Posts a delayed task. On versions of Android starting from U, may run the
  // task sooner if we are backgrounded. In this case, we run the task on the
  // correct task runner, ignoring the given delay.
  static void PostDelayedBackgroundTask(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(void)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock()) {
    PostDelayedBackgroundTask(
        task_runner, from_here,
        BindOnce(
            [](OnceClosure task,
               MemoryReductionTaskContext called_from_prefreeze) {
              std::move(task).Run();
            },
            std::move(task)),
        delay);
  }
  static void PostDelayedBackgroundTask(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock());

  class PreFreezeMetric {
   public:
    virtual ~PreFreezeMetric();

    // |Measure| should return an amount of memory in bytes, or nullopt if
    // unable to record the metric for any reason. It is called underneath a
    // lock, so it should be fast enough to avoid delays (the same lock is held
    // when unregistering metrics).
    virtual std::optional<ByteCount> Measure() const = 0;

    const std::string& name() const LIFETIME_BOUND { return name_; }

   protected:
    friend class PreFreezeBackgroundMemoryTrimmer;
    explicit PreFreezeMetric(const std::string& name);

   private:
    const std::string name_;
  };

  // Registers a new metric to record before and after PreFreeze. Callers are
  // responsible for making sure that the same metric is not registered
  // multiple times.
  //
  // See |PreFreezeMetric| for details on the metric itself.
  //
  // Each time |OnPreFreeze| is run, |metric->Measure()| will be called twice:
  // - Once directly before any tasks are run; and
  // - Once two seconds after the first time it was called.
  //
  // As an example, calling RegisterMemoryMetric(PrivateMemoryFootprintMetric)
  // in the Browser process would cause the following metrics to be recorded 2
  // seconds after the next time |OnPreFreeze| is run:
  // - "Memory.PreFreeze2.Browser.PrivateMemoryFootprint.Before"
  // - "Memory.PreFreeze2.Browser.PrivateMemoryFootprint.After"
  // - "Memory.PreFreeze2.Browser.PrivateMemoryFootprint.Diff"
  //
  // See "Memory.PreFreeze2.{process_type}.{name}.{suffix}" for details on the
  // exact metrics.
  static void RegisterMemoryMetric(const PreFreezeMetric* metric)
      LOCKS_EXCLUDED(lock());

  static void UnregisterMemoryMetric(const PreFreezeMetric* metric)
      LOCKS_EXCLUDED(lock());

  static void SetSupportsModernTrimForTesting(bool is_supported);
  static void ClearMetricsForTesting() LOCKS_EXCLUDED(lock());
  size_t GetNumberOfPendingBackgroundTasksForTesting() const
      LOCKS_EXCLUDED(lock());
  size_t GetNumberOfKnownMetricsForTesting() const LOCKS_EXCLUDED(lock());
  size_t GetNumberOfValuesBeforeForTesting() const LOCKS_EXCLUDED(lock());
  bool DidRegisterTasksForTesting() const;

  static void OnPreFreezeForTesting() LOCKS_EXCLUDED(lock()) { OnPreFreeze(); }

  // Called when Chrome is about to be frozen. Runs as many delayed tasks as
  // possible immediately, before we are frozen.
  static void OnPreFreeze() LOCKS_EXCLUDED(lock());

  static bool SupportsModernTrim();
  static bool ShouldUseModernTrim();
  static bool IsTrimMemoryBackgroundCritical();

 private:
  friend class base::NoDestructor<PreFreezeBackgroundMemoryTrimmer>;
  friend jboolean JNI_MemoryPurgeManager_IsOnPreFreezeMemoryTrimEnabled(
      JNIEnv* env);
  friend class base::android::MemoryPurgeManagerAndroid;
  friend class base::OneShotDelayedBackgroundTimer;
  friend class SelfCompactionManager;
  friend class PreFreezeBackgroundMemoryTrimmerTest;
  friend class PreFreezeSelfCompactionTest;
  friend class PreFreezeSelfCompactionTestWithParam;
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, Disabled);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, TimeoutCancel);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTestWithParam, Cancel);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTest, NotCanceled);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTest, SimpleCancel);
  FRIEND_TEST_ALL_PREFIXES(PreFreezeSelfCompactionTest, OnSelfFreezeCancel);

  // We use our own implementation here, based on |PostCancelableDelayedTask|,
  // rather than relying on something like |base::OneShotTimer|, since
  // |base::OneShotTimer| doesn't support things like immediately running our
  // task from a different sequence, and some |base::OneShotTimer|
  // functionality (e.g. |FireNow|) only works with the default task runner.
  class BackgroundTask final {
   public:
    static std::unique_ptr<BackgroundTask> Create(
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        const base::Location& from_here,
        OnceCallback<void(MemoryReductionTaskContext)> task,
        base::TimeDelta delay);

    explicit BackgroundTask(
        scoped_refptr<base::SequencedTaskRunner> task_runner);
    ~BackgroundTask();

    static void RunNow(std::unique_ptr<BackgroundTask> background_task);

    void Run(MemoryReductionTaskContext from_pre_freeze);

    void CancelTask();

   private:
    friend class PreFreezeBackgroundMemoryTrimmer;
    void Start(const Location& from_here,
               TimeDelta delay,
               OnceCallback<void(MemoryReductionTaskContext)> task);
    void StartInternal(const Location& from_here,
                       TimeDelta delay,
                       OnceClosure task);

    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::DelayedTaskHandle GUARDED_BY_CONTEXT(sequence_checker_) task_handle_;

    OnceCallback<void(MemoryReductionTaskContext)> task_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

 private:
  PreFreezeBackgroundMemoryTrimmer();

  static base::Lock& lock() { return Instance().lock_; }


  void RegisterMemoryMetricInternal(const PreFreezeMetric* metric)
      EXCLUSIVE_LOCKS_REQUIRED(lock());

  void UnregisterMemoryMetricInternal(const PreFreezeMetric* metric)
      EXCLUSIVE_LOCKS_REQUIRED(lock());
  static void UnregisterBackgroundTask(BackgroundTask*) LOCKS_EXCLUDED(lock());

  void UnregisterBackgroundTaskInternal(BackgroundTask*) LOCKS_EXCLUDED(lock());

  static void RegisterPrivateMemoryFootprintMetric() LOCKS_EXCLUDED(lock());
  void RegisterPrivateMemoryFootprintMetricInternal() LOCKS_EXCLUDED(lock());

  void PostDelayedBackgroundTaskInternal(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock());
  void PostDelayedBackgroundTaskModern(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock());
  BackgroundTask* PostDelayedBackgroundTaskModernHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) EXCLUSIVE_LOCKS_REQUIRED(lock());

  void OnPreFreezeInternal() LOCKS_EXCLUDED(lock());
  void RunPreFreezeTasks() EXCLUSIVE_LOCKS_REQUIRED(lock());

  void PostMetricsTasksIfModern() EXCLUSIVE_LOCKS_REQUIRED(lock());
  void PostMetricsTask() EXCLUSIVE_LOCKS_REQUIRED(lock());
  void RecordMetrics() LOCKS_EXCLUDED(lock());

  mutable base::Lock lock_;
  std::deque<std::unique_ptr<BackgroundTask>> background_tasks_
      GUARDED_BY(lock());
  std::vector<const PreFreezeMetric*> metrics_ GUARDED_BY(lock());
  // When a metrics task is posted (see |RecordMetrics|), the values of each
  // metric before any tasks are run are saved here. The "i"th entry corresponds
  // to the "i"th entry in |metrics_|. When there is no pending metrics task,
  // |values_before_| should be empty.
  std::vector<std::optional<ByteCount>> values_before_ GUARDED_BY(lock());
  bool supports_modern_trim_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
