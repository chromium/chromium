// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_interface.h"

#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_manager/sampling/task_manager_impl.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_dialogs.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace task_manager {

// static
void TaskManagerInterface::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
  registry->RegisterDictionaryPref(prefs::kTaskManagerColumnVisibility);
  registry->RegisterBooleanPref(prefs::kTaskManagerEndProcessEnabled, true);
}

// static
bool TaskManagerInterface::IsEndProcessEnabled() {
  PrefService* state = g_browser_process->local_state();
  return !state || state->GetBoolean(prefs::kTaskManagerEndProcessEnabled);
}

// static
TaskManagerInterface* TaskManagerInterface::GetTaskManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return TaskManagerImpl::GetInstance();
}

// static
void TaskManagerInterface::UpdateAccumulatedStatsNetworkForRoute(
    content::GlobalRenderFrameHostId render_frame_host_id,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  // Don't create a task manager if it hasn't already been created.
  if (TaskManagerImpl::IsCreated()) {
    TaskManagerImpl::GetInstance()->UpdateAccumulatedStatsNetworkForRoute(
        render_frame_host_id, recv_bytes, sent_bytes);
  }
}

void TaskManagerInterface::AddObserver(TaskManagerObserver* observer) {
  observers_.AddObserver(observer);
  observer->observed_task_manager_ = this;

  ResourceFlagsAdded(observer->desired_resources_flags());

  base::TimeDelta current_refresh_time = GetCurrentRefreshTime();
  if (current_refresh_time == base::TimeDelta::Max()) {
    // This is the first observer to be added. Start updating.
    StartUpdating();
  }

  if (observer->desired_refresh_time() > current_refresh_time)
    return;

  // Reached here, then this is EITHER (not the first observer to be added AND
  // it requires a more frequent refresh rate) OR (it's the very first observer
  // to be added).
  // Reset the refresh timer.
  ScheduleRefresh(observer->desired_refresh_time());
}

void TaskManagerInterface::RemoveObserver(TaskManagerObserver* observer) {
  observers_.RemoveObserver(observer);
  observer->observed_task_manager_ = nullptr;

  // Recalculate the minimum refresh rate and the enabled resource flags.
  int64_t flags = 0;
  base::TimeDelta min_time = base::TimeDelta::Max();
  for (auto& obs : observers_) {
    if (obs.desired_refresh_time() < min_time)
      min_time = obs.desired_refresh_time();

    flags |= obs.desired_resources_flags();
  }

  if (min_time == base::TimeDelta::Max()) {
    // This is the last observer to be removed. Stop updating.
    SetEnabledResourceFlags(0);
    refresh_timer_->Stop();
    StopUpdating();
  } else {
    SetEnabledResourceFlags(flags);
    ScheduleRefresh(min_time);
  }
}

void TaskManagerInterface::RecalculateRefreshFlags() {
  int64_t flags = 0;
  for (auto& observer : observers_)
    flags |= observer.desired_resources_flags();

  SetEnabledResourceFlags(flags);
}

bool TaskManagerInterface::IsResourceRefreshEnabled(RefreshType type) const {
  return (enabled_resources_flags_ & type) != 0;
}

TaskManagerInterface::TaskManagerInterface()
    : refresh_timer_(new base::RepeatingTimer()), enabled_resources_flags_(0) {}

TaskManagerInterface::~TaskManagerInterface() = default;

void TaskManagerInterface::NotifyObserversOnTaskAdded(TaskId id) {
  for (TaskManagerObserver& observer : observers_)
    observer.OnTaskAdded(id);
}

void TaskManagerInterface::NotifyObserversOnTaskToBeRemoved(TaskId id) {
  for (TaskManagerObserver& observer : observers_)
    observer.OnTaskToBeRemoved(id);
}

void TaskManagerInterface::NotifyObserversOnRefresh(
    const TaskIdList& task_ids) {
  for (TaskManagerObserver& observer : observers_)
    observer.OnTasksRefreshed(task_ids);
}

void TaskManagerInterface::NotifyObserversOnRefreshWithBackgroundCalculations(
    const TaskIdList& task_ids) {
  for (TaskManagerObserver& observer : observers_)
    observer.OnTasksRefreshedWithBackgroundCalculations(task_ids);
}

void TaskManagerInterface::NotifyObserversOnTaskUnresponsive(TaskId id) {
  for (TaskManagerObserver& observer : observers_)
    observer.OnTaskUnresponsive(id);
}

void TaskManagerInterface::NotifyObserversOnActiveTaskFetched(TaskId id) {
  for (TaskManagerObserver& observer : observers_) {
    observer.OnActiveTaskFetched(id);
  }
}

base::TimeDelta TaskManagerInterface::GetCurrentRefreshTime() const {
  return refresh_timer_->IsRunning() ? refresh_timer_->GetCurrentDelay()
                                     : base::TimeDelta::Max();
}

void TaskManagerInterface::ResourceFlagsAdded(int64_t flags) {
  SetEnabledResourceFlags(enabled_resources_flags_ | flags);
}

void TaskManagerInterface::SetEnabledResourceFlags(int64_t flags) {
  enabled_resources_flags_ = flags;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set refresh flags of the remote task manager if lacros is enabled.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->task_manager_ash()
        ->SetRefreshFlags(enabled_resources_flags_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void TaskManagerInterface::ScheduleRefresh(base::TimeDelta refresh_time) {
  refresh_timer_->Start(FROM_HERE, refresh_time,
                        base::BindRepeating(&TaskManagerInterface::Refresh,
                                            base::Unretained(this)));
}

}  // namespace task_manager
