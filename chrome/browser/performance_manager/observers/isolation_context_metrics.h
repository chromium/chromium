// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_ISOLATION_CONTEXT_METRICS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_ISOLATION_CONTEXT_METRICS_H_

#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

// A graph observer that tracks various metrics related to the isolation context
// of frames and pages:
//
// (1) How common it is for frames to be hosted in a same process that don't
//     actually need to be hosted together (they are not in the same site
//     instance and thus can't synchronously script each other). This is to
//     inform value of work on Blink Isolates.
// (2) How common it is for pages to be in browsing instances with other pages,
//     as opposed to in browsing instances on their own. This is for estimating
//     the impact of extending freezing logic to entire browsing instances.
class IsolationContextMetrics : public FrameNode::ObserverDefaultImpl,
                                public GraphOwned,
                                public PageNode::ObserverDefaultImpl,
                                public ProcessNode::ObserverDefaultImpl {
 public:
  IsolationContextMetrics();
  ~IsolationContextMetrics() override;

  // Starts the timer for periodic reporting.
  void StartTimer();

 protected:
  // Helper struct that implements storage for ProcessData.
  friend struct IsolationContextMetricsProcessDataImpl;

  // Periodic reporting interval. This would ideally be triggered by UMA
  // collection and not on its own timer, but UMA collection lives on the UI
  // thread. This wants to be something on the same order of magnitude as UMA
  // collection but not so fast as to cause pointless wakeups.
  static constexpr base::TimeDelta kReportingInterval =
      base::TimeDelta::FromMinutes(5);

  // This histogram records the cumulative amount of time a process spends
  // hosting only frames from distinct site instances, versus hosting more than
  // one frame from the same site instance. See ProcessDataState for details.
  static const char kProcessDataByTimeHistogramName[];
  // This histogram records the number of processes that ever only host frames
  // from distinct site instances, versus those that ever host more than one
  // frame from the same site instance. See ProcessDataState for details.
  static const char kProcessDataByProcessHistogramName[];
  // This histogram tallies the cumulative amount of time pages spent in the
  // foreground versus background, and as sole occupants of a browsing instance
  // versus in a shared browsing instance. See BrowsingInstanceDataState for
  // more details.
  static const char kBrowsingInstanceDataByPageTimeHistogramName[];
  // This histogram tallies the cumulative amount of time browsing instances
  // spend in the foreground versus background, and as browsing instances with
  // only one page versus multi-page browsing instances. See
  // BrowsingInstanceDataState for more details.
  static const char kBrowsingInstanceDataByTimeHistogramName[];
  // This histogram records the number of frames in a renderer over time.
  static const char kFramesPerRendererByTimeHistogram[];
  // This histogram records the number of site instances in a renderer over
  // time.
  static const char kSiteInstancesPerRendererByTimeHistogram[];

  // Tracks the number of distinct site instances being hosted per process.
  struct ProcessData {
    ProcessData();
    ~ProcessData();

    // Factories/accessors for node attached data.
    static ProcessData* Get(const ProcessNode* process_node);
    static ProcessData* GetOrCreate(const ProcessNode* process_node);

    // A map between site instance ID and the number of frames with that site
    // instance in the process. This is typically small for most processes, but
    // can go to O(100s) for power users hence the use of small_map.
    base::small_map<std::unordered_map<int32_t, int>> site_instance_frame_count;
    // The number of frames in this process.
    int frame_count = 0;
    // The number of site instances with multiple frames in this process.
    // Basically, this counts the number of entries in
    // |site_instance_frame_count| that are > 1.
    int multi_frame_site_instance_count = 0;
    // Whether or not this process has *ever* hosted multiple frames.
    bool has_hosted_multiple_frames = false;
    // Whether or not this process has *ever* hosted multiple frames in the same
    // site instance. This goes to true if |multi_frame_site_instance_count| is
    // ever greater than 0.
    bool has_hosted_multiple_frames_with_same_site_instance = false;
    // The last time data related to this process was reported to the histogram.
    // This happens on a timer or on state changes. This is initialized to the
    // time of the struct creation.
    base::TimeTicks last_reported;
  };

  // A state that can be calculated from a ProcessData.
  enum class ProcessDataState {
    kUndefined = -1,  // This value is never reported, but used in logic.
    kAllFramesHaveDistinctSiteInstances = 0,
    kSomeFramesHaveSameSiteInstance = 1,
    kOnlyOneFrameExists = 2,
    // Must be maintained as the max value.
    kMaxValue = kOnlyOneFrameExists
  };

  // Tracks summary information regarding pages in a browsing instance.
  struct BrowsingInstanceData {
    BrowsingInstanceData();
    ~BrowsingInstanceData();

    // The number of pages in this browsing instance.
    int page_count = 0;
    // The number of visible pages in this browsing instance. This is always
    // <= |page_count|.
    int visible_page_count = 0;
    // The last time the data related to this browsing instance was reported to
    // the histogram. This happens on a timer or on state changes. This is
    // initialized to the time of the struct creation.
    base::TimeTicks last_reported;
  };

  // A state that can be calculated from a BrowsingInstanceData. The
  // non-negative values are used in a histogram, so should not be modified.
  enum class BrowsingInstanceDataState {
    kUndefined = -1,  // This value is never reported, but used in logic.
    kSinglePageForeground = 0,
    kSinglePageBackground = 1,
    kMultiPageSomeForeground = 2,     // At least one page is foreground.
    kMultiPageBackground = 3,         // All pages are background.
    kMaxValue = kMultiPageBackground  // Must be maintained as the max value.
  };

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsCurrentChanged(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // Returns the state associated with a ProcessData.
  static ProcessDataState GetProcessDataState(const ProcessData* process_data);

  // Reports data associated with a ProcessData.
  static void ReportProcessData(ProcessData* process_data,
                                ProcessDataState state,
                                base::TimeTicks now);

  // Reports all process data.
  void ReportAllProcessData(base::TimeTicks now);

  // Adds or removes a frame from the relevant process data. The |delta| should
  // be +/- 1.
  void ChangeFrameCount(const FrameNode* frame_node, int delta);

  // Returns the state associated with a BrowsingInstanceData.
  static BrowsingInstanceDataState GetBrowsingInstanceDataState(
      const BrowsingInstanceData* browsing_instance_data);

  // Reports the data associated with a browsing instance.
  static void ReportBrowsingInstanceData(
      BrowsingInstanceData* browsing_instance_data,
      int page_count,
      BrowsingInstanceDataState state,
      base::TimeTicks now);

  // Reports all current browsing instance data.
  void ReportAllBrowsingInstanceData(base::TimeTicks now);

  // Updates |browsing_instance_data_| as pages are added and removed from
  // a browsing instance. The |delta| must be +/- 1.
  void ChangePageCount(const PageNode* page_node,
                       int32_t browsing_instance_id,
                       int delta);
  void OnPageAddedToBrowsingInstance(const PageNode* page_node,
                                     int32_t browsing_instance_id);
  void OnPageRemovedFromBrowsingInstance(const PageNode* page_node,
                                         int32_t browsing_instance_id);

  // This is virtual in order to provide a testing seam.
  virtual void OnReportingTimerFired();

  // The graph to which this object belongs.
  Graph* graph_ = nullptr;

  // Timer that is used to periodically flush metrics. This ensures that they
  // are mostly up to date in the event of a catastrophic browser crash. We
  // could do this on the same schedule as UMA itself by being a MetricsProvider
  // but that is UI thread bound, and would require all sorts of hassles for us
  // to build.
  // TODO(chrisha): Migrate away if metrics team provides a convenient API.
  // https://crbug.com/961468
  base::RepeatingTimer reporting_timer_;

  // Tracks data related to all currently known browsing instances. 90% of users
  // don't have more than 10 tabs open according to Tabs.MaxTabsInADay.
  base::small_map<std::unordered_map<int32_t, BrowsingInstanceData>, 10>
      browsing_instance_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolationContextMetrics);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_ISOLATION_CONTEXT_METRICS_H_
