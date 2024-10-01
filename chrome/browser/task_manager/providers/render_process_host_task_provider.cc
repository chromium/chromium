// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/render_process_host_task_provider.h"

#include "base/process/process.h"
#include "chrome/browser/task_manager/providers/child_process_task.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "extensions/buildflags/buildflags.h"

using content::RenderProcessHost;
using content::BrowserThread;
using content::ChildProcessData;

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

namespace task_manager {

RenderProcessHostTaskProvider::RenderProcessHostTaskProvider() = default;

RenderProcessHostTaskProvider::~RenderProcessHostTaskProvider() = default;

Task* RenderProcessHostTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                         int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto itr = tasks_by_rph_id_.find(child_id);
  if (itr == tasks_by_rph_id_.end())
    return nullptr;

  return itr->second.get();
}

void RenderProcessHostTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_by_rph_id_.empty());
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (host->GetProcess().IsValid()) {
      CreateTask(host->GetID());
    } else {
      // If the host isn't ready do nothing and we will learn of its creation
      // from the notification service.
    }
  }

  is_updating_ = true;
}

void RenderProcessHostTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Then delete all tasks (if any).
  tasks_by_rph_id_.clear();

  is_updating_ = false;
}

void RenderProcessHostTaskProvider::CreateTask(
    const int render_process_host_id) {
  // Checks that the task by RenderProcessHost ID isn't already a task in the
  // map and deletes it if it is so they new task can be cleanly added.
  DeleteTask(render_process_host_id);
  std::unique_ptr<ChildProcessTask>& task =
      tasks_by_rph_id_[render_process_host_id];

  RenderProcessHost* host = RenderProcessHost::FromID(render_process_host_id);

  // TODO(cburn): plumb out something from RPH so the title can be set here.
  // Create the task and notify the observer.
  ChildProcessData data(content::PROCESS_TYPE_RENDERER);
  data.SetProcess(host->GetProcess().Duplicate());
  data.id = host->GetID();
  task = std::make_unique<ChildProcessTask>(
      data, ChildProcessTask::ProcessSubtype::kUnknownRenderProcess);
  NotifyObserverTaskAdded(task.get());
}

void RenderProcessHostTaskProvider::DeleteTask(
    const int render_process_host_id) {
  auto itr = tasks_by_rph_id_.find(render_process_host_id);
  // If the render process host id isn't being tracked in |tasks_by_rph_id| do
  // nothing.
  if (itr == tasks_by_rph_id_.end())
    return;

  NotifyObserverTaskRemoved(itr->second.get());

  // Finally delete the task.
  tasks_by_rph_id_.erase(itr);
}

void RenderProcessHostTaskProvider::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (is_updating_) {
    CreateTask(host->GetID());
    if (!host_observation_.IsObservingSource(host)) {
      host_observation_.AddObservation(host);
    }
  }
}

void RenderProcessHostTaskProvider::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DeleteTask(host->GetID());
  host_observation_.RemoveObservation(host);
}

void RenderProcessHostTaskProvider::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DeleteTask(host->GetID());
  host_observation_.RemoveObservation(host);
}

}  // namespace task_manager
