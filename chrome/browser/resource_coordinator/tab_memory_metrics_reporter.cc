// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"

#include <cstdint>
#include <memory>

#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/process_memory_metrics_emitter.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

TabMemoryMetricsReporter::TabMemoryMetricsReporter() = default;

TabMemoryMetricsReporter::~TabMemoryMetricsReporter() = default;

TabMemoryMetricsReporter::TabMemoryMetricsReporter(
    const base::TickClock* tick_clock)
    : update_timer_(tick_clock) {}

void TabMemoryMetricsReporter::StartReporting(TabLoadTracker* tracker) {
  if (reporting_started_)
    return;
  tracker->AddObserver(this);
  reporting_started_ = true;
}

void TabMemoryMetricsReporter::OnStartTracking(
    content::WebContents* web_contents,
    TabLoadTracker::LoadingState loading_state) {
  if (loading_state != TabLoadTracker::LoadingState::LOADED)
    return;

  MonitorWebContents(web_contents);
}

void TabMemoryMetricsReporter::OnLoadingStateChange(
    content::WebContents* web_contents,
    TabLoadTracker::LoadingState old_loading_state,
    LoadingState new_loading_state) {
  if (new_loading_state != TabLoadTracker::LoadingState::LOADED)
    return;

  MonitorWebContents(web_contents);
}

void TabMemoryMetricsReporter::RemoveWebContentsDataFromMonitoredListIfExists(
    content::WebContents* web_contents) {
  for (auto it = monitored_contents_.begin(); it != monitored_contents_.end();
       ++it) {
    if (it->web_contents == web_contents) {
      monitored_contents_.erase(it);
      break;
    }
  }
}

void TabMemoryMetricsReporter::MonitorWebContents(
    content::WebContents* web_contents) {
  base::TimeTicks current_time = NowTicks();
  WebContentsData data;
  data.page_loaded_time = current_time;
  data.next_emit_time = data.page_loaded_time + base::Minutes(1);
  data.state = NO_METRICS_EMITTED;
  data.web_contents = web_contents;

  RemoveWebContentsDataFromMonitoredListIfExists(web_contents);
  monitored_contents_.insert(data);

  if (monitored_contents_.cbegin()->web_contents != web_contents)
    return;

  RestartTimerIfNeeded(current_time);
}

void TabMemoryMetricsReporter::OnStopTracking(
    content::WebContents* web_contents,
    TabLoadTracker::LoadingState loading_state) {
  bool should_update_timer =
      !monitored_contents_.empty() &&
      monitored_contents_.cbegin()->web_contents == web_contents;

  RemoveWebContentsDataFromMonitoredListIfExists(web_contents);
  if (!should_update_timer)
    return;

  RestartTimerIfNeeded(NowTicks());
}

void TabMemoryMetricsReporter::UpdateTimerCallback() {
  base::TimeTicks current_time = NowTicks();
  // A list of renderers whose memory dumps were emitted.
  std::forward_list<WebContentsData> renderers_memory_dumped;

  // Extract all WebContentsData whose next_emit_time have expired,
  // and emit metrics for them.
  auto it = monitored_contents_.begin();
  while (it != monitored_contents_.end() &&
         it->next_emit_time <= current_time) {
    if (EmitMemoryMetricsAfterPageLoaded(*it))
      renderers_memory_dumped.push_front(*it);
    it = monitored_contents_.erase(it);
  }

  // Advance the state of each item that just emitted metrics.
  // If they aren't done, put them back into the monitored list
  // with the time of their next event.
  while (!renderers_memory_dumped.empty()) {
    WebContentsData& data = renderers_memory_dumped.front();
    data.state = NextStateOfEmitMemoryDumpAfterPageLoaded(
        current_time - data.page_loaded_time);
    if (data.state < EMITTED_ALL_METRICS) {
      data.next_emit_time =
          data.page_loaded_time + NextEmitTimeAfterPageLoaded(data.state);
      DCHECK(data.next_emit_time > current_time);
      monitored_contents_.insert(data);
    }
    renderers_memory_dumped.pop_front();
  }

  RestartTimerIfNeeded(current_time);
}

void TabMemoryMetricsReporter::RestartTimerIfNeeded(
    base::TimeTicks current_time) {
  update_timer_.Stop();
  if (monitored_contents_.empty())
    return;

  base::TimeDelta timeout =
      monitored_contents_.cbegin()->next_emit_time - current_time;
  update_timer_.Start(FROM_HERE, timeout, this,
                      &TabMemoryMetricsReporter::UpdateTimerCallback);
}

bool TabMemoryMetricsReporter::EmitMemoryMetricsAfterPageLoaded(
    const TabMemoryMetricsReporter::WebContentsData& content_data) {
  content::RenderFrameHost* render_frame_host =
      content_data.web_contents->GetPrimaryMainFrame();
  if (!render_frame_host)
    return false;

  const base::Process& process = render_frame_host->GetProcess()->GetProcess();
  if (!process.IsValid())
    return false;
  // To record only this tab's process memory metrics, we will create
  // ProcessMemoryMetricsEmitter with pid.
  scoped_refptr<ProcessMemoryMetricsEmitter> emitter(
      new ProcessMemoryMetricsEmitter(process.Pid()));
  emitter->FetchAndEmitProcessMemoryMetrics();
  return true;
}

base::TimeDelta TabMemoryMetricsReporter::NextEmitTimeAfterPageLoaded(
    TabMemoryMetricsReporter::ReportState state) {
  static constexpr base::TimeDelta next_emit_time_after_page_loaded[] = {
      base::Minutes(1), base::Minutes(5), base::Minutes(10), base::Minutes(15)};
  DCHECK(NO_METRICS_EMITTED <= state && state < EMITTED_ALL_METRICS);
  return next_emit_time_after_page_loaded[state];
}

TabMemoryMetricsReporter::ReportState
TabMemoryMetricsReporter::NextStateOfEmitMemoryDumpAfterPageLoaded(
    base::TimeDelta time_passed) {
  if (time_passed >= base::Minutes(15))
    return EMITTED_ALL_METRICS;
  if (time_passed >= base::Minutes(10))
    return EMITTED_10MIN_METRIC;
  if (time_passed >= base::Minutes(5))
    return EMITTED_5MIN_METRIC;
  if (time_passed >= base::Minutes(1))
    return EMITTED_1MIN_METRIC;
  return NO_METRICS_EMITTED;
}

bool TabMemoryMetricsReporter::WebContentsDataComparator::operator()(
    const WebContentsData& a,
    const WebContentsData& b) const {
  return a.next_emit_time < b.next_emit_time;
}

}  // namespace resource_coordinator
