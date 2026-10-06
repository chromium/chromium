// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_FOOTPRINT_AGGREGATOR_H_
#define CHROME_BROWSER_METRICS_TAB_FOOTPRINT_AGGREGATOR_H_

#include <map>
#include <vector>

#include "base/process/process_handle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ukm {
class UkmRecorder;
}

// Given information about which render processes are responsible for hosting
// the main- and sub-frames of a page instance, this class produces
// |Memory_TabFootprint| UKM records. |Memory_TabFootprint| records can be used
// to analyze and monitor the effective memory footprints that real world sites
// impose.
class TabFootprintAggregator {
 public:
  TabFootprintAggregator();
  ~TabFootprintAggregator();

  typedef uint64_t PageId;

  // Tracks the process identified by |pid| as the host of the main-frame for
  // the tab identified by |page_id|. |pmf_kb| should be the private memory
  // footprint of the process. |sid| should be the source id of the tab's
  // top-level navigation.
  void AssociateMainFrame(ukm::SourceId sid,
                          base::ProcessId pid,
                          PageId page_id,
                          uint64_t pmf_kb);

  // Tracks the process identified by |pid| as the host of one or more
  // sub-frames for the tab identified by |page_id|. |pmf_kb| should be the
  // private memory footprint of the process. |sid| should be the source id of
  // the tab's top-level navigation.
  void AssociateSubFrame(ukm::SourceId sid,
                         base::ProcessId pid,
                         PageId page_id,
                         uint64_t pmf_kb);

  // Serializes this aggregator's current state as a collection of
  // |Memory_TabFootprint| events which get written to the given recorder.
  void RecordPmfs(ukm::UkmRecorder* ukm_recorder) const;

 private:
  void AssociateFrame(ukm::SourceId sid,
                      base::ProcessId pid,
                      PageId page_id,
                      uint64_t pmf_kb);

  struct PageState {
    PageState();
    ~PageState();

    base::ProcessId main_frame_process = base::kNullProcessId;
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    std::vector<base::ProcessId> processes;
  };

  struct ProcessState {
    ProcessState();
    ~ProcessState();

    uint64_t pmf_kb = 0;
    std::vector<PageId> pages;
  };

  // Tracks per-tab main frame process, UKM SourceId, and hosting processes.
  std::map<PageId, PageState> pages_;

  // Tracks per-process private memory footprint (in KB) and hosted tabs.
  std::map<base::ProcessId, ProcessState> processes_;
};

#endif  // CHROME_BROWSER_METRICS_TAB_FOOTPRINT_AGGREGATOR_H_
