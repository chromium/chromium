// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_footprint_aggregator.h"

#include <limits>
#include <numeric>
#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

using PageId = TabFootprintAggregator::PageId;
using ukm::builders::Memory_TabFootprint;

namespace {

static const uint64_t kInvalidAmount = std::numeric_limits<uint64_t>::max();

// Unfortunately, there's no way to read attributes from |UkmEntryBuilderBase|
// instances so we can't build up counts in-place. We'll use |TabStats| for
// building the counts and copy the results to a |Memory_TabFootprint| to
// report them. See the |Memory_TabFootprint| event definition in ukm.xml for a
// description of each metric; they correspond to the getters of this class.
// Note that this class expects and reports values in terms of kilobytes while
// the ukm event uses megabytes.
class TabStats {
 public:
  uint64_t GetMainFramePmf() const { return main_frame_pmf_; }
  void SetMainFramePmf(uint64_t pmf_kb) { main_frame_pmf_ = pmf_kb; }

  uint64_t GetSubFramePmf() const { return sub_frame_pmf_; }
  void AddSubFramePmf(uint64_t pmf_kb) {
    sub_frame_pmf_ += pmf_kb;
    ++sub_frames_included_;
  }
  void IgnoreSubFrame() { ++sub_frames_excluded_; }

  uint64_t GetSubFramesIncluded() const { return sub_frames_included_; }
  uint64_t GetSubFramesExcluded() const { return sub_frames_excluded_; }

  uint64_t GetTabPmf() const {
    if (sub_frames_excluded_ != 0) {
      return kInvalidAmount;
    }
    if (main_frame_pmf_ == kInvalidAmount) {
      return kInvalidAmount;
    }

    return main_frame_pmf_ + sub_frame_pmf_;
  }

 private:
  uint64_t main_frame_pmf_ = kInvalidAmount;
  uint64_t sub_frame_pmf_ = 0u;
  uint64_t sub_frames_included_ = 0u;
  uint64_t sub_frames_excluded_ = 0u;
};

}  // namespace

TabFootprintAggregator::TabFootprintAggregator() = default;
TabFootprintAggregator::~TabFootprintAggregator() = default;

void TabFootprintAggregator::AssociateMainFrame(ukm::SourceId sid,
                                                base::ProcessId pid,
                                                PageId page_id,
                                                uint64_t pmf_kb) {
  bool did_insert =
      page_to_main_frame_process_.insert(std::make_pair(page_id, pid)).second;
  DCHECK(did_insert) << "there shouldn't be more than one main frame per page.";

  AssociateFrame(sid, pid, page_id, pmf_kb);
}

void TabFootprintAggregator::AssociateSubFrame(ukm::SourceId sid,
                                               base::ProcessId pid,
                                               PageId page_id,
                                               uint64_t pmf_kb) {
  AssociateFrame(sid, pid, page_id, pmf_kb);
}

void TabFootprintAggregator::AssociateFrame(ukm::SourceId sid,
                                            base::ProcessId pid,
                                            PageId page_id,
                                            uint64_t pmf_kb) {
  std::map<PageId, ukm::SourceId>::iterator insert_position;
  bool did_insert;
  std::tie(insert_position, did_insert) =
      page_to_source_id_.insert(std::make_pair(page_id, sid));
  // If there was already a |SourceId| associated to the |PageId|, make sure
  // it's the same |SourceId| as |sid|. This guards against attempts to
  // associate more than one top-level-navigation to a single tab.
  DCHECK(did_insert || insert_position->second == sid)
      << "Can't associate multiple SourceIds to a single PageId.";

  std::vector<PageId>& pages = process_to_pages_[pid];
  DCHECK(!std::count(pages.begin(), pages.end(), page_id))
      << "Can't duplicate associations between a process and a page.";
  pages.push_back(page_id);

  std::vector<base::ProcessId>& processes = page_to_processes_[page_id];
  DCHECK(!std::count(processes.begin(), processes.end(), pid))
      << "Can't duplicate associations between a page and a process.";
  processes.push_back(pid);

  process_to_pmf_.insert(std::make_pair(pid, pmf_kb));
}

void TabFootprintAggregator::RecordPmfs(ukm::UkmRecorder* ukm_recorder) const {
  // A map from page identifier (1:1 with tab) to a collection of stats for
  // that page's memory usage. Note that, if a component of a particular
  // TabStats::tab_pmf is invalid, the whole tab_pmf is invalid.
  std::map<PageId, TabStats> page_stats;

  for (const auto& page_procs : page_to_processes_) {
    PageId page_id = page_procs.first;

    TabStats& sink = page_stats[page_id];

    // Set |main_frame_process| to the id of the process that hosts the main
    // frame for |page_id|.
    base::ProcessId main_frame_process = base::kNullProcessId;
    auto page_to_main_frame_process_iterator =
        page_to_main_frame_process_.find(page_id);
    if (page_to_main_frame_process_iterator !=
        page_to_main_frame_process_.end()) {
      main_frame_process = page_to_main_frame_process_iterator->second;
    }

    for (const auto& proc : page_procs.second) {
      if (proc == main_frame_process) {
        // Determine if the process hosting the main frame for |page_id| is only
        // concerned with frames in that tab. If the process hosts frames from
        // any other tab, we can't use the MainFrameProcessPMF.
        const auto& all_tabs_for_proc = process_to_pages_.at(proc);
        if (all_tabs_for_proc.size() == 1) {
          DCHECK_EQ(sink.GetMainFramePmf(), kInvalidAmount)
              << "there can't be more than one process hosting a particular "
                 "frame.";
          sink.SetMainFramePmf(process_to_pmf_.at(proc));
        }
      } else {
        // The SubFrameProcessPMF is viable iff |proc| is associated to
        // |page_id| only.
        const auto& pages = process_to_pages_.at(proc);
        if (pages.size() == 1) {
          DCHECK_EQ(pages.front(), page_id);
          sink.AddSubFramePmf(process_to_pmf_.at(proc));
        } else {
          sink.IgnoreSubFrame();
        }
      }
    }
  }

  for (const auto& page_stat : page_stats) {
    // Note: fields in |Memory_TabFootprint| use MB while our accumulators use
    // KB.
    Memory_TabFootprint sink(page_to_source_id_.at(page_stat.first));

    // If MainFrameProcessPMF has been marked invalid it should be skipped.
    uint64_t main_frame_pmf = page_stat.second.GetMainFramePmf();
    if (main_frame_pmf != kInvalidAmount) {
      sink.SetMainFrameProcessPMF(main_frame_pmf / 1024);
    }

    sink.SetSubFrameProcessPMF_Total(page_stat.second.GetSubFramePmf() / 1024);
    sink.SetSubFrameProcessPMF_Included(
        page_stat.second.GetSubFramesIncluded());
    sink.SetSubFrameProcessPMF_Excluded(
        page_stat.second.GetSubFramesExcluded());

    // If TabPMF has been marked invalid it should be skipped.
    uint64_t tab_pmf = page_stat.second.GetTabPmf();
    if (tab_pmf != kInvalidAmount) {
      sink.SetTabPMF(tab_pmf / 1024);
    }

    sink.Record(ukm_recorder);
  }
}
