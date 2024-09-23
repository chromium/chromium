// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

char const* GetPermanentName(const std::string& name) {
  // A set of histogram names that provides the "permanent" lifetime required
  // by histogram objects for those strings that are not already code constants
  // or held in persistent memory.
  static base::NoDestructor<std::set<std::string>> permanent_names;

  auto result = permanent_names->insert(name);
  return result.first->c_str();
}

size_t GetBucketIndex(HistogramBase::Sample value, const BucketRanges* ranges) {
  size_t bucket_count = ranges->bucket_count();
  EXPECT_GE(bucket_count, 1U);
  for (size_t i = 0; i < bucket_count; ++i) {
    if (ranges->range(i) > value) {
      return i - 1;
    }
  }
  return bucket_count - 1;
}

// Runs a task in a thread that will emit |num_emission_| times the passed
// |histograms| and snapshot them. The thread will also keep track of the
// actual samples emitted, as well as the ones found in the snapshots taken, so
// that they can be compared.
class SnapshotDeltaThread : public SimpleThread {
 public:
  SnapshotDeltaThread(const std::string& name,
                      size_t num_emissions,
                      span<HistogramBase*> histograms,
                      HistogramBase::Sample histogram_max,
                      subtle::Atomic32* real_total_samples_count,
                      span<subtle::Atomic32> real_bucket_counts,
                      subtle::Atomic32* snapshots_total_samples_count,
                      span<subtle::Atomic32> snapshots_bucket_counts)
      : SimpleThread(name, Options()),
        num_emissions_(num_emissions),
        histograms_(histograms),
        histogram_max_(histogram_max),
        real_total_samples_count_(real_total_samples_count),
        real_bucket_counts_(real_bucket_counts),
        snapshots_total_samples_count_(snapshots_total_samples_count),
        snapshots_bucket_counts_(snapshots_bucket_counts) {}

  SnapshotDeltaThread(const SnapshotDeltaThread&) = delete;
  SnapshotDeltaThread& operator=(const SnapshotDeltaThread&) = delete;

  ~SnapshotDeltaThread() override = default;

  void Run() override {
    for (size_t i = 0; i < num_emissions_; ++i) {
      for (HistogramBase* histogram : histograms_) {
        // Emit a random sample. rand() is used here to generate such a sample,
        // but the randomness does not really matter as thread-safety is what is
        // being tested here and there is already a lot of non-determinism
        // surrounding scheduling.
        Histogram::Sample sample = rand() % histogram_max_;
        histogram->Add(sample);

        // Take a snapshot of the histogram. Because of the multithreading
        // nature of the test, this may or may not include the sample that was
        // just emitted, and/or may include samples that came from other
        // threads.
        std::unique_ptr<HistogramSamples> snapshot = histogram->SnapshotDelta();

        // Store the sample that was emitted as well as the snapshot so that
        // the totals can be compared later on.
        StoreActualSample(histogram, sample);
        StoreSnapshot(std::move(snapshot));
      }
    }
  }

 private:
  // Stores an actual |sample| that was emitted for |histogram|. This is done
  // to compare what was found in histogram snapshots (see StoreSnapshot()).
  void StoreActualSample(HistogramBase* histogram, Histogram::Sample sample) {
    subtle::NoBarrier_AtomicIncrement(real_total_samples_count_, 1);
    switch (histogram->GetHistogramType()) {
      case HISTOGRAM: {
        const BucketRanges* ranges =
            static_cast<Histogram*>(histogram)->bucket_ranges();
        size_t bucket_index = GetBucketIndex(sample, ranges);
        size_t bucket_min = ranges->range(bucket_index);
        subtle::NoBarrier_AtomicIncrement(&real_bucket_counts_[bucket_min], 1);
        break;
      }
      case SPARSE_HISTOGRAM:
        subtle::NoBarrier_AtomicIncrement(&real_bucket_counts_[sample], 1);
        break;
      case LINEAR_HISTOGRAM:
      case BOOLEAN_HISTOGRAM:
      case CUSTOM_HISTOGRAM:
      case DUMMY_HISTOGRAM:
        NOTREACHED();
    }
  }

  // Store a |snapshot| that was taken of a histogram. This is done to compare
  // what was actually emitted (see StoreActualSample()).
  void StoreSnapshot(std::unique_ptr<HistogramSamples> snapshot) {
    HistogramBase::Count snapshot_samples_count = snapshot->TotalCount();
    subtle::NoBarrier_AtomicIncrement(snapshots_total_samples_count_,
                                      snapshot_samples_count);
    for (auto it = snapshot->Iterator(); !it->Done(); it->Next()) {
      HistogramBase::Sample min;
      int64_t max;
      HistogramBase::Count count;
      it->Get(&min, &max, &count);
      // Verify that the snapshot contains only positive bucket counts.
      // This is to ensure SnapshotDelta() is fully thread-safe, not just
      // "eventually consistent".
      ASSERT_GE(count, 0);
      subtle::NoBarrier_AtomicIncrement(&snapshots_bucket_counts_[min], count);
    }
  }

