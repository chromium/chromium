// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/render_process_host.h"

namespace task_manager {

class ChildProcessTask;

// This provides a task that represents the spare RenderProcessHost process.
class SpareRenderProcessHostTaskProvider : public TaskProvider {
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

  void SpareRenderProcessHostTaskChanged(content::RenderProcessHost* host);

  // The one task representing the spare render process host. If null, there is
  // no current spare render process host.
  std::unique_ptr<ChildProcessTask> task_;

  // The subscription for the notifications of the spare host changing.
  base::CallbackListSubscription subscription_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SPARE_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
