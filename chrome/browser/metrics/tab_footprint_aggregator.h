// Copyright 2018 The Chromium Authors. All rights reserved.
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

 private:
  // For a given tab, this tracks what renderer process hosts the main frame.
  std::map<PageId, base::ProcessId> page_to_main_frame_process_;

  // This tracks the tabs who have a frame hosted by a given process.
  std::map<base::ProcessId, std::vector<PageId>> process_to_pages_;

  // For a given tab, this tracks which processes host some frame in the tab.
  std::map<PageId, std::vector<base::ProcessId>> page_to_processes_;

  // Tracks the pmf (in kilobytes) of a given process.
  std::map<base::ProcessId, uint64_t> process_to_pmf_;

  // Tracks the main frame's |ukm::SourceId| for a given tab. Conceptually,
  // distinct |ukm::SourceId|s correspond to distinct URLs. Note that, although
  // multiple tabs can navigate to the same top-level URL, an individual tab
  // can only be at a single URL at a time.
  std::map<PageId, ukm::SourceId> page_to_source_id_;
};

#endif  // CHROME_BROWSER_METRICS_TAB_FOOTPRINT_AGGREGATOR_H_
