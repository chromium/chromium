// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/tab_helper_frame_node_source.h"

class Profile;

namespace content {
class LockObserver;
}

namespace performance_manager {
class BrowserChildProcessWatcher;
class GraphImpl;
class PerformanceManagerImpl;
class SharedWorkerWatcher;
}  // namespace performance_manager

// Handles the initialization of the performance manager and a few dependent
// classes that create/manage graph nodes.
class ChromeBrowserMainExtraPartsPerformanceManager
    : public ChromeBrowserMainExtraParts,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  ChromeBrowserMainExtraPartsPerformanceManager();
  ~ChromeBrowserMainExtraPartsPerformanceManager() override;

  // Returns the only instance of this class.
  static ChromeBrowserMainExtraPartsPerformanceManager* GetInstance();

  static void CreateDefaultPoliciesAndDecorators(
      performance_manager::GraphImpl* graph);

  // Returns the LockObserver that should be exposed to //content to allow the
  // performance manager to track usage of locks in frames. Valid to call from
  // any thread, but external synchronization is needed to make sure that the
  // performance manager is available.
  content::LockObserver* GetLockObserver();

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;
  void PostMainMessageLoopRun() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  std::unique_ptr<performance_manager::PerformanceManagerImpl>
      performance_manager_;

  // This must be alive at least until the end of base::ThreadPool shutdown,
  // because it can be accessed by IndexedDB which runs on a base::ThreadPool
  // sequence.
  const std::unique_ptr<content::LockObserver> lock_observer_;

  std::unique_ptr<performance_manager::BrowserChildProcessWatcher>
      browser_child_process_watcher_;

  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  // Needed by the worker watchers to access existing process nodes and frame
  // nodes.
  performance_manager::ProcessNodeSource process_node_source_;
  performance_manager::TabHelperFrameNodeSource frame_node_source_;

  // Observes the lifetime of shared workers.
  base::flat_map<Profile*,
                 std::unique_ptr<performance_manager::SharedWorkerWatcher>>
      shared_worker_watchers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsPerformanceManager);
};

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
