// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/subprocess_metrics_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

namespace {

const uint32_t TEST_MEMORY_SIZE = 64 << 10;  // 64 KiB

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() {}

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    recorded_delta_histogram_names_.push_back(histogram.histogram_name());
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(HistogramFlattenerDeltaRecorder);
};

}  // namespace

class SubprocessMetricsProviderTest : public testing::Test {
 protected:
  SubprocessMetricsProviderTest() {
    // MergeHistogramDeltas needs to be called beause it uses a histogram
    // macro which caches a pointer to a histogram. If not done before setting
    // a persistent global allocator, then it would point into memory that
    // will go away.
    provider_.MergeHistogramDeltas();

    // Create a dedicated StatisticsRecorder for this test.
    test_recorder_ = base::StatisticsRecorder::CreateTemporaryForTesting();

    // Create a global allocator using a block of memory from the heap.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(TEST_MEMORY_SIZE,
                                                          0, "");
  }

  ~SubprocessMetricsProviderTest() override {
    base::GlobalHistogramAllocator::ReleaseForTesting();
  }

  SubprocessMetricsProvider* provider() { return &provider_; }

  std::unique_ptr<base::PersistentHistogramAllocator> CreateDuplicateAllocator(
      base::PersistentHistogramAllocator* allocator) {
    // Just wrap around the data segment in-use by the passed allocator.
    return std::make_unique<base::PersistentHistogramAllocator>(
        std::make_unique<base::PersistentMemoryAllocator>(
            const_cast<void*>(allocator->data()), allocator->length(), 0, 0,
            std::string(), false));
  }

  std::vector<std::string> GetSnapshotHistogramNames() {
    // Merge the data from the allocator into the StatisticsRecorder.
    provider_.MergeHistogramDeltas();

    // Flatten what is known to see what has changed since the last time.
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    // "true" to the begin() includes histograms held in persistent storage.
    base::StatisticsRecorder::PrepareDeltas(true, base::Histogram::kNoFlags,
                                            base::Histogram::kNoFlags,
                                            &snapshot_manager);
    return flattener.GetRecordedDeltaHistogramNames();
  }

  void EnableRecording() { provider_.OnRecordingEnabled(); }
  void DisableRecording() { provider_.OnRecordingDisabled(); }

  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator) {
    provider_.RegisterSubprocessAllocator(id, std::move(allocator));
  }

  void DeregisterSubprocessAllocator(int id) {
    provider_.DeregisterSubprocessAllocator(id);
  }

 private:
  // A thread-bundle makes the tests appear on the UI thread, something that is
  // checked in methods called from the SubprocessMetricsProvider class under
  // test. This must be constructed before the |provider_| field.
  content::BrowserTaskEnvironment task_environment_;

  SubprocessMetricsProvider provider_;
  std::unique_ptr<base::StatisticsRecorder> test_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SubprocessMetricsProviderTest);
};

TEST_F(SubprocessMetricsProviderTest, SnapshotMetrics) {
  base::HistogramBase* foo = base::Histogram::FactoryGet("foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("bar", 1, 100, 10, 0);
  base::HistogramBase* baz = base::Histogram::FactoryGet("baz", 1, 100, 10, 0);
  foo->Add(42);
  bar->Add(84);

  // Detach the global allocator but keep it around until this method exits
  // so that the memory holding histogram data doesn't get released. Register
  // a new allocator that duplicates the global one.
  std::unique_ptr<base::GlobalHistogramAllocator> global_allocator(
      base::GlobalHistogramAllocator::ReleaseForTesting());
  RegisterSubprocessAllocator(123,
                              CreateDuplicateAllocator(global_allocator.get()));

  // Recording should find the two histograms created in persistent memory.
  EXPECT_THAT(GetSnapshotHistogramNames(), UnorderedElementsAre("foo", "bar"));

  // A second run should have nothing to produce.
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());

  // Create a new histogram and update existing ones. Should now report 3 items.
  baz->Add(1969);
  foo->Add(10);
  bar->Add(20);
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("foo", "bar", "baz"));

  // Ensure that deregistering does a final merge of the data.
  foo->Add(10);
  bar->Add(20);
  DeregisterSubprocessAllocator(123);
  EXPECT_THAT(GetSnapshotHistogramNames(), UnorderedElementsAre("foo", "bar"));

  // Further snapshots should be empty even if things have changed.
  foo->Add(10);
  bar->Add(20);
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
}
