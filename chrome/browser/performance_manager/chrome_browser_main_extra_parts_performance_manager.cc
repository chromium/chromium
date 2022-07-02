// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_browser_main_extra_parts_performance_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/browser_child_process_watcher.h"
#include "chrome/browser/performance_manager/decorators/frozen_frame_aggregator.h"
#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/metrics/memory_pressure_metrics.h"
#include "chrome/browser/performance_manager/observers/page_load_metrics_observer.h"
#include "chrome/browser/performance_manager/policies/background_tab_loading_policy.h"
#include "chrome/browser/performance_manager/policies/high_pmf_discard_policy.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/embedder/performance_manager_lifetime.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/performance_manager_feature_observer_client.h"
#include "components/performance_manager/public/decorators/page_load_tracker_decorator_helper.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/allocator/buildflags.h"
#if defined(ARCH_CPU_X86_64)
#include "chrome/browser/performance_manager/policies/userspace_swap_policy_chromeos.h"
#endif  // defined(ARCH_CPU_X86_64)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/performance_manager/extension_watcher.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy.h"
#include "chrome/browser/performance_manager/policies/high_efficiency_mode_policy_helper.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/page_freezing_policy.h"
#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"
#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"
#include "components/performance_manager/graph/policies/bfcache_policy.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {
ChromeBrowserMainExtraPartsPerformanceManager* g_instance = nullptr;
}

ChromeBrowserMainExtraPartsPerformanceManager::
    ChromeBrowserMainExtraPartsPerformanceManager()
    : feature_observer_client_(
          std::make_unique<
              performance_manager::PerformanceManagerFeatureObserverClient>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

ChromeBrowserMainExtraPartsPerformanceManager::
    ~ChromeBrowserMainExtraPartsPerformanceManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
ChromeBrowserMainExtraPartsPerformanceManager*
ChromeBrowserMainExtraPartsPerformanceManager::GetInstance() {
  return g_instance;
}

// static
void ChromeBrowserMainExtraPartsPerformanceManager::CreatePoliciesAndDecorators(
    performance_manager::Graph* graph) {
  graph->PassToGraph(std::make_unique<performance_manager::PageAggregator>());
  graph->PassToGraph(
      std::make_unique<performance_manager::FrozenFrameAggregator>());
  graph->PassToGraph(
      std::make_unique<performance_manager::ProcessMetricsDecorator>());

  if (performance_manager::policies::WorkingSetTrimmerPolicy::
          PlatformSupportsWorkingSetTrim()) {
    graph->PassToGraph(performance_manager::policies::WorkingSetTrimmerPolicy::
                           CreatePolicyForPlatform());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(ARCH_CPU_X86_64)
  if (performance_manager::policies::UserspaceSwapPolicy::
          UserspaceSwapSupportedAndEnabled()) {
    graph->PassToGraph(
        std::make_unique<performance_manager::policies::UserspaceSwapPolicy>());
  }
#endif  // defined(ARCH_CPU_X86_64)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
  graph->PassToGraph(FormInteractionTabHelper::CreateGraphObserver());

  // The PageDiscardingHelper instance is required by the HighPMFDiscardPolicy
  // and by UrgentDiscardingFromPerformanceManager.

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighPMFDiscardPolicy) ||
      base::FeatureList::IsEnabled(
          performance_manager::features::
              kUrgentDiscardingFromPerformanceManager)) {
    graph->PassToGraph(std::make_unique<
                       performance_manager::policies::PageDiscardingHelper>());
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kUrgentDiscardingFromPerformanceManager)) {
    graph->PassToGraph(
        std::make_unique<
            performance_manager::policies::UrgentPageDiscardingPolicy>());
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::
              kBackgroundTabLoadingFromPerformanceManager)) {
    graph->PassToGraph(
        std::make_unique<
            performance_manager::policies::BackgroundTabLoadingPolicy>());
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighPMFDiscardPolicy)) {
    graph->PassToGraph(std::make_unique<
                       performance_manager::policies::HighPMFDiscardPolicy>());
  }

  // The freezing policy isn't enabled on Android yet as it doesn't play well
  // with the freezing logic already in place in renderers. This logic should be
  // moved to PerformanceManager, this is tracked in https://crbug.com/1156803.
  graph->PassToGraph(
      std::make_unique<performance_manager::policies::PageFreezingPolicy>());

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable)) {
    graph->PassToGraph(
        std::make_unique<
            performance_manager::policies::HighEfficiencyModePolicy>());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  graph->PassToGraph(
      std::make_unique<performance_manager::metrics::MemoryPressureMetrics>());

  // TODO(crbug.com/1225070): Consider using this policy on Android.
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kBFCachePerformanceManagerPolicy)) {
    graph->PassToGraph(
        std::make_unique<performance_manager::policies::BFCachePolicy>());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

content::FeatureObserverClient*
ChromeBrowserMainExtraPartsPerformanceManager::GetFeatureObserverClient() {
  return feature_observer_client_.get();
}

void ChromeBrowserMainExtraPartsPerformanceManager::PostCreateThreads() {
  performance_manager_lifetime_ =
      std::make_unique<performance_manager::PerformanceManagerLifetime>(
          performance_manager::GraphFeatures::WithDefault(),
          base::BindOnce(&ChromeBrowserMainExtraPartsPerformanceManager::
                             CreatePoliciesAndDecorators));
  browser_child_process_watcher_ =
      std::make_unique<performance_manager::BrowserChildProcessWatcher>();
  browser_child_process_watcher_->Initialize();

  // There are no existing loaded profiles.
  DCHECK(g_browser_process->profile_manager()->GetLoadedProfiles().empty());

  g_browser_process->profile_manager()->AddObserver(this);

  page_load_metrics_observer_ =
      std::make_unique<performance_manager::PageLoadMetricsObserver>();
  page_live_state_data_helper_ =
      std::make_unique<performance_manager::PageLiveStateDecoratorHelper>();
  page_load_tracker_decorator_helper_ =
      std::make_unique<performance_manager::PageLoadTrackerDecoratorHelper>();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_watcher_ =
      std::make_unique<performance_manager::ExtensionWatcher>();
#endif
}

void ChromeBrowserMainExtraPartsPerformanceManager::PreMainMessageLoopRun() {
#if !BUILDFLAG(IS_ANDROID)
  // This object requires the host frame sink manager to exist, which is created
  // after all the extra parts have run their PostCreateThreads.
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kHighEfficiencyModeAvailable) ||
      base::FeatureList::IsEnabled(
          performance_manager::features::kBatterySaverModeAvailable)) {
    high_efficiency_mode_policy_helper_ = std::make_unique<
        performance_manager::policies::HighEfficiencyModePolicyHelper>(
        g_browser_process->local_state());
  }
