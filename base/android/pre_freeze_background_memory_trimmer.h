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

namespace base {

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
      base::OnceClosure task,
      base::TimeDelta delay);

  static void SetIsRespectingModernTrimForTesting(bool is_respecting);
  size_t GetNumberOfPendingBackgroundTasksForTesting();

  static void OnPreFreezeForTesting() { OnPreFreeze(); }

  // Called when Chrome is about to be frozen. Runs as many delayed tasks as
  // possible immediately, before we are frozen.
  static void OnPreFreeze();

 private:
  friend class base::NoDestructor<PreFreezeBackgroundMemoryTrimmer>;

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

   private:
    void Start(const base::Location& from_here,
               base::TimeDelta delay,
               base::OnceClosure task);
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::DelayedTaskHandle task_handle_;
    base::OnceClosure task_;
  };

  PreFreezeBackgroundMemoryTrimmer();

  static void UnregisterBackgroundTask(BackgroundTask*);

  void UnregisterBackgroundTaskInternal(BackgroundTask*) LOCKS_EXCLUDED(lock_);

  static bool IsRespectingModernTrim();

  void PostDelayedBackgroundTaskInternal(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay);
  void PostDelayedBackgroundTaskModern(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& from_here,
      base::OnceClosure task,
      base::TimeDelta delay);

  void OnPreFreezeInternal();

  mutable base::Lock lock_;
  std::deque<std::unique_ptr<BackgroundTask>> background_tasks_
      GUARDED_BY(lock_);
  bool is_respecting_modern_trim_;
};
}  // namespace base

#endif  // BASE_ANDROID_PRE_FREEZE_BACKGROUND_MEMORY_TRIMMER_H_
