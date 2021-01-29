// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

PowerMetricsReporter::PowerMetricsReporter(
    const base::WeakPtr<UsageScenarioDataStore>& data_store)
    : data_store_(data_store) {
  DCHECK(performance_monitor::ProcessMonitor::Get());
  performance_monitor::ProcessMonitor::Get()->AddObserver(this);
}

PowerMetricsReporter::~PowerMetricsReporter() {
  if (auto* process_monitor = performance_monitor::ProcessMonitor::Get()) {
    process_monitor->RemoveObserver(this);
  }
}

void PowerMetricsReporter::OnAggregatedMetricsSampled(
    const performance_monitor::ProcessMonitor::Metrics& metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto now = base::TimeTicks::Now();
  ReportUKMs(metrics, now - interval_begin_);
  interval_begin_ = now;
}

void PowerMetricsReporter::ReportUKMs(
    const performance_monitor::ProcessMonitor::Metrics& metrics,
    base::TimeDelta interval_duration) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_store_.MaybeValid());

  // UKM may be unavailable in content_shell or other non-chrome/ builds; it
  // may also be unavailable if browser shutdown has started; so this may be a
  // nullptr. If it's unavailable, UKM reporting will be skipped.
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  auto usage_metrics = data_store_->ResetIntervalData();
  auto source_id = usage_metrics.source_id_for_longest_visible_origin;

  // TODO(sebmarchand): Figure out if we need to report data when we don't have
  // a valid sourceID.
  if (source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::PowerUsageScenariosIntervalData builder(source_id);
  builder.SetURLVisibilityTimeSeconds(ukm::GetExponentialBucketMinForUserTiming(
      usage_metrics.source_id_for_longest_visible_origin_duration.InSeconds()));
  builder.SetIntervalDurationSeconds(interval_duration.InSeconds());
  builder.SetUptimeSeconds(ukm::GetExponentialBucketMinForUserTiming(
      usage_metrics.uptime_at_interval_end.InSeconds()));
  // TODO(sebmarchand): Record the battery discharge rate.
  builder.SetBatteryDischargeRate(0);
  builder.SetCPUTimeMs(metrics.cpu_usage * interval_duration.InMilliseconds());
#if defined(OS_MAC)
  builder.SetIdleWakeUps(metrics.idle_wakeups);
  builder.SetPackageExits(metrics.package_idle_wakeups);
  builder.SetEnergyImpactScore(metrics.energy_impact);
#endif
  builder.SetMaxTabCount(
      ukm::GetExponentialBucketMinForCounts1000(usage_metrics.max_tab_count));
  // The number of windows is usually relatively low, use a small bucket
  // spacing.
  builder.SetMaxVisibleWindowCount(ukm::GetExponentialBucketMin(
      usage_metrics.max_visible_window_count, 1.05));
  builder.SetTabClosed(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.tabs_closed_during_interval));
  builder.SetTopLevelNavigationEvents(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.top_level_navigation_count));
  builder.SetUserInteractionCount(ukm::GetExponentialBucketMinForCounts1000(
      usage_metrics.user_interaction_count));
  builder.SetFullscreenVideoSingleMonitorSeconds(
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_playing_video_full_screen_single_monitor
              .InSeconds()));
  builder.SetTimeWithOpenWebRTCConnectionSeconds(
      ukm::GetExponentialBucketMinForUserTiming(
          usage_metrics.time_with_open_webrtc_connection.InSeconds()));

  builder.Record(ukm_recorder);
}
