// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_

#include <map>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"

namespace task_manager {

class ChildProcessTask;

// This provides tasks that represent RenderProcessHost processes. It does so by
// listening to the notification service for the creation and destruction of the
// RenderProcessHost.
class RenderProcessHostTaskProvider
    : public TaskProvider,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  RenderProcessHostTaskProvider();
  RenderProcessHostTaskProvider(const RenderProcessHostTaskProvider&) = delete;
  RenderProcessHostTaskProvider& operator=(
      const RenderProcessHostTaskProvider&) = delete;
  ~RenderProcessHostTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates a RenderProcessHostTask from the given |data| and notifies the
  // observer of its addition.
  void CreateTask(const int render_process_host_id);

  // Deletes a RenderProcessHostTask whose |render_process_host_id| is provided
  // after notifying the observer of its deletion.
  void DeleteTask(const int render_process_host_id);

  // True if the provider is between StartUpdating() and StopUpdating().
  bool is_updating_ = false;

  std::map<int, std::unique_ptr<ChildProcessTask>> tasks_by_rph_id_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<RenderProcessHostTaskProvider> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
