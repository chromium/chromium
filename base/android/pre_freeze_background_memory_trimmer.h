// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
#define BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_

#include <deque>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/memory/post_delayed_memory_reduction_task.h"
#include "base/no_destructor.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"

namespace base::android {
class MemoryPurgeManagerAndroid;

BASE_EXPORT BASE_DECLARE_FEATURE(kOnPreFreezeMemoryTrim);

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
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_) {
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
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);

  static void SetSupportsModernTrimForTesting(bool is_supported);
  static void SetDidRegisterTasksForTesting(bool did_register_tasks);
  size_t GetNumberOfPendingBackgroundTasksForTesting() LOCKS_EXCLUDED(lock_);
  bool DidRegisterTasksForTesting();

  static void OnPreFreezeForTesting() LOCKS_EXCLUDED(lock_) { OnPreFreeze(); }

  // Called when Chrome is about to be frozen. Runs as many delayed tasks as
  // possible immediately, before we are frozen.
  static void OnPreFreeze() LOCKS_EXCLUDED(lock_);

  static bool SupportsModernTrim();
  static bool ShouldUseModernTrim();

 private:
  friend class base::NoDestructor<PreFreezeBackgroundMemoryTrimmer>;
  friend jboolean JNI_MemoryPurgeManager_IsOnPreFreezeMemoryTrimEnabled(
      JNIEnv* env);
  friend class base::android::MemoryPurgeManagerAndroid;
  friend class base::OneShotDelayedBackgroundTimer;

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
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::DelayedTaskHandle task_handle_;

    OnceCallback<void(MemoryReductionTaskContext)> task_;
  };

  PreFreezeBackgroundMemoryTrimmer();

  static void UnregisterBackgroundTask(BackgroundTask*) LOCKS_EXCLUDED(lock_);

  void UnregisterBackgroundTaskInternal(BackgroundTask*) LOCKS_EXCLUDED(lock_);

  static void SetDidRegisterTask() LOCKS_EXCLUDED(lock_);
  void SetDidRegisterTaskInternal() LOCKS_EXCLUDED(lock_);

  void PostDelayedBackgroundTaskInternal(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);
  void PostDelayedBackgroundTaskModern(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);
  BackgroundTask* PostDelayedBackgroundTaskModernHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      OnceCallback<void(MemoryReductionTaskContext)> task,
      base::TimeDelta delay) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void OnPreFreezeInternal() LOCKS_EXCLUDED(lock_);

  void PostMetricsTask(std::optional<uint64_t> pmf_before);

  mutable base::Lock lock_;
  std::deque<std::unique_ptr<BackgroundTask>> background_tasks_
      GUARDED_BY(lock_);
  // Keeps track of whether any tasks have been registered so far (set to true
  // once the first task is registered).
  bool did_register_task_ GUARDED_BY(lock_) = false;
  bool supports_modern_trim_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
