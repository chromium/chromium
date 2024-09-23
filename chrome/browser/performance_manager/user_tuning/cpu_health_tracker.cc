// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"

namespace performance_manager::user_tuning {

CpuHealthTracker::CpuHealthTracker(
    StatusChangeCallback on_status_change_cb,
    ActionableTabResultCallback on_actionability_change_cb)
    : status_change_cb_(std::move(on_status_change_cb)),
      actionable_tabs_cb_(std::move(on_actionability_change_cb)),
      cpu_health_sample_window_size_(
          performance_manager::features::kCPUTimeOverThreshold.Get() /
          performance_manager::features::kCPUSampleFrequency.Get()),
      is_demo_mode_(base::FeatureList::IsEnabled(
          features::kPerformanceInterventionDemoMode)),
      recent_resource_measurements_(cpu_health_sample_window_size_,
                                    CpuPercent(0)),
      // scoped_cpu_query_ is initialized to monitor CPU usage. Actual queries
      // are being sent from ProcessCpuProbeResult().
      scoped_cpu_query_(
          resource_attribution::QueryBuilder()
              .AddAllContextsOfType<resource_attribution::PageContext>()
              .AddResourceType(resource_attribution::ResourceType::kCPUTime)
              .CreateScopedQuery()) {
  std::unique_ptr<system_cpu::CpuProbe> cpu_probe =
      system_cpu::CpuProbe::Create();
  cpu_probe->StartSampling();
  cpu_probe_timer_.Start(
      FROM_HERE, performance_manager::features::kCPUSampleFrequency.Get(),
      base::BindRepeating(
          &system_cpu::CpuProbe::RequestSample, std::move(cpu_probe),
          base::BindRepeating(&CpuHealthTracker::ProcessCpuProbeResult,
                              base::Unretained(this))));
  // base::Unretained(this) is safe here because the CPU probe is owned by the
  // callback, which is owned by the timer. The timer is owned by this, so the
  // callback will not be invoked after this is destroyed
}

CpuHealthTracker::~CpuHealthTracker() = default;

CpuHealthTracker::HealthLevel CpuHealthTracker::GetCurrentHealthLevel() {
  return current_health_status_;
}

int CpuHealthTracker::GetTotalCpuPercentUsage(ActionableTabsResult tabs) {
  int total_cpu = 0;
  for (resource_attribution::PageContext context : tabs) {
    auto iter = tab_page_measurements_.find(context);
    if (iter != tab_page_measurements_.end()) {
      total_cpu += iter->second.value();
    }
  }
  return total_cpu;
}

void CpuHealthTracker::QueryAndProcessTabActionability(
    std::optional<CpuPercent> system_cpu_usage_percentage) {
  // We must have a value for system CPU usage while not in demo mode to
  // properly determine tab actionability. In demo mode we ignore CPU thresholds
  // when determining tab actionability so system CPU usage is irrelevant in
  // this case.
  CHECK(system_cpu_usage_percentage.has_value() || is_demo_mode_);
  resource_attribution::QueryBuilder()
      .AddResourceType(resource_attribution::ResourceType::kCPUTime)
      .AddAllContextsOfType<resource_attribution::PageContext>()
      .QueryOnce(base::BindOnce(&CpuHealthTracker::ProcessQueryResultMap,
                                weak_ptr_factory_.GetWeakPtr(),
                                system_cpu_usage_percentage.value_or(
                                    recent_resource_measurements_.back())));
}

base::OnceCallback<void(CpuHealthTracker::ActionableTabsResult)>
CpuHealthTracker::GetStatusAndActionabilityCallback(
    bool did_status_change,
    CpuHealthTracker::HealthLevel health_level) {
  return base::BindOnce(
      [](bool is_demo_mode, StatusChangeCallback status_change,
         ActionableTabResultCallback actionability_change,
         bool did_status_change, HealthLevel health_level,
         ActionableTabsResult previously_actionable,
         ActionableTabsResult actionable_tabs) {
        if (did_status_change) {
          status_change.Run(ResourceType::kCpu, health_level,
                            !actionable_tabs.empty());
        }

        if (is_demo_mode || (previously_actionable != actionable_tabs)) {
          actionability_change.Run(ResourceType::kCpu, actionable_tabs);
        }
      },
      is_demo_mode_, status_change_cb_, actionable_tabs_cb_, did_status_change,
      health_level, actionable_tabs_);
}

CpuHealthTracker::HealthLevel CpuHealthTracker::GetHealthLevelForMeasurement(
    CpuPercent measurement) {
  if (measurement >
      CpuPercent(performance_manager::features::kCPUUnhealthyPercentageThreshold
                     .Get())) {
    return HealthLevel::kUnhealthy;
  }

  if (measurement >
      CpuPercent(
          performance_manager::features::kCPUDegradedHealthPercentageThreshold
              .Get())) {
    return HealthLevel::kDegraded;
  }

  return HealthLevel::kHealthy;
}

void CpuHealthTracker::GetFilteredActionableTabs(
    PageResourceMeasurements unfiltered_measurements,
    CpuPercent recent_measurement,
    base::OnceCallback<void(ActionableTabsResult)> callback) {
  // Sort the measurements in descending order
  std::vector<std::pair<resource_attribution::PageContext, CpuPercent>>
      sorted_measurements = base::ToVector(unfiltered_measurements);
  std::sort(
      sorted_measurements.begin(), sorted_measurements.end(),
      [](const std::pair<resource_attribution::PageContext, CpuPercent>& pair1,
         const std::pair<resource_attribution::PageContext, CpuPercent>&
             pair2) { return pair1.second > pair2.second; });

  ActionableTabsResult actionable_tabs;
  int total_actionable_cpu_percentage = 0;
  bool take_action_improves_health = is_demo_mode_;
  const size_t max_actionable_tabs =
      std::min(unfiltered_measurements.size(),
               size_t(features::kCPUMaxActionableTabs.Get()));
  const int recent_measurement_percentage = recent_measurement.value();

  for (size_t i = 0; i < max_actionable_tabs; i++) {
    const auto& [context, measurement] = sorted_measurements.at(i);

    // Since sorted_measurements is sorted in descending order, we can
    // terminate early as there is no longer any eligible actionable pages.
    if (!is_demo_mode_ &&
        measurement.value() <
            performance_manager::features::kMinimumActionableTabCPUPercentage
                .Get()) {
      break;
    }

    if (CanDiscardPage(context)) {
      total_actionable_cpu_percentage += measurement.value();
      actionable_tabs.push_back(context);
      if (GetHealthLevelForMeasurement(CpuPercent(
              recent_measurement_percentage -
              total_actionable_cpu_percentage)) < current_health_status_) {
        take_action_improves_health = true;
        break;
      }
    }
  }

  // If health status can't change after taking action, then we should consider
  // all of the tabs as not actionable.
  if (!take_action_improves_health) {
    actionable_tabs = {};
  }

  actionable_tabs_ = actionable_tabs;
  std::move(callback).Run(actionable_tabs);
}

bool CpuHealthTracker::CanDiscardPage(
    resource_attribution::PageContext context) {
  PageNode* const page_node = context.GetPageNode();

  // Page is not discardable since the page no longer exists
  if (page_node == nullptr) {
    return false;
  }

  policies::PageDiscardingHelper* const discard_helper =
      policies::PageDiscardingHelper::GetFromGraph(GetOwningGraph());
  CHECK(discard_helper);

  // While in demo mode, we don't need to use the measurement_window when
  // determining tab actionability so we can immediately trigger the
  // intervention UI for testing purposes.
  const base::TimeDelta measurement_window =
      is_demo_mode_ ? base::TimeDelta() : features::kCPUTimeOverThreshold.Get();

  // We should not discard pages that played audio during the measurement window
  // as it may affect CPU measurements.
  const bool did_audio_status_change =
      page_node->GetTimeSinceLastAudibleChange().value_or(
          base::TimeDelta::Max()) < measurement_window;

  return !did_audio_status_change &&
         discard_helper->CanDiscard(
             page_node, ::mojom::LifecycleUnitDiscardReason::SUGGESTED,
             measurement_window) ==
             policies::PageDiscardingHelper::CanDiscardResult::kEligible;
}

bool CpuHealthTracker::RecordAndUpdateHealthStatus(CpuPercent measurement) {
  CHECK_EQ(recent_resource_measurements_.size(),
           cpu_health_sample_window_size_);

  // Remove the oldest health measurement and add the updated measurement
  const CpuPercent removed_measurement = recent_resource_measurements_.front();
  CHECK_GE(removed_measurement, min_resource_measurement_);
  recent_resource_measurements_.pop_front();
  recent_resource_measurements_.push_back(measurement);

  if (measurement <= min_resource_measurement_) {
    // Our newest measurement is the new smallest measurement
    min_resource_measurement_ = measurement;
  } else if (removed_measurement == min_resource_measurement_) {
    // Since we removed the minimum resource measurement from the dequeue, we
    // need to traverse through the queue again to find the next smallest
    // measurement
    min_resource_measurement_ =
        *std::min_element(recent_resource_measurements_.begin(),
                          recent_resource_measurements_.end());
  }

  const HealthLevel old_level = current_health_status_;
  const HealthLevel new_level =
      GetHealthLevelForMeasurement(min_resource_measurement_);
  current_health_status_ = new_level;
  return new_level != old_level;
}

void CpuHealthTracker::ProcessCpuProbeResult(
    std::optional<system_cpu::CpuSample> cpu_sample) {
  if (!cpu_sample.has_value()) {
    return;
  }

  const CpuPercent total_system_cpu_usage{cpu_sample.value().cpu_utilization *
                                          100};
  if (GetHealthLevelForMeasurement(total_system_cpu_usage) !=
      HealthLevel::kHealthy) {
    // Query for tab CPU usage to determine actionability
    QueryAndProcessTabActionability(total_system_cpu_usage);
    // We delay recording total_system_cpu_usage for not healthy CPU usage until
    // we get results from the query to ensure that the recorded CPU and
    // resulting health status stays consistent with tab actionability
  } else if (RecordAndUpdateHealthStatus(total_system_cpu_usage)) {
    // Notify observers that the health level became healthy.
    // We don't need to query for tab data because nothing needs to be
    // actionable when CPU is healthy.

    base::OnceCallback<void(CpuHealthTracker::ActionableTabsResult)>
        notify_healthy_status =
            GetStatusAndActionabilityCallback(true, HealthLevel::kHealthy);

    if (!actionable_tabs_.empty()) {
      actionable_tabs_ = {};
      tab_page_measurements_ = {};
    }

    std::move(notify_healthy_status).Run({});
  }
}

void CpuHealthTracker::ProcessQueryResultMap(
    CpuPercent system_cpu_usage_percentage,
    const resource_attribution::QueryResultMap& results) {
  const base::TimeTicks measurement_time = base::TimeTicks::Now();
  const bool did_status_change =
      RecordAndUpdateHealthStatus(system_cpu_usage_percentage);

  if (!page_cpu_proportion_tracker_.IsTracking()) {
    page_cpu_proportion_tracker_.StartFirstInterval(measurement_time, results);
  } else {
    // Determine cpu usage for each page context
    std::map<resource_attribution::ResourceContext, double> page_cpu =
        page_cpu_proportion_tracker_.StartNextInterval(measurement_time,
                                                       results);
    tab_page_measurements_ = FilterForPossibleActionablePages(page_cpu);

    GetFilteredActionableTabs(tab_page_measurements_,
                              system_cpu_usage_percentage,
                              GetStatusAndActionabilityCallback(
                                  did_status_change, current_health_status_));
  }
}

CpuHealthTracker::PageResourceMeasurements
CpuHealthTracker::FilterForPossibleActionablePages(
    std::map<resource_attribution::ResourceContext, double> page_cpu) {
  std::vector<std::pair<resource_attribution::PageContext, CpuPercent>>
      eligible_pages;
  for (const auto& [context, cpu_usage] : page_cpu) {
    resource_attribution::PageContext page_context =
        resource_attribution::AsContext<resource_attribution::PageContext>(
            context);
    PageNode* const page_node = page_context.GetPageNode();
    const bool is_tab = page_node && page_node->GetType() == PageType::kTab;

    const int cpu_usage_percentage =
        cpu_usage * 100 / base::SysInfo::NumberOfProcessors();
    if (is_tab && !page_node->IsOffTheRecord()) {
      eligible_pages.emplace_back(page_context, cpu_usage_percentage);
    }
  }

  return base::MakeFlatMap<resource_attribution::PageContext, CpuPercent>(
      std::move(eligible_pages));
}
}  // namespace performance_manager::user_tuning
