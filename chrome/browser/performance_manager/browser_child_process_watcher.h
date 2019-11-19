// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/common/service_manager_connection.h"

namespace performance_manager {

class ProcessNodeImpl;

// Responsible for maintaining the process nodes for the browser and the GPU
// process.
class BrowserChildProcessWatcher : public content::BrowserChildProcessObserver {
 public:
  BrowserChildProcessWatcher();
  ~BrowserChildProcessWatcher() override;

  // Initialize this watcher.
  void Initialize();

  // Tear down this watcher and any state it's gathered.
  void TearDown();

 private:
  // BrowserChildProcessObserver overrides.
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  void GPUProcessExited(int id, int exit_code);

  static void OnProcessLaunched(const base::Process& process,
                                ProcessNodeImpl* process_node);

  std::unique_ptr<ProcessNodeImpl> browser_process_node_;

  // Apparently more than one GPU process can be existent at a time, though
  // secondaries are very transient. This map keeps track of all GPU processes
  // by their unique ID from |content::ChildProcessData|.
  base::flat_map<int, std::unique_ptr<ProcessNodeImpl>> gpu_process_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BrowserChildProcessWatcher);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_BROWSER_CHILD_PROCESS_WATCHER_H_
