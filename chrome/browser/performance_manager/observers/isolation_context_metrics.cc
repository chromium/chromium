// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/isolation_context_metrics.h"

#include <cmath>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/performance_manager/public/graph/node_attached_data.h"

namespace performance_manager {

namespace {

// Given |data|, an instance of DataType that contains a |last_reported| field,
// calculates and returns the amount of time (in seconds) that has elapsed since
// the last report was filed, and updates the last report time to |now|.
template <typename DataType>
int GetSecondsSinceLastReportAndUpdate(const base::TimeTicks now,
                                       const base::TimeDelta reporting_interval,
                                       DataType* data) {
  auto elapsed = now - data->last_reported;
  data->last_reported = now;

  // It's entirely possible for time to advance by extremely large jumps if the
  // machine is put to sleep, for example. In this case, the data won't
  // meaningfully contribute to metrics. If the amount of time elapsed greatly
  // surpasses our reporting interval, silently drop the report by returning
  // that no time has elapsed.
  if (elapsed >= 2 * reporting_interval)
    return 0;

  return static_cast<int>(std::round(elapsed.InSecondsF()));
}

// Adds |count| samples of the given |value| to the linear histogram with the
// provided |name|. Assumes the enum starts at 0, and ends at
// EnumType::kMaxValue inclusively. This is templated on the histogram name so
// that each histogram gets a distinct static histogram pointer.
template <const char* kName, typename EnumType>
void AddCountsToHistogram(EnumType value, int count) {
  static constexpr int32_t kMaxValue =
      static_cast<int32_t>(EnumType::kMaxValue);
  STATIC_HISTOGRAM_POINTER_BLOCK(
      kName, AddCount(static_cast<int32_t>(value), count),
      base::LinearHistogram::FactoryGet(
          kName, 0, kMaxValue, kMaxValue + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

}  // namespace

// Wrapper around ProcessData providing storage. Keeps the impl details out of
// the header.
struct IsolationContextMetricsProcessDataImpl
    : public ExternalNodeAttachedDataImpl<
          IsolationContextMetricsProcessDataImpl> {
  explicit IsolationContextMetricsProcessDataImpl(
      const ProcessNode* process_node) {}
  ~IsolationContextMetricsProcessDataImpl() override = default;

  IsolationContextMetrics::ProcessData process_data;

  DISALLOW_COPY_AND_ASSIGN(IsolationContextMetricsProcessDataImpl);
};

// static
constexpr base::TimeDelta IsolationContextMetrics::kReportingInterval;

// static
const char IsolationContextMetrics::kProcessDataByTimeHistogramName[] =
    "PerformanceManager.FrameSiteInstanceProcessRelationship.ByTime2";

// static
const char IsolationContextMetrics::kProcessDataByProcessHistogramName[] =
    "PerformanceManager.FrameSiteInstanceProcessRelationship.ByProcess2";

// static
const char
    IsolationContextMetrics::kBrowsingInstanceDataByPageTimeHistogramName[] =
        "PerformanceManager.BrowsingInstancePluralityVisibilityState."
        "ByPageTime";

// static
const char IsolationContextMetrics::kBrowsingInstanceDataByTimeHistogramName[] =
    "PerformanceManager.BrowsingInstancePluralityVisibilityState.ByTime";

// static
const char IsolationContextMetrics::kFramesPerRendererByTimeHistogram[] =
    "PerformanceManager.FramesPerRendererByTime";

// static
const char IsolationContextMetrics::kSiteInstancesPerRendererByTimeHistogram[] =
    "PerformanceManager.SiteInstancesPerRendererByTime";

IsolationContextMetrics::IsolationContextMetrics() = default;

IsolationContextMetrics::~IsolationContextMetrics() {}

void IsolationContextMetrics::StartTimer() {
  // The timer cancels itself when it goes out of scope, so base::Unretained is
  // safe here.
  reporting_timer_.Start(
      FROM_HERE, kReportingInterval,
      base::BindRepeating(&IsolationContextMetrics::OnReportingTimerFired,
                          base::Unretained(this)));
}

void IsolationContextMetrics::OnFrameNodeAdded(const FrameNode* frame_node) {
  // Track frame node births and use that to keep ProcessData up to date.
  ChangeFrameCount(frame_node, 1);

  // This should be impossible, as frame nodes are created not current, and
  // are added to the graph before |IsCurrent| is set.
  DCHECK(!frame_node->IsCurrent());
}

void IsolationContextMetrics::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  // Track frame node deaths and use that to keep ProcessData up to date.
  ChangeFrameCount(frame_node, -1);

  // If the frame is the current main frame of a page, then remove the page
  // from the browsing instance as well.
  if (frame_node->IsMainFrame() && frame_node->IsCurrent()) {
    ChangePageCount(frame_node->GetPageNode(),
                    frame_node->GetBrowsingInstanceId(), -1);
  }
}

void IsolationContextMetrics::OnIsCurrentChanged(const FrameNode* frame_node) {
  if (!frame_node->IsMainFrame())
    return;

  const auto* page_node = frame_node->GetPageNode();
  DCHECK(page_node);
  const int32_t browsing_instance_id = frame_node->GetBrowsingInstanceId();

  const int delta = frame_node->IsCurrent() ? 1 : -1;
  ChangePageCount(page_node, browsing_instance_id, delta);
}

void IsolationContextMetrics::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  RegisterObservers(graph);
}

void IsolationContextMetrics::OnTakenFromGraph(Graph* graph) {
  UnregisterObservers(graph);
  graph_ = nullptr;
}

void IsolationContextMetrics::OnIsVisibleChanged(const PageNode* page_node) {
  // If there is no current main frame node associated with the page, we will
  // capture the visibility event when a node is added and made current via
  // "OnIsCurrentChanged".
  const auto* frame_node = page_node->GetMainFrameNode();
  if (!frame_node || !frame_node->IsCurrent())
    return;

  // Get the data related to this browsing instance. Since there is a current
  // main frame it must already have existed.
  DCHECK(base::Contains(browsing_instance_data_,
                        frame_node->GetBrowsingInstanceId()));
  auto* data = &browsing_instance_data_[frame_node->GetBrowsingInstanceId()];
  const BrowsingInstanceDataState old_state =
      GetBrowsingInstanceDataState(data);
  const int old_page_count = data->page_count;
  DCHECK_NE(BrowsingInstanceDataState::kUndefined, old_state);
  DCHECK_LT(0, data->page_count);

  if (page_node->IsVisible()) {
    ++data->visible_page_count;
    DCHECK_LE(data->visible_page_count, data->page_count);
  } else {
    DCHECK_LT(0, data->visible_page_count);
    --data->visible_page_count;
  }

  // Report the data if the state has changed.
  const BrowsingInstanceDataState new_state =
      GetBrowsingInstanceDataState(data);
  if (old_state != new_state) {
    // Report the state change. Flush all other related data so as not to
    // introduce bias into the metrics when only a partial reporting cycle
    // occurs.
    const auto now = base::TimeTicks::Now();
    ReportBrowsingInstanceData(data, old_page_count, old_state, now);
    ReportAllBrowsingInstanceData(now);
  }
}

void IsolationContextMetrics::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  // Track process death and use that to report whether or not the process
  // ever hosted frames from the same site instance. The ProcessData will only
  // exist for renderer processes that ever actually hosted frames.
  if (auto* process_data = ProcessData::Get(process_node)) {
    auto state = ProcessDataState::kOnlyOneFrameExists;
    if (process_data->has_hosted_multiple_frames) {
      state = process_data->has_hosted_multiple_frames_with_same_site_instance
                  ? ProcessDataState::kSomeFramesHaveSameSiteInstance
                  : ProcessDataState::kAllFramesHaveDistinctSiteInstances;
    }
    UMA_HISTOGRAM_ENUMERATION(kProcessDataByProcessHistogramName, state);
  }
}

void IsolationContextMetrics::RegisterObservers(Graph* graph) {
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
  StartTimer();
}

void IsolationContextMetrics::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);

  // Drain all metrics on shutdown to avoid losing the tail.
  reporting_timer_.Stop();
  OnReportingTimerFired();
}

