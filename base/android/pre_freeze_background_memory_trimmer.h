// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
#define BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_

#include <deque>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"

namespace base::android {
class MemoryPurgeManagerAndroid;

class OneShotDelayedBackgroundTimer;

BASE_EXPORT BASE_DECLARE_FEATURE(kOnPreFreezeMemoryTrim);

// Replace for |OneShotTimer|, that allows the tasks to be run by |OnPreFreeze|
// (see |PreFreezeBackgroundMemoryTrimmer| below).
class BASE_EXPORT OneShotDelayedBackgroundTimer final {
 public:
  OneShotDelayedBackgroundTimer();
  ~OneShotDelayedBackgroundTimer();

  void Stop();

  void Start(const Location& posted_from, TimeDelta delay, OnceClosure task);

  bool IsRunning() const;

  template <class Receiver>
  void Start(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, delay, BindOnce(method, Unretained(receiver)));
  }

  void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner);

  class OneShotDelayedBackgroundTimerImpl {
   public:
    virtual ~OneShotDelayedBackgroundTimerImpl() = default;
    virtual void Stop() = 0;
    virtual void Start(const Location& posted_from,
                       TimeDelta delay,
                       OnceClosure task) = 0;
    virtual bool IsRunning() const = 0;
    virtual void SetTaskRunner(
        scoped_refptr<SequencedTaskRunner> task_runner) = 0;
  };

 private:
  friend class PreFreezeBackgroundMemoryTrimmer;
  class TimerImpl;
  class TaskImpl;

  std::unique_ptr<OneShotDelayedBackgroundTimerImpl> impl_;
};

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
      base::OnceClosure task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);

  static void SetIsRespectingModernTrimForTesting(bool is_respecting);
  size_t GetNumberOfPendingBackgroundTasksForTesting() LOCKS_EXCLUDED(lock_);

  static void OnPreFreezeForTesting() LOCKS_EXCLUDED(lock_) { OnPreFreeze(); }

  // Called when Chrome is about to be frozen. Runs as many delayed tasks as
  // possible immediately, before we are frozen.
  static void OnPreFreeze() LOCKS_EXCLUDED(lock_);

  static bool IsRespectingModernTrim();
  static bool ShouldUseModernTrim();

 private:
  friend class base::NoDestructor<PreFreezeBackgroundMemoryTrimmer>;
  friend jboolean JNI_MemoryPurgeManager_IsOnPreFreezeMemoryTrimEnabled(
      JNIEnv* env);
  friend class base::android::MemoryPurgeManagerAndroid;
  friend class OneShotDelayedBackgroundTimer::TaskImpl;

  // We use our own implementation here, based on |PostCancelableDelayedTask|,
  // rather than relying on something like |base::OneShotTimer|, since
  // |base::OneShotTimer| doesn't support things like immediately running our
  // task from a different sequence, and some |base::OneShotTimer|
  // functionality (e.g. |FireNow|) only works with the default task runner.
  class BackgroundTask {
   public:
    static std::unique_ptr<BackgroundTask> Create(
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        const base::Location& from_here,
        base::OnceClosure task,
        base::TimeDelta delay);

    explicit BackgroundTask(
        scoped_refptr<base::SequencedTaskRunner> task_runner);
    ~BackgroundTask();

    static void RunNow(std::unique_ptr<BackgroundTask>);

    void CancelTask();

   private:
    void Start(const base::Location& from_here,
               base::TimeDelta delay,
               base::OnceClosure task);
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::DelayedTaskHandle task_handle_;
    base::OnceClosure task_;
  };

  PreFreezeBackgroundMemoryTrimmer();

  static void UnregisterBackgroundTask(BackgroundTask*) LOCKS_EXCLUDED(lock_);

  void UnregisterBackgroundTaskInternal(BackgroundTask*) LOCKS_EXCLUDED(lock_);

  void PostDelayedBackgroundTaskInternal(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);
  void PostDelayedBackgroundTaskModern(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay) LOCKS_EXCLUDED(lock_);
  BackgroundTask* PostDelayedBackgroundTaskModernHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void OnPreFreezeInternal() LOCKS_EXCLUDED(lock_);

  void PostMetricsTask(std::optional<uint64_t> pmf_before);

  mutable base::Lock lock_;
  std::deque<std::unique_ptr<BackgroundTask>> background_tasks_
      GUARDED_BY(lock_);
  // Keeps track of whether any tasks have been registered so far (set to true
  // once the first task is registered).
  bool did_register_task_ GUARDED_BY(lock_) = false;
  bool is_respecting_modern_trim_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