#endif
}

void ChromeBrowserMainExtraPartsPerformanceManager::PostMainMessageLoopRun() {
  // Release all graph nodes before destroying the performance manager.
  // First release the browser and GPU process nodes.
  browser_child_process_watcher_->TearDown();
  browser_child_process_watcher_.reset();

  g_browser_process->profile_manager()->RemoveObserver(this);
  profile_observations_.RemoveAllObservations();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_watcher_.reset();
#endif
  page_load_tracker_decorator_helper_.reset();
  page_live_state_data_helper_.reset();
  page_load_metrics_observer_.reset();

#if !BUILDFLAG(IS_ANDROID)
  high_efficiency_mode_policy_helper_.reset();
#endif

  // Releasing `performance_manager_lifetime_` will tear down the registry and
  // graph safely.
  performance_manager_lifetime_.reset();
}

void ChromeBrowserMainExtraPartsPerformanceManager::OnProfileAdded(
    Profile* profile) {
  profile_observations_.AddObservation(profile);
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->NotifyBrowserContextAdded(profile);
}

void ChromeBrowserMainExtraPartsPerformanceManager::
    OnOffTheRecordProfileCreated(Profile* off_the_record) {
  OnProfileAdded(off_the_record);
}

void ChromeBrowserMainExtraPartsPerformanceManager::OnProfileWillBeDestroyed(
    Profile* profile) {
  profile_observations_.RemoveObservation(profile);
  performance_manager::PerformanceManagerRegistry::GetInstance()
      ->NotifyBrowserContextRemoved(profile);
}