// static
IsolationContextMetrics::ProcessDataState
IsolationContextMetrics::GetProcessDataState(const ProcessData* process_data) {
  if (process_data->site_instance_frame_count.empty())
    return ProcessDataState::kUndefined;

  if (process_data->multi_frame_site_instance_count == 0) {
    if (process_data->site_instance_frame_count.size() == 1)
      return ProcessDataState::kOnlyOneFrameExists;
    return ProcessDataState::kAllFramesHaveDistinctSiteInstances;
  }

  return ProcessDataState::kSomeFramesHaveSameSiteInstance;
}

// static
void IsolationContextMetrics::ReportProcessData(ProcessData* process_data,
                                                ProcessDataState state,
                                                base::TimeTicks now) {
  const int seconds =
      GetSecondsSinceLastReportAndUpdate(now, kReportingInterval, process_data);
  if (seconds) {
    AddCountsToHistogram<kProcessDataByTimeHistogramName>(state, seconds);

    // The following are effectively UMA_HISTOGRAM_COUNTS_100, but using
    // AddCount rather than Add.
    STATIC_HISTOGRAM_POINTER_BLOCK(
        kFramesPerRendererByTimeHistogram,
        AddCount(process_data->frame_count, seconds),
        base::Histogram::FactoryGet(
            kFramesPerRendererByTimeHistogram, 1, 100, 50,
            base::HistogramBase::kUmaTargetedHistogramFlag));

    STATIC_HISTOGRAM_POINTER_BLOCK(
        kSiteInstancesPerRendererByTimeHistogram,
        AddCount(process_data->site_instance_frame_count.size(), seconds),
        base::Histogram::FactoryGet(
            kSiteInstancesPerRendererByTimeHistogram, 1, 100, 50,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }
}

void IsolationContextMetrics::ReportAllProcessData(base::TimeTicks now) {
  for (const auto* process_node : graph_->GetAllProcessNodes()) {
    auto* process_data = ProcessData::Get(process_node);
    if (process_data)
      ReportProcessData(process_data, GetProcessDataState(process_data), now);
  }
}

void IsolationContextMetrics::ChangeFrameCount(const FrameNode* frame_node,
                                               int delta) {
  DCHECK(delta == -1 || delta == 1);
  const auto* process_node = frame_node->GetProcessNode();
  auto* data = ProcessData::GetOrCreate(process_node);
  const auto old_state = GetProcessDataState(data);

  if (delta == 1) {
    if (++data->frame_count > 1)
      data->has_hosted_multiple_frames = true;
  } else {
    --data->frame_count;
  }

  auto iter = data->site_instance_frame_count
                  .insert(std::make_pair(frame_node->GetSiteInstanceId(), 0))
                  .first;

  DCHECK_LE(0, iter->second);
  const int frame_count = iter->second += delta;
  DCHECK_LE(0, iter->second);

  if (delta == 1 && frame_count == 2) {
    ++data->multi_frame_site_instance_count;
    data->has_hosted_multiple_frames_with_same_site_instance = true;
  } else if (delta == -1 && frame_count == 1) {
    --data->multi_frame_site_instance_count;
  }

  if (frame_count == 0)
    data->site_instance_frame_count.erase(iter);

  const auto new_state = GetProcessDataState(data);

  if (old_state == ProcessDataState::kUndefined || old_state == new_state)
    return;

  // Report the state change. Flush all other related data so as not to
  // introduce bias into the metrics when only a partial reporting cycle occurs.
  const auto now = base::TimeTicks::Now();
  ReportProcessData(data, old_state, now);
  ReportAllProcessData(now);
}

// static
IsolationContextMetrics::BrowsingInstanceDataState
IsolationContextMetrics::GetBrowsingInstanceDataState(
    const BrowsingInstanceData* browsing_instance_data) {
  if (browsing_instance_data->page_count == 0)
    return BrowsingInstanceDataState::kUndefined;

  if (browsing_instance_data->page_count == 1) {
    if (browsing_instance_data->visible_page_count == 1)
      return BrowsingInstanceDataState::kSinglePageForeground;
    return BrowsingInstanceDataState::kSinglePageBackground;
  }

  if (browsing_instance_data->visible_page_count > 0)
    return BrowsingInstanceDataState::kMultiPageSomeForeground;
  return BrowsingInstanceDataState::kMultiPageBackground;
}

// static
void IsolationContextMetrics::ReportBrowsingInstanceData(
    BrowsingInstanceData* browsing_instance_data,
    int page_count,
    BrowsingInstanceDataState state,
    base::TimeTicks now) {
  const int seconds = GetSecondsSinceLastReportAndUpdate(
      now, kReportingInterval, browsing_instance_data);
  if (seconds) {
    DCHECK_LT(0, page_count);
    AddCountsToHistogram<kBrowsingInstanceDataByPageTimeHistogramName>(
        state, seconds * page_count);
    AddCountsToHistogram<kBrowsingInstanceDataByTimeHistogramName>(state,
                                                                   seconds);
  }
}

void IsolationContextMetrics::ReportAllBrowsingInstanceData(
    base::TimeTicks now) {
  for (auto& id_data_pair : browsing_instance_data_) {
    auto* data = &id_data_pair.second;
    ReportBrowsingInstanceData(data, data->page_count,
                               GetBrowsingInstanceDataState(data), now);
  }
}

void IsolationContextMetrics::ChangePageCount(const PageNode* page_node,
                                              int32_t browsing_instance_id,
                                              int delta) {
  DCHECK(delta == -1 || delta == 1);

  auto iter =
      browsing_instance_data_
          .insert(std::make_pair(browsing_instance_id, BrowsingInstanceData()))
          .first;
  auto* data = &iter->second;

  BrowsingInstanceDataState old_state = GetBrowsingInstanceDataState(data);
  int old_page_count = data->page_count;

  // Modify the page counts, checking invariants before and after.
  DCHECK_LE(0, data->page_count);
  DCHECK_LE(0, data->visible_page_count);
  DCHECK_LE(data->visible_page_count, data->page_count);
  data->page_count += delta;
  if (page_node->IsVisible())
    data->visible_page_count += delta;
  DCHECK_LE(0, data->page_count);
  DCHECK_LE(0, data->visible_page_count);
  DCHECK_LE(data->visible_page_count, data->page_count);

  BrowsingInstanceDataState new_state = GetBrowsingInstanceDataState(data);

  // No point reporting anything if this is newly created, or if the state
  // hasn't changed.
  if (old_state != BrowsingInstanceDataState::kUndefined &&
      old_state != new_state) {
    // Report the state change. Flush all other related data so as not to
    // introduce bias into the metrics when only a partial reporting cycle
    // occurs.
    const auto now = base::TimeTicks::Now();
    ReportBrowsingInstanceData(data, old_page_count, old_state, now);
    ReportAllBrowsingInstanceData(now);
  }

  // If the page count is down to zero then the browsing instance can be
  // erased. Note that |data| becomes an invalid pointer after this point.
  if (data->page_count == 0)
    browsing_instance_data_.erase(iter);
}

void IsolationContextMetrics::OnReportingTimerFired() {
  const auto now = base::TimeTicks::Now();
  ReportAllProcessData(now);
  ReportAllBrowsingInstanceData(now);
}

IsolationContextMetrics::ProcessData::ProcessData()
    : last_reported(base::TimeTicks::Now()) {}

IsolationContextMetrics::ProcessData::~ProcessData() = default;

// static
IsolationContextMetrics::ProcessData* IsolationContextMetrics::ProcessData::Get(
    const ProcessNode* process_node) {
  auto* impl = IsolationContextMetricsProcessDataImpl::Get(process_node);
  if (!impl)
    return nullptr;
  return &impl->process_data;
}

// static
IsolationContextMetrics::ProcessData*
IsolationContextMetrics::ProcessData::GetOrCreate(
    const ProcessNode* process_node) {
  return &IsolationContextMetricsProcessDataImpl::GetOrCreate(process_node)
              ->process_data;
}

IsolationContextMetrics::BrowsingInstanceData::BrowsingInstanceData()
    : last_reported(base::TimeTicks::Now()) {}

IsolationContextMetrics::BrowsingInstanceData::~BrowsingInstanceData() =
    default;

}  // namespace performance_manager