  const size_t num_emissions_;
  raw_span<HistogramBase*> histograms_;
  const HistogramBase::Sample histogram_max_;
  raw_ptr<subtle::Atomic32> real_total_samples_count_;
  raw_span<subtle::Atomic32> real_bucket_counts_;
  raw_ptr<subtle::Atomic32> snapshots_total_samples_count_;
  raw_span<subtle::Atomic32> snapshots_bucket_counts_;
};

}  // namespace

class HistogramThreadsafeTest : public testing::Test {
 public:
  HistogramThreadsafeTest() = default;

  HistogramThreadsafeTest(const HistogramThreadsafeTest&) = delete;
  HistogramThreadsafeTest& operator=(const HistogramThreadsafeTest&) = delete;

  ~HistogramThreadsafeTest() override = default;

  void SetUp() override {
    GlobalHistogramAllocator::CreateWithLocalMemory(4 << 20, /*id=*/0,
                                                    /*name=*/"");
    ASSERT_TRUE(GlobalHistogramAllocator::Get());

    // Create a second view of the persistent memory with a new persistent
    // histogram allocator in order to simulate a subprocess with its own view
    // of some shared memory.
    PersistentMemoryAllocator* allocator =
        GlobalHistogramAllocator::Get()->memory_allocator();
    std::unique_ptr<PersistentMemoryAllocator> memory_view =
        std::make_unique<PersistentMemoryAllocator>(
            /*base=*/const_cast<void*>(allocator->data()), allocator->size(),
            /*page_size=*/0, /*id=*/0,
            /*name=*/"GlobalHistogramAllocatorView",
            PersistentMemoryAllocator::kReadWrite);
    allocator_view_ =
        std::make_unique<PersistentHistogramAllocator>(std::move(memory_view));
  }

  void TearDown() override {
    histograms_.clear();
    allocator_view_.reset();
    GlobalHistogramAllocator::ReleaseForTesting();
    ASSERT_FALSE(GlobalHistogramAllocator::Get());
  }

