// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"

namespace task_manager {

class ChildProcessTask;

// This provides tasks that represent spare RenderProcessHost processes.
class SpareRenderProcessHostTaskProvider
    : public TaskProvider,
      public content::SpareRenderProcessHostManager::Observer {
 public:
  SpareRenderProcessHostTaskProvider();
  ~SpareRenderProcessHostTaskProvider() override;
  SpareRenderProcessHostTaskProvider(
      const SpareRenderProcessHostTaskProvider&) = delete;
  SpareRenderProcessHostTaskProvider& operator=(
      const SpareRenderProcessHostTaskProvider&) = delete;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  friend class SpareRenderProcessHostTaskTest;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // content::SpareRenderProcessHostManager::Observer:
  void OnSpareRenderProcessHostReady(content::RenderProcessHost* host) override;
  void OnSpareRenderProcessHostRemoved(
      content::RenderProcessHost* host) override;

  base::flat_map<int, std::unique_ptr<ChildProcessTask>> tasks_by_rph_id_;

  // The subscription for the notifications of the spare host changing.
  base::ScopedObservation<content::SpareRenderProcessHostManager,
                          content::SpareRenderProcessHostManager::Observer>
      scoped_observation_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
