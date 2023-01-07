// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_MONITOR_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_MONITOR_H_

#include "base/android/jni_android.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/process/process_metrics.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

// NearOomMonitor tracks memory stats to estimate whether we are in "near-OOM"
// situation. Near-OOM is defined as a situation where the foreground apps
// will be killed soon when we keep allocating memory.
// This monitor periodically checks memory stats and invokes registered
// callbacks when the monitor detects near-OOM situation.
class NearOomMonitor {
 public:
  // Returns nullptr when the monitor isn't enabled by
  // OomInterventionConfig::is_swap_monitor_enabled().
  static NearOomMonitor* GetInstance();

  NearOomMonitor(const NearOomMonitor&) = delete;
  NearOomMonitor& operator=(const NearOomMonitor&) = delete;

  virtual ~NearOomMonitor();

  base::TimeDelta GetMonitoringInterval() const { return monitoring_interval_; }
  base::TimeDelta GetCooldownInterval() const { return cooldown_interval_; }

  using CallbackList = base::RepeatingClosureList;

  // Registers a callback which is invoked when this monitor detects near-OOM
  // situation. The callback will be called on the task runner on which this
  // monitor is running. Destroy the returned subscription to unregister.
  base::CallbackListSubscription RegisterCallback(
      base::RepeatingClosure callback);

  void OnLowMemory(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jcaller);

 protected:
  static NearOomMonitor* Create();

  NearOomMonitor(scoped_refptr<base::SequencedTaskRunner> task_runner,
                 int64_t swapfree_threshold);

  // Gets system memory info. This is a virtual method so that we can override
  // this for testing.
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* memory_info);

  // Returns true when the monitor uses Android's memory pressure signals.
  // This is a virtual method so that we can override this for testing.
  virtual bool ComponentCallbackIsEnabled();

 private:
  // Checks whether we are in near-OOM situation.
  void Check();
  void ScheduleCheck();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::RepeatingCallback<void()> check_callback_;

  // The time between Check() calls. When Check() detects a near-OOM
  // situation, |cooldown_interval_| is used instead of this interval to
  // schedule next Check().
  base::TimeDelta monitoring_interval_;
  // The time which should pass between two successive near-OOM detections.
  base::TimeDelta cooldown_interval_;
  // The time when Check() will be called next.
  base::TimeTicks next_check_time_;

  int64_t swapfree_threshold_;

  CallbackList callbacks_;

  bool component_callback_is_enabled_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_MONITOR_H_
