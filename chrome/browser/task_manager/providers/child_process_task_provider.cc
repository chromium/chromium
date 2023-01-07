// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/child_process_task_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "chrome/browser/task_manager/providers/child_process_task.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::ChildProcessData;

namespace task_manager {

ChildProcessTaskProvider::ChildProcessTaskProvider() {}

ChildProcessTaskProvider::~ChildProcessTaskProvider() {
}

Task* ChildProcessTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                    int route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto itr = tasks_by_child_id_.find(child_id);
  if (itr == tasks_by_child_id_.end())
    return nullptr;

  return itr->second;
}

void ChildProcessTaskProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!data.GetProcess().IsValid())
    return;

  CreateTask(data);
}

void ChildProcessTaskProvider::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DeleteTask(data.GetProcess().Handle());
}

void ChildProcessTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(tasks_by_processid_.empty());
  DCHECK(tasks_by_child_id_.empty());

  // First, get the pre-existing child processes data.
  for (BrowserChildProcessHostIterator itr; !itr.Done(); ++itr) {
    const ChildProcessData& process_data = itr.GetData();

    // Only add processes that have already started, i.e. with valid handles.
    if (!process_data.GetProcess().IsValid())
      continue;

    CreateTask(process_data);
  }

  // Now start observing.
  BrowserChildProcessObserver::Add(this);
}

void ChildProcessTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // First, stop observing.
  BrowserChildProcessObserver::Remove(this);

  // Remember: You can't notify the observer of tasks removal here,
  // StopUpdating() is called after the observer has been cleared.

  // Then delete all tasks (if any).
  tasks_by_processid_.clear();
  tasks_by_child_id_.clear();
}

void ChildProcessTaskProvider::CreateTask(
    const content::ChildProcessData& data) {
  std::unique_ptr<ChildProcessTask>& task =
      tasks_by_processid_[data.GetProcess().Pid()];
  if (task) {
    // This task is already known to us. This case can happen when some of the
    // child process data we collect upon StartUpdating() might be of
    // BrowserChildProcessHosts whose process hadn't launched yet. So we just
    // return.
    return;
  }

  // Create the task and notify the observer.
  task = std::make_unique<ChildProcessTask>(
      data, ChildProcessTask::ProcessSubtype::kNoSubtype);
  tasks_by_child_id_[task->GetChildProcessUniqueID()] = task.get();
  NotifyObserverTaskAdded(task.get());
}

void ChildProcessTaskProvider::DeleteTask(base::ProcessHandle handle) {
  auto itr = tasks_by_processid_.find(base::GetProcId(handle));

  // The following case should never happen since we start observing
  // |BrowserChildProcessObserver| only after we collect all pre-existing child
  // processes and are notified (on the UI thread) that the collection is
  // completed at |ChildProcessDataCollected()|.
  if (itr == tasks_by_processid_.end()) {
    // BUG(crbug.com/611067): Temporarily removing due to test flakes. The
    // reason why this happens is well understood (see bug), but there's no
    // quick and easy fix.
    // NOTREACHED();
    return;
  }

  NotifyObserverTaskRemoved(itr->second.get());

  // Clear from the child_id index.
  tasks_by_child_id_.erase(itr->second->GetChildProcessUniqueID());

  // Finally delete the task.
  tasks_by_processid_.erase(itr);
}

}  // namespace task_manager
