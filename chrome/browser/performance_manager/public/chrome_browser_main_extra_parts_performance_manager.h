// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/scoped_multi_source_observation.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "extensions/buildflags/buildflags.h"

class Profile;

#if !BUILDFLAG(IS_ANDROID)
namespace base {
class BatteryStateSampler;
}
#endif

namespace content {
class FeatureObserverClient;
}

namespace performance_manager {
class Graph;
class PageLiveStateDecoratorHelper;
class PageLoadMetricsObserver;
class PageLoadTrackerDecoratorHelper;
class PerformanceManagerFeatureObserverClient;
class PerformanceManagerLifetime;
class ScopedGlobalScenarioMemory;

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionWatcher;
#endif

namespace user_tuning {
class BatterySaverModeManager;
class PerformanceDetectionManager;
class ProfileDiscardOptOutListHelper;
class UserPerformanceTuningManager;
}  // namespace user_tuning

}  // namespace performance_manager

// Handles the initialization of the performance manager and a few dependent
// classes that create/manage graph nodes.
class ChromeBrowserMainExtraPartsPerformanceManager
    : public ChromeBrowserMainExtraParts,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  ChromeBrowserMainExtraPartsPerformanceManager();

  ChromeBrowserMainExtraPartsPerformanceManager(
      const ChromeBrowserMainExtraPartsPerformanceManager&) = delete;
  ChromeBrowserMainExtraPartsPerformanceManager& operator=(
      const ChromeBrowserMainExtraPartsPerformanceManager&) = delete;

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
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Manages the lifetime of the PerformanceManager graph and registry for the
  // browser process.
  std::unique_ptr<performance_manager::PerformanceManagerLifetime>
      performance_manager_lifetime_;

  const std::unique_ptr<
      performance_manager::PerformanceManagerFeatureObserverClient>
      feature_observer_client_;

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

  std::unique_ptr<performance_manager::ScopedGlobalScenarioMemory>
      global_performance_scenario_memory_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<performance_manager::ExtensionWatcher> extension_watcher_;
#endif

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<performance_manager::user_tuning::BatterySaverModeManager>
      battery_saver_mode_manager_;
  std::unique_ptr<
      performance_manager::user_tuning::UserPerformanceTuningManager>
      user_performance_tuning_manager_;
  std::unique_ptr<
      performance_manager::user_tuning::ProfileDiscardOptOutListHelper>
      profile_discard_opt_out_list_helper_;
  std::unique_ptr<base::BatteryStateSampler> battery_state_sampler_;
  std::unique_ptr<performance_manager::user_tuning::PerformanceDetectionManager>
      performance_detection_manager_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
