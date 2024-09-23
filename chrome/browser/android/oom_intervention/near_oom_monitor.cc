// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NearOomMonitor_jni.h"

namespace {

// Default interval to check memory stats.
constexpr base::TimeDelta kDefaultMonitoringDelta = base::Seconds(1);

// Default cooldown interval to resume monitoring after a detection.
constexpr base::TimeDelta kDefaultCooldownDelta = base::Seconds(30);

}  // namespace

// static
NearOomMonitor* NearOomMonitor::Create() {
  auto* config = OomInterventionConfig::GetInstance();
  if (!config->is_swap_monitor_enabled())
    return nullptr;
  return new NearOomMonitor(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            config->swapfree_threshold());
}

// static
NearOomMonitor* NearOomMonitor::GetInstance() {
  static NearOomMonitor* instance = NearOomMonitor::Create();
  return instance;
}

NearOomMonitor::NearOomMonitor(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int64_t swapfree_threshold)
    : task_runner_(task_runner),
      check_callback_(
          base::BindRepeating(&NearOomMonitor::Check, base::Unretained(this))),
      monitoring_interval_(kDefaultMonitoringDelta),
      cooldown_interval_(kDefaultCooldownDelta),
      swapfree_threshold_(swapfree_threshold),
      component_callback_is_enabled_(
          OomInterventionConfig::GetInstance()->use_components_callback()) {
  if (ComponentCallbackIsEnabled()) {
    j_object_.Reset(Java_NearOomMonitor_create(
        base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this)));
  }
}

NearOomMonitor::~NearOomMonitor() = default;

base::CallbackListSubscription NearOomMonitor::RegisterCallback(
    base::RepeatingClosure callback) {
  if (callbacks_.empty() && !ComponentCallbackIsEnabled())
    ScheduleCheck();
  return callbacks_.Add(std::move(callback));
}

void NearOomMonitor::OnLowMemory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  callbacks_.Notify();
}

bool NearOomMonitor::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* memory_info) {
  DCHECK(memory_info);
  return base::GetSystemMemoryInfo(memory_info);
}

bool NearOomMonitor::ComponentCallbackIsEnabled() {
  return component_callback_is_enabled_;
}

void NearOomMonitor::Check() {
  base::SystemMemoryInfoKB memory_info;
  if (!GetSystemMemoryInfo(&memory_info)) {
    LOG(WARNING) << "Failed to get system memory info and stop monitoring.";
    return;
  }

  if (memory_info.swap_free <= swapfree_threshold_) {
    callbacks_.Notify();
    next_check_time_ = base::TimeTicks::Now() + cooldown_interval_;
  } else {
    next_check_time_ = base::TimeTicks::Now() + monitoring_interval_;
  }

  if (!callbacks_.empty())
    ScheduleCheck();
}

void NearOomMonitor::ScheduleCheck() {
  DCHECK(!ComponentCallbackIsEnabled());

  if (next_check_time_.is_null()) {
    task_runner_->PostTask(FROM_HERE, check_callback_);
  } else {
    base::TimeDelta delta =
        std::max(next_check_time_ - base::TimeTicks::Now(), base::TimeDelta());
    task_runner_->PostDelayedTask(FROM_HERE, check_callback_, delta);
  }
}
