// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_footprint_aggregator.h"

#include <string_view>
#include <utility>

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using ukm::builders::Memory_TabFootprint;

namespace {

static const ukm::SourceId kSourceId1 = 1;
static const ukm::SourceId kSourceId2 = 2;

static const base::ProcessId kProcId1 = 3;
static const base::ProcessId kProcId2 = 4;
static const base::ProcessId kProcId3 = 5;

static const TabFootprintAggregator::PageId kPageId1 = 6;
static const TabFootprintAggregator::PageId kPageId2 = 7;

// For a given metric, a |ResultMap| encodes what value that metric should have
// on a per ukm::SourceId basis.
// Use a multi-map because we want to be able to test against multiple records
// being emitted for a single URL. This scenario resembles a user having
// multiple tabs open for the same top-level-navigation.
using ResultMap = std::multimap<ukm::SourceId, int64_t>;

}  // namespace

class TabFootprintAggregatorTest : public testing::Test {
 protected:
  // Walk through |mock_recorder_|'s UKM entries to collect |metric_name|
  // values.
  ResultMap CollectResults(std::string_view metric_name) const {
    ResultMap result;
    for (const ukm::mojom::UkmEntry* entry :
         mock_recorder_.GetEntriesByName(Memory_TabFootprint::kEntryName)) {
      const int64_t* metric_value =
          mock_recorder_.GetEntryMetric(entry, metric_name);

      if (metric_value == nullptr) {
        // Undefined attributes are signalled with a null pointer from
        // |GetEntryMetric|. Memory_TabFootprint events are supposed to leave
        // some attributes undefined in certain circumstances so we won't add
        // an entry to |result| in this case.
        continue;
      }

      result.insert(std::make_pair(entry->source_id, *metric_value));
    }
    return result;
  }

  ResultMap MainFrameResults() const {
    return CollectResults(Memory_TabFootprint::kMainFrameProcessPMFName);
  }

  ResultMap SubFrameTotalResults() const {
    return CollectResults(Memory_TabFootprint::kSubFrameProcessPMF_TotalName);
  }

  ResultMap SubFrameIncludedResults() const {
    return CollectResults(
        Memory_TabFootprint::kSubFrameProcessPMF_IncludedName);
  }

  ResultMap SubFrameExcludedResults() const {
    return CollectResults(
        Memory_TabFootprint::kSubFrameProcessPMF_ExcludedName);
  }

  ResultMap TabTotalResults() const {
    return CollectResults(Memory_TabFootprint::kTabPMFName);
  }

  ukm::TestUkmRecorder mock_recorder_;
};

TEST_F(TabFootprintAggregatorTest, TestEmpty) {
  TabFootprintAggregator empty;
  // All renderers were excluded from analysis.

  empty.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(0u, mock_recorder_.entries_count());

  // With no calls to |AssociateMainFrame| or |AssociateSubFrame| we expect no
  // UKM events to exist.
  EXPECT_EQ(ResultMap(), MainFrameResults());
  EXPECT_EQ(ResultMap(), SubFrameTotalResults());
  EXPECT_EQ(ResultMap(), SubFrameIncludedResults());
  EXPECT_EQ(ResultMap(), SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestSimple) {
  TabFootprintAggregator accumulator;
  // One page with a main frame renderer.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(1u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}}), MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}}), SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}}), SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}}), SubFrameExcludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 11}}), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestSubOnly) {
  TabFootprintAggregator accumulator;
  // One page with one sub-frame renderer.
  // This case shouldn't happen in practice right now (there's always a main
  // frame if there's a sub-frame) but the class under test supports this.
  accumulator.AssociateSubFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(1u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap(), MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 11}}), SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 1}}), SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}}), SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestMainWithSub) {
  TabFootprintAggregator accumulator;
  // One page with a main frame and a sub-frame that are co-hosted by one
  // renderer.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateSubFrame(kSourceId1, kProcId2, kPageId1, 22 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(1u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}}), MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 22}}), SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 1}}), SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}}), SubFrameExcludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 33}}), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestTwoPages) {
  TabFootprintAggregator accumulator;
  // Two pages with distinct URLs and their main frames have their own renderer
  // process.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId2, kProcId2, kPageId2, 22 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId2, 22}}),
            MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId2, 22}}), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestMainFrameOverload) {
  TabFootprintAggregator accumulator;
  // Two pages with distinct URLs but their main frames are co-hosted by one
  // renderer.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId2, kProcId1, kPageId2, 11 * 1024);

  // Note that we're emitting records with an undefined MainFrameProcessPMF on
  // purpose.
  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap(), MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestSharedSubframeRenderer) {
  TabFootprintAggregator accumulator;
  // Two pages with distinct URLs and their own main frame hosts but they each
  // have sub-frames that are co-hosted in a third render process.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId2, kProcId2, kPageId2, 22 * 1024);
  accumulator.AssociateSubFrame(kSourceId1, kProcId3, kPageId1, 33 * 1024);
  accumulator.AssociateSubFrame(kSourceId2, kProcId3, kPageId2, 33 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId2, 22}}),
            MainFrameResults());
  // Since the sub-frames are on separate pages but share a renderer process,
  // their contribution to SubFrameProcessPMF.Total is skipped.
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId2, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 1}, {kSourceId2, 1}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestCohostedDuplicateTabs) {
  TabFootprintAggregator accumulator;
  // Two pages with the same URL which share a render process.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId2, 11 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap(), MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestDuplicateMainframeTabs) {
  TabFootprintAggregator accumulator;
  // Two pages with the same URL but their own render process.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId1, kProcId2, kPageId2, 22 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId1, 22}}),
            MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId1, 22}}), TabTotalResults());
}

TEST_F(TabFootprintAggregatorTest, TestDuplicateTabsSharedSubframes) {
  TabFootprintAggregator accumulator;
  // Two pages with the same URL and their own main-frame render process but a
  // third render process with subframes from each of the pages.
  accumulator.AssociateMainFrame(kSourceId1, kProcId1, kPageId1, 11 * 1024);
  accumulator.AssociateMainFrame(kSourceId1, kProcId2, kPageId2, 22 * 1024);
  accumulator.AssociateSubFrame(kSourceId1, kProcId3, kPageId1, 33 * 1024);
  accumulator.AssociateSubFrame(kSourceId1, kProcId3, kPageId2, 33 * 1024);

  accumulator.RecordPmfs(&mock_recorder_);
  EXPECT_EQ(2u, mock_recorder_.entries_count());

  EXPECT_EQ(ResultMap({{kSourceId1, 11}, {kSourceId1, 22}}),
            MainFrameResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameTotalResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 0}, {kSourceId1, 0}}),
            SubFrameIncludedResults());
  EXPECT_EQ(ResultMap({{kSourceId1, 1}, {kSourceId1, 1}}),
            SubFrameExcludedResults());
  EXPECT_EQ(ResultMap(), TabTotalResults());
}
