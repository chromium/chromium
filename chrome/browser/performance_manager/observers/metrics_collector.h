// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_

#include <map>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/observers/background_metrics_reporter.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

extern const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[];
extern const char kTabFromBackgroundedToFirstTitleUpdatedUMA[];
extern const char
    kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[];
extern const base::TimeDelta kMetricsReportDelayTimeout;
extern const int kDefaultFrequencyUkmEQTReported;

// The MetricsCollector is a graph observer that reports UMA/UKM.
class MetricsCollector : public FrameNode::ObserverDefaultImpl,
                         public GraphOwned,
                         public PageNode::ObserverDefaultImpl,
                         public ProcessNode::ObserverDefaultImpl {
 public:
  MetricsCollector();
  ~MetricsCollector() override;

  // FrameNodeObserver implementation:
  void OnNonPersistentNotificationCreated(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnUkmSourceIdChanged(const PageNode* page_node) override;
  void OnFaviconUpdated(const PageNode* page_node) override;
  void OnTitleUpdated(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnExpectedTaskQueueingDurationSample(
      const ProcessNode* process_node) override;

 protected:
  friend class MetricsReportRecordHolder;
  friend class UkmCollectionStateHolder;

  struct MetricsReportRecord {
    MetricsReportRecord();
    MetricsReportRecord(const MetricsReportRecord& other);
    void UpdateUkmSourceID(ukm::SourceId ukm_source_id);
    void Reset();
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstFaviconUpdated,
        kTabFromBackgroundedToFirstFaviconUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_favicon_updated;
    BackgroundMetricsReporter<
        ukm::builders::
            TabManager_Background_FirstNonPersistentNotificationCreated,
        kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA,
        internal::UKMFrameReportType::kMainFrameAndChildFrame>
        first_non_persistent_notification_created;
    BackgroundMetricsReporter<
        ukm::builders::TabManager_Background_FirstTitleUpdated,
        kTabFromBackgroundedToFirstTitleUpdatedUMA,
        internal::UKMFrameReportType::kMainFrameOnly>
        first_title_updated;
  };

  struct UkmCollectionState {
    int num_unreported_eqt_measurements = 0u;
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  };

 private:
  static MetricsReportRecord* GetMetricsReportRecord(const PageNode* page_node);
  static UkmCollectionState* GetUkmCollectionState(const PageNode* page_node);

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  bool ShouldReportMetrics(const PageNode* page_node);
  bool IsCollectingExpectedQueueingTimeForUkm(const PageNode* page_node);
  void RecordExpectedQueueingTimeForUkm(
      const PageNode* page_node,
      const base::TimeDelta& expected_queueing_time);
  void UpdateUkmSourceIdForPage(const PageNode* page_node,
                                ukm::SourceId ukm_source_id);
  void UpdateWithFieldTrialParams();
  void ResetMetricsReportRecord(const PageNode* page_nod);

  // The graph to which this object belongs.
  Graph* graph_ = nullptr;

  // The number of reports to wait before reporting ExpectedQueueingTime. For
  // example, if |frequency_ukm_eqt_reported_| is 2, then the first value is not
  // reported, the second one is, the third one isn't, etc.
  int frequency_ukm_eqt_reported_;
  DISALLOW_COPY_AND_ASSIGN(MetricsCollector);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_METRICS_COLLECTOR_H_
