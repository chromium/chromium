// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/worker_task_provider.h"

#include "base/not_fatal_until.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/providers/per_profile_worker_task_tracker.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

WorkerTaskProvider::WorkerTaskProvider() = default;

WorkerTaskProvider::~WorkerTaskProvider() = default;

Task* WorkerTaskProvider::GetTaskOfUrlRequest(int child_id, int route_id) {
  return nullptr;
}

void WorkerTaskProvider::OnProfileAdded(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // It is possible for this method to be called multiple times for the same
  // profile, if the profile loads an extension during initialization which also
  // triggers this logic path. https://crbug.com/1065798.
  if (observed_profiles_.IsObservingSource(profile))
    return;

  observed_profiles_.AddObservation(profile);

  auto per_profile_worker_task_tracker =
      std::make_unique<PerProfileWorkerTaskTracker>(this, profile);
  const bool inserted =
      per_profile_worker_task_trackers_
          .emplace(profile, std::move(per_profile_worker_task_tracker))
          .second;
  DCHECK(inserted);
}

void WorkerTaskProvider::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void WorkerTaskProvider::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnProfileAdded(off_the_record);
}

void WorkerTaskProvider::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  observed_profiles_.RemoveObservation(profile);

  auto it = per_profile_worker_task_trackers_.find(profile);
  CHECK(it != per_profile_worker_task_trackers_.end(),
        base::NotFatalUntil::M130);
  per_profile_worker_task_trackers_.erase(it);
}

void WorkerTaskProvider::OnWorkerTaskAdded(Task* worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NotifyObserverTaskAdded(worker_task);
}

void WorkerTaskProvider::OnWorkerTaskRemoved(Task* worker_task) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Do not forward the notification when StopUpdating() has been called, as the
  // observer is now null.
  if (!IsUpdating())
    return;

  NotifyObserverTaskRemoved(worker_task);
}

void WorkerTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);

    auto loaded_profiles = profile_manager->GetLoadedProfiles();
    for (auto* profile : loaded_profiles) {
      OnProfileAdded(profile);

      // If any off-the-record profile exists, we have to check them and create
      // the tasks if there are any.
      for (Profile* otr : profile->GetAllOffTheRecordProfiles())
        OnProfileAdded(otr);
    }
  }
}

void WorkerTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop observing profile creation and destruction.
  profile_manager_observation_.Reset();
  observed_profiles_.RemoveAllObservations();

  // Clear all ProfileWorkerTaskProvider instances to remove existing tasks.
  per_profile_worker_task_trackers_.clear();
}

}  // namespace task_manager
