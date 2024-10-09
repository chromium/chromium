// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/spare_render_process_host_task_provider.h"

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "chrome/browser/task_manager/providers/child_process_task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/common/process_type.h"

using content::BrowserThread;
using content::ChildProcessData;
using content::RenderProcessHost;

namespace task_manager {

SpareRenderProcessHostTaskProvider::SpareRenderProcessHostTaskProvider() =
    default;

SpareRenderProcessHostTaskProvider::~SpareRenderProcessHostTaskProvider() =
    default;

Task* SpareRenderProcessHostTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                              int route_id) {
  // It's called the "spare" host because it isn't yet handling requests.
  return nullptr;
}

void SpareRenderProcessHostTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_by_rph_id_.empty());

  auto& spare_manager = content::SpareRenderProcessHostManager::Get();
  for (RenderProcessHost* spare_rph : spare_manager.GetSpares()) {
    if (spare_rph->IsReady()) {
      OnSpareRenderProcessHostReady(spare_rph);
    }
  }

  scoped_observation_.Observe(&spare_manager);
}

void SpareRenderProcessHostTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_observation_.Reset();

  // Destroy all tasks.
  tasks_by_rph_id_.clear();
}

void SpareRenderProcessHostTaskProvider::OnSpareRenderProcessHostReady(
    RenderProcessHost* host) {
  ChildProcessData data(content::PROCESS_TYPE_RENDERER);
  data.SetProcess(host->GetProcess().Duplicate());
  data.id = host->GetID();
  auto task = std::make_unique<ChildProcessTask>(
      data, ChildProcessTask::ProcessSubtype::kSpareRenderProcess);

  auto [it, inserted] =
      tasks_by_rph_id_.emplace(host->GetID(), std::move(task));
  CHECK(inserted);

  NotifyObserverTaskAdded(it->second.get());
}

void SpareRenderProcessHostTaskProvider::OnSpareRenderProcessHostRemoved(
    RenderProcessHost* host) {
  auto it = tasks_by_rph_id_.find(host->GetID());
  if (it == tasks_by_rph_id_.end()) {
    // This can happen when a spare RenderProcessHost was created but never
    // reached the "ready" state.
    return;
  }

  NotifyObserverTaskRemoved(it->second.get());
  tasks_by_rph_id_.erase(it);
}

}  // namespace task_manager