  // Creates and returns various histograms (some that live on the persistent
  // memory, some that live on the local heap, and some that point to the same
  // underlying data as those that live on the persistent memory but are
  // different objects).
  std::vector<HistogramBase*> CreateHistograms(size_t suffix,
                                               HistogramBase::Sample max,
                                               size_t bucket_count) {
    // There are 4 ways histograms can store their underlying data:
    // PersistentSampleVector, PersistentSampleMap, SampleVector, and SampleMap.
    // The first two are intended for when the data may be either persisted to a
    // file or shared with another process. The last two are when the histograms
    // are to be used by the local process only.
    // Create 4 histograms that use those storage structures respectively.
    std::vector<HistogramBase*> histograms;

    // Create histograms on the persistent memory (created through the
    // GlobalHistogramAllocator, which is automatically done when using the
    // FactoryGet() API). There is no need to store them in |histograms_|
    // because these histograms are owned by the StatisticsRecorder.
    std::string numeric_histogram_name =
        StringPrintf("NumericHistogram%zu", suffix);
    Histogram* numeric_histogram = static_cast<Histogram*>(
        Histogram::FactoryGet(numeric_histogram_name, /*minimum=*/1, max,
                              bucket_count, /*flags=*/HistogramBase::kNoFlags));
    histograms.push_back(numeric_histogram);
    std::string sparse_histogram_name =
        StringPrintf("SparseHistogram%zu", suffix);
    HistogramBase* sparse_histogram =
        SparseHistogram::FactoryGet(sparse_histogram_name,
                                    /*flags=*/HistogramBase::kNoFlags);
    histograms.push_back(sparse_histogram);

    // Create histograms on the "local heap" (i.e., are not instantiated using
    // the GlobalHistogramAllocator, which is automatically done when using the
    // FactoryGet() API). Store them in |histograms_| so that they are not freed
    // during the test.
    std::string local_heap_histogram_name =
        StringPrintf("LocalHeapNumericHistogram%zu", suffix);
    auto& local_heap_histogram = histograms_.emplace_back(
        new Histogram(GetPermanentName(local_heap_histogram_name),
                      numeric_histogram->bucket_ranges()));
    histograms.push_back(local_heap_histogram.get());
    std::string local_heap_sparse_histogram_name =
        StringPrintf("LocalHeapSparseHistogram%zu", suffix);
    auto& local_heap_sparse_histogram =
        histograms_.emplace_back(new SparseHistogram(
            GetPermanentName(local_heap_sparse_histogram_name)));
    histograms.push_back(local_heap_sparse_histogram.get());

    // Furthermore, create two additional *different* histogram objects that
    // point to the same underlying data as the first two (|numeric_histogram|
    // and |sparse_histogram|). This is to simulate subprocess histograms (i.e.,
    // both the main browser process and the subprocess have their own histogram
    // instance with possibly their own lock, but they both point to the same
    // underlying storage, and they may both interact with it simultaneously).
    // There is no need to do this for the "local heap" histograms because "by
    // definition" they should only be interacted with within the same process.
    PersistentHistogramAllocator::Iterator hist_it(allocator_view_.get());
    std::unique_ptr<HistogramBase> subprocess_numeric_histogram;
    std::unique_ptr<HistogramBase> subprocess_sparse_histogram;
    while (true) {
      // GetNext() creates a new histogram instance that points to the same
      // underlying data as the histogram the iterator is pointing to.
      std::unique_ptr<HistogramBase> histogram = hist_it.GetNext();
      if (!histogram) {
        break;
      }

      // Make sure the "local heap" histograms are not in persistent memory.
      EXPECT_NE(local_heap_histogram_name, histogram->histogram_name());
      EXPECT_NE(local_heap_sparse_histogram_name, histogram->histogram_name());

      if (histogram->histogram_name() == numeric_histogram_name) {
        subprocess_numeric_histogram = std::move(histogram);
      } else if (histogram->histogram_name() == sparse_histogram_name) {
        subprocess_sparse_histogram = std::move(histogram);
      }
    }
    // Make sure we found the histograms, and ensure that they are not the same
    // histogram objects. Assertions to verify that they are actually pointing
    // to the same underlying data are not done now (to not mess up the sample
    // counts).
    EXPECT_TRUE(subprocess_numeric_histogram);
    EXPECT_TRUE(subprocess_sparse_histogram);
    histograms.push_back(subprocess_numeric_histogram.get());
    histograms.push_back(subprocess_sparse_histogram.get());
    EXPECT_NE(numeric_histogram, subprocess_numeric_histogram.get());
    EXPECT_NE(sparse_histogram, subprocess_sparse_histogram.get());

    // Store the histograms in |histograms_| so that they are not freed during
    // the test.
    histograms_.emplace_back(std::move(subprocess_numeric_histogram));
    histograms_.emplace_back(std::move(subprocess_sparse_histogram));

    // Lastly, again, create two additional *different* histogram objects that
    // point to the same underlying data as the first two (|numeric_histogram|
    // and |sparse_histogram|). Unlike above, this is not necessarily done to
    // simulate subprocess histograms, but rather to verify that different
    // histogram objects created through the *same* allocator work correctly
    // together. In particular, the sparse histogram found here will use the
    // same "data manager" (see base::PersistentSparseHistogramDataManager) as
    // the original |sparse_histogram|. This is in contrast to the "subprocess"
    // histograms above, which will use a different "data manager" since those
    // histogram objects were created through a different allocator
    // (allocator_view_). In production, this is what happens when we try to
    // merge the histograms of a child process multiple times concurrently
    // (e.g. while we are merging the histograms of a certain child process in
    // the background, the browser is backgrounded, triggering another merge but
    // on the main thread).
    PersistentHistogramAllocator::Iterator hist_it2(
        GlobalHistogramAllocator::Get());
    std::unique_ptr<HistogramBase> numeric_histogram2;
    std::unique_ptr<HistogramBase> sparse_histogram2;
    while (true) {
      // GetNext() creates a new histogram instance that points to the same
      // underlying data as the histogram the iterator is pointing to.
      std::unique_ptr<HistogramBase> histogram = hist_it2.GetNext();
      if (!histogram) {
        break;
      }

      // Make sure the "local heap" histograms are not in persistent memory.
      EXPECT_NE(local_heap_histogram_name, histogram->histogram_name());
      EXPECT_NE(local_heap_sparse_histogram_name, histogram->histogram_name());

      if (histogram->histogram_name() == numeric_histogram_name) {
        numeric_histogram2 = std::move(histogram);
      } else if (histogram->histogram_name() == sparse_histogram_name) {
        sparse_histogram2 = std::move(histogram);
      }
    }
    // Make sure we found the histograms, and ensure that they are not the same
    // histogram objects. Assertions to verify that they are actually pointing
    // to the same underlying data are not done now (to not mess up the sample
    // counts).
    EXPECT_TRUE(numeric_histogram2);
    EXPECT_TRUE(sparse_histogram2);
    histograms.push_back(numeric_histogram2.get());
    histograms.push_back(sparse_histogram2.get());
    EXPECT_NE(numeric_histogram, numeric_histogram2.get());
    EXPECT_NE(sparse_histogram, sparse_histogram2.get());

    // Store the histograms in |histograms_| so that they are not freed during
    // the test.
    histograms_.emplace_back(std::move(numeric_histogram2));
    histograms_.emplace_back(std::move(sparse_histogram2));

    return histograms;
  }

