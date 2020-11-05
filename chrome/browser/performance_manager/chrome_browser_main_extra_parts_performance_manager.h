// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

namespace content {
class FeatureObserverClient;
}

namespace performance_manager {
class BrowserChildProcessWatcher;
class Graph;
class PageLiveStateDecoratorHelper;
class PageLoadMetricsObserver;
class PageLoadTrackerDecoratorHelper;
class PerformanceManager;
class PerformanceManagerFeatureObserverClient;
class PerformanceManagerRegistry;
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

  // Returns the FeatureObserverClient that should be exposed to //content to
  // allow the performance manager to track usage of features in frames. Valid
  // to call from any thread, but external synchronization is needed to make
  // sure that the performance manager is available.
  content::FeatureObserverClient* GetFeatureObserverClient();

 private:
  static void CreatePoliciesAndDecorators(performance_manager::Graph* graph);

  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;
  void PostMainMessageLoopRun() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  std::unique_ptr<performance_manager::PerformanceManager> performance_manager_;
  std::unique_ptr<performance_manager::PerformanceManagerRegistry> registry_;

  const std::unique_ptr<
      performance_manager::PerformanceManagerFeatureObserverClient>
      feature_observer_client_;

  std::unique_ptr<performance_manager::BrowserChildProcessWatcher>
      browser_child_process_watcher_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};

  // Needed to record "Pageloads" metrics.
  std::unique_ptr<performance_manager::PageLoadMetricsObserver>
      page_load_metrics_observer_;

  // Needed to maintain some of the PageLiveStateDecorator' properties.
  std::unique_ptr<performance_manager::PageLiveStateDecoratorHelper>
      page_live_state_data_helper_;

  // Needed to maintain the PageNode::IsLoading() property.
  std::unique_ptr<performance_manager::PageLoadTrackerDecoratorHelper>
      page_load_tracker_decorator_helper_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsPerformanceManager);
};

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
