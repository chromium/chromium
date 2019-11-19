// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/browser_child_process_watcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

BrowserChildProcessWatcher::BrowserChildProcessWatcher() = default;

BrowserChildProcessWatcher::~BrowserChildProcessWatcher() {
  DCHECK(!browser_process_node_);
  DCHECK(gpu_process_nodes_.empty());
}

void BrowserChildProcessWatcher::Initialize() {
  DCHECK(!browser_process_node_);
  DCHECK(gpu_process_nodes_.empty());

  browser_process_node_ =
      PerformanceManagerImpl::GetInstance()->CreateProcessNode(
          RenderProcessHostProxy());
  OnProcessLaunched(base::Process::Current(), browser_process_node_.get());
  BrowserChildProcessObserver::Add(this);
}

void BrowserChildProcessWatcher::TearDown() {
  BrowserChildProcessObserver::Remove(this);

  PerformanceManagerImpl* performance_manager =
      PerformanceManagerImpl::GetInstance();
  performance_manager->DeleteNode(std::move(browser_process_node_));
  for (auto& node : gpu_process_nodes_)
    performance_manager->DeleteNode(std::move(node.second));
  gpu_process_nodes_.clear();
}

void BrowserChildProcessWatcher::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    std::unique_ptr<ProcessNodeImpl> gpu_node =
        PerformanceManagerImpl::GetInstance()->CreateProcessNode(
            RenderProcessHostProxy());
    OnProcessLaunched(data.GetProcess(), gpu_node.get());
    gpu_process_nodes_[data.id] = std::move(gpu_node);
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  if (data.process_type == content::PROCESS_TYPE_GPU) {
    auto it = gpu_process_nodes_.find(data.id);
    // Apparently there are cases where a disconnect notification arrives here
    // either multiple times for the same process, or else before a
    // launch-and-connect notification arrives.
    // See https://crbug.com/942500.
    if (it != gpu_process_nodes_.end()) {
      PerformanceManagerImpl::GetInstance()->DeleteNode(std::move(it->second));
      gpu_process_nodes_.erase(it);
    }
  }
}

void BrowserChildProcessWatcher::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessExited(data.id, info.exit_code);
}

void BrowserChildProcessWatcher::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  if (data.process_type == content::PROCESS_TYPE_GPU)
    GPUProcessExited(data.id, info.exit_code);
}

void BrowserChildProcessWatcher::GPUProcessExited(int id, int exit_code) {
  // It appears the exit code can be delivered either after the host is
  // disconnected, or perhaps before the HostConnected notification,
  // specifically on crash.
  if (base::Contains(gpu_process_nodes_, id)) {
    auto* process_node = gpu_process_nodes_[id].get();

    DCHECK(PerformanceManagerImpl::GetInstance());
    PerformanceManagerImpl::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessNodeImpl::SetProcessExitStatus,
                                  base::Unretained(process_node), exit_code));
  }
}

// static
void BrowserChildProcessWatcher::OnProcessLaunched(
    const base::Process& process,
    ProcessNodeImpl* process_node) {
  const base::Time launch_time =
#if defined(OS_ANDROID)
      // Process::CreationTime() is not available on Android. Since this method
      // is called immediately after the process is launched, the process launch
      // time can be approximated with the current time.
      base::Time::Now();
#else
      process.CreationTime();
#endif

  DCHECK(PerformanceManagerImpl::GetInstance());
  PerformanceManagerImpl::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessNodeImpl::SetProcess,
                                base::Unretained(process_node),
                                process.Duplicate(), launch_time));
}

}  // namespace performance_manager