 private:
  // A view of the GlobalHistogramAllocator to simulate a subprocess having its
  // own view of some shared memory.
  std::unique_ptr<PersistentHistogramAllocator> allocator_view_;

  // Used to prevent histograms from being freed during the test.
  std::vector<std::unique_ptr<HistogramBase>> histograms_;
};

// Verifies that SnapshotDelta() is thread safe. That means 1) a sample emitted
// while a snapshot is taken is not lost, and 2) concurrent calls to
// SnapshotDelta() will not return the same samples. Note that the test makes
// use of ASSERT_* instead EXPECT_* because the test is repeated multiple times,
// and the use of EXPECT_* produces spammy outputs as it does not end the test
// immediately.
TEST_F(HistogramThreadsafeTest, SnapshotDeltaThreadsafe) {
  // We try this test |kNumIterations| times to have a coverage of different
  // scenarios. For example, for a numeric histogram, if it has only samples
  // within the same bucket, the samples will be stored in a different way than
  // if it had samples in multiple buckets for efficiency reasons (SingleSample
  // vs a vector). Hence, the goal of doing this test multiple time is to have
  // coverage of the SingleSample scenario, because once the histogram has moved
  // to using a vector, it will not use SingleSample again.
  // Note: |kNumIterations| was 100 on 4/2023, but was decreased because the
  // workload was causing flakiness (timing out).
  constexpr size_t kNumIterations = 50;
  for (size_t iteration = 0; iteration < kNumIterations; ++iteration) {
    // TL;DR of the test: multiple threads are created, which will each emit to
    // the same histograms and snapshot their delta multiple times. We keep
    // track of the actual number of samples found in the snapshots, and ensure
    // that it matches what we actually emitted.

    // Create histograms. Two histograms should live on persistent memory,
    // two should live on local heap, and two of them should be simulations of
    // subprocess histograms that point to the same underlying data as first two
    // histograms (but are different objects).
    // The max values of the histograms will alternate between 2 and 50 in order
    // to have coverage of histograms that are being emitted to with a small
    // range of values, and a large range of values.
    const HistogramBase::Sample kHistogramMax = (iteration % 2 == 0) ? 2 : 50;
    const size_t kBucketCount = (iteration % 2 == 0) ? 3 : 10;
    std::vector<HistogramBase*> histograms =
        CreateHistograms(/*suffix=*/iteration, kHistogramMax, kBucketCount);

    // Start |kNumThreads| that will each emit and snapshot the histograms (see
    // SnapshotDeltaThread). We keep track of the real samples as well as the
    // samples found in the snapshots so that we can compare that they match
    // later on.
    constexpr size_t kNumThreads = 2;
    constexpr size_t kNumEmissions = 1000;
    subtle::Atomic32 real_total_samples_count = 0;
    std::vector<subtle::Atomic32> real_bucket_counts(kHistogramMax, 0);
    subtle::Atomic32 snapshots_total_samples_count = 0;
    std::vector<subtle::Atomic32> snapshots_bucket_counts(kHistogramMax, 0);
    std::unique_ptr<SnapshotDeltaThread> threads[kNumThreads];
    for (size_t i = 0; i < kNumThreads; ++i) {
      threads[i] = std::make_unique<SnapshotDeltaThread>(
          StringPrintf("SnapshotDeltaThread.%zu.%zu", iteration, i),
          kNumEmissions, histograms, kHistogramMax, &real_total_samples_count,
          real_bucket_counts, &snapshots_total_samples_count,
          snapshots_bucket_counts);
      threads[i]->Start();
    }

    // Wait until all threads have finished.
    for (auto& thread : threads) {
      thread->Join();
    }

    // Verify that the samples found in the snapshots match what we emitted.
    ASSERT_EQ(static_cast<size_t>(real_total_samples_count),
              kNumThreads * kNumEmissions * histograms.size());
    ASSERT_EQ(snapshots_total_samples_count, real_total_samples_count);
    for (HistogramBase::Sample i = 0; i < kHistogramMax; ++i) {
      ASSERT_EQ(snapshots_bucket_counts[i], real_bucket_counts[i]);
    }

    // Also verify that no more unlogged samples remain, and that the internal
    // logged samples of the histograms match what we emitted.

    HistogramBase::Count logged_total_samples_count = 0;
    std::vector<HistogramBase::Count> logged_bucket_counts(
        /*value=*/kHistogramMax, 0);
    // We ignore the last four histograms since they are the same as the first
    // two (they are simulations of histogram instances from a subprocess that
    // point to the same underlying data, and different histogram instances that
    // are created from the same allocator). Otherwise, we will be counting the
    // samples from those histograms thrice.
    for (size_t i = 0; i < histograms.size() - 4; ++i) {
      HistogramBase* histogram = histograms[i];
      ASSERT_EQ(histogram->SnapshotDelta()->TotalCount(), 0);
      std::unique_ptr<HistogramSamples> logged_samples =
          histogram->SnapshotSamples();
      // Each individual histograms should have been emitted to a specific
      // amount of times. Non-"local heap" histograms were emitted to thrice as
      // much because they appeared thrice in the |histograms| array -- once as
      // a normal histogram, once as a simulation of a subprocess histogram, and
      // once as a duplicate histogram created from the same allocator.
      size_t expected_logged_samples_count = kNumThreads * kNumEmissions;
      if (!strstr(histogram->histogram_name(), "LocalHeap")) {
        expected_logged_samples_count *= 3;
      }
      ASSERT_EQ(static_cast<size_t>(logged_samples->TotalCount()),
                expected_logged_samples_count);

      for (auto it = logged_samples->Iterator(); !it->Done(); it->Next()) {
        HistogramBase::Sample min;
        int64_t max;
        HistogramBase::Count count;
        it->Get(&min, &max, &count);
        ASSERT_GE(count, 0);
        logged_total_samples_count += count;
        logged_bucket_counts[min] += count;
      }
    }
    ASSERT_EQ(logged_total_samples_count, real_total_samples_count);
    for (HistogramBase::Sample i = 0; i < kHistogramMax; ++i) {
      ASSERT_EQ(logged_bucket_counts[i], real_bucket_counts[i]);
    }

    // Verify that our "subprocess histograms" actually point to the same
    // underlying data as the "main browser" histograms, despite being different
    // instances (this was verified earlier). This is done at the end of the
    // test so as to not mess up the sample counts.
    HistogramBase* numeric_histogram = histograms[0];
    HistogramBase* subprocess_numeric_histogram = histograms[4];
    HistogramBase* sparse_histogram = histograms[1];
    HistogramBase* subprocess_sparse_histogram = histograms[5];
    ASSERT_EQ(subprocess_numeric_histogram->SnapshotDelta()->TotalCount(), 0);
    ASSERT_EQ(subprocess_sparse_histogram->SnapshotDelta()->TotalCount(), 0);
    numeric_histogram->Add(0);
    sparse_histogram->Add(0);
    ASSERT_EQ(subprocess_numeric_histogram->SnapshotDelta()->TotalCount(), 1);
    ASSERT_EQ(subprocess_sparse_histogram->SnapshotDelta()->TotalCount(), 1);
    ASSERT_EQ(numeric_histogram->SnapshotDelta()->TotalCount(), 0);
    ASSERT_EQ(sparse_histogram->SnapshotDelta()->TotalCount(), 0);

    // Verify that our "duplicate histograms" created from the same allocator
    // actually point to the same underlying data as the "main" histograms,
    // despite being different instances (this was verified earlier). This is
    // done at the end of the test so as to not mess up the sample counts.
    HistogramBase* numeric_histogram2 = histograms[6];
    HistogramBase* sparse_histogram2 = histograms[7];
    ASSERT_EQ(numeric_histogram2->SnapshotDelta()->TotalCount(), 0);
    ASSERT_EQ(sparse_histogram2->SnapshotDelta()->TotalCount(), 0);
    numeric_histogram->Add(0);
    sparse_histogram->Add(0);
    ASSERT_EQ(numeric_histogram2->SnapshotDelta()->TotalCount(), 1);
    ASSERT_EQ(sparse_histogram2->SnapshotDelta()->TotalCount(), 1);
    ASSERT_EQ(numeric_histogram->SnapshotDelta()->TotalCount(), 0);
    ASSERT_EQ(sparse_histogram->SnapshotDelta()->TotalCount(), 0);
  }
}

}  // namespace base
