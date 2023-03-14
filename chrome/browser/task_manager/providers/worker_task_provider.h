// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/task_manager/providers/task_provider.h"

class ProfileManager;

namespace task_manager {

class PerProfileWorkerTaskTracker;

// This is an implementation of TaskProvider that tracks dedicated workers,
// shared workers and service workers.
//
// It doesn't directly create the tasks but instead creates an instance of
// PerProfileWorkerTaskTracker for each existing profile. These
// PerProfileWorkerTaskTracker instances are then responsible for tracking
// workers and creating/deleting tasks.

// See https://w3c.github.io/workers/ or https://w3c.github.io/ServiceWorker/
// for more details.
class WorkerTaskProvider : public TaskProvider,
                           public ProfileManagerObserver,
                           public ProfileObserver {
 public:
  WorkerTaskProvider();
  ~WorkerTaskProvider() override;

  WorkerTaskProvider(const WorkerTaskProvider& other) = delete;
  WorkerTaskProvider& operator=(const WorkerTaskProvider& other) = delete;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Invoked by the PerProfileWorkerTaskTrackers when a new task is created or
  // deleted.
  void OnWorkerTaskAdded(Task* worker_task);
  void OnWorkerTaskRemoved(Task* worker_task);

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  // Observes all types of workers for a given profile.
  base::flat_map<Profile*, std::unique_ptr<PerProfileWorkerTaskTracker>>
      per_profile_worker_task_trackers_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_PROVIDER_H_
