// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/metrics/persistent_histogram_allocator.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class PersistentHistogramAllocatorTest : public testing::Test {
 public:
  PersistentHistogramAllocatorTest(const PersistentHistogramAllocatorTest&) =
      delete;
  PersistentHistogramAllocatorTest& operator=(
      const PersistentHistogramAllocatorTest&) = delete;

 protected:
  constexpr static int32_t kAllocatorMemorySize = 64 << 10;  // 64 KiB

  PersistentHistogramAllocatorTest()
      : statistics_recorder_(StatisticsRecorder::CreateTemporaryForTesting()) {
    CreatePersistentHistogramAllocator();
  }
  ~PersistentHistogramAllocatorTest() override {
    DestroyPersistentHistogramAllocator();
  }

  void CreatePersistentHistogramAllocator() {
    // GlobalHistogramAllocator is never deleted, hence intentionally leak
    // allocated memory in this test.
    allocator_memory_ = new char[kAllocatorMemorySize];
    ANNOTATE_LEAKING_OBJECT_PTR(allocator_memory_);

    GlobalHistogramAllocator::ReleaseForTesting();
    memset(allocator_memory_, 0, kAllocatorMemorySize);
    GlobalHistogramAllocator::CreateWithPersistentMemory(
        allocator_memory_, kAllocatorMemorySize, 0, 0,
        "PersistentHistogramAllocatorTest");
    allocator_ = GlobalHistogramAllocator::Get()->memory_allocator();
  }

  void DestroyPersistentHistogramAllocator() {
    allocator_ = nullptr;
    GlobalHistogramAllocator::ReleaseForTesting();
  }

  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
  raw_ptr<char> allocator_memory_ = nullptr;
  raw_ptr<PersistentMemoryAllocator> allocator_ = nullptr;
};

TEST_F(PersistentHistogramAllocatorTest, CreateAndIterate) {
  PersistentMemoryAllocator::MemoryInfo meminfo0;
  allocator_->GetMemoryInfo(&meminfo0);

  // Try basic construction
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kIsPersistent);
  EXPECT_TRUE(histogram);
  histogram->CheckName("TestHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo1;
  allocator_->GetMemoryInfo(&meminfo1);
  EXPECT_GT(meminfo0.free, meminfo1.free);

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, HistogramBase::kIsPersistent);
  EXPECT_TRUE(linear_histogram);
  linear_histogram->CheckName("TestLinearHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo2;
  allocator_->GetMemoryInfo(&meminfo2);
  EXPECT_GT(meminfo1.free, meminfo2.free);

  HistogramBase* boolean_histogram = BooleanHistogram::FactoryGet(
      "TestBooleanHistogram", HistogramBase::kIsPersistent);
  EXPECT_TRUE(boolean_histogram);
  boolean_histogram->CheckName("TestBooleanHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo3;
  allocator_->GetMemoryInfo(&meminfo3);
  EXPECT_GT(meminfo2.free, meminfo3.free);

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, HistogramBase::kIsPersistent);
  EXPECT_TRUE(custom_histogram);
  custom_histogram->CheckName("TestCustomHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo4;
  allocator_->GetMemoryInfo(&meminfo4);
  EXPECT_GT(meminfo3.free, meminfo4.free);

  PersistentMemoryAllocator::Iterator iter(allocator_);
  uint32_t type;
  EXPECT_NE(0U, iter.GetNext(&type));  // Histogram
  EXPECT_NE(0U, iter.GetNext(&type));  // LinearHistogram
  EXPECT_NE(0U, iter.GetNext(&type));  // BooleanHistogram
  EXPECT_NE(0U, iter.GetNext(&type));  // CustomHistogram
  EXPECT_EQ(0U, iter.GetNext(&type));

  // Create a second allocator and have it access the memory of the first.
  std::unique_ptr<HistogramBase> recovered;
  PersistentHistogramAllocator recovery(
      std::make_unique<PersistentMemoryAllocator>(
          allocator_memory_, kAllocatorMemorySize, 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator histogram_iter(&recovery);

  recovered = histogram_iter.GetNext();
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestHistogram");

  recovered = histogram_iter.GetNext();
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestLinearHistogram");

  recovered = histogram_iter.GetNext();
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestBooleanHistogram");

  recovered = histogram_iter.GetNext();
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestCustomHistogram");

  recovered = histogram_iter.GetNext();
  EXPECT_FALSE(recovered);
}

TEST_F(PersistentHistogramAllocatorTest, ConstructPaths) {
  const FilePath dir_path(FILE_PATH_LITERAL("foo/"));
  const std::string dir_string =
      dir_path.NormalizePathSeparators().AsUTF8Unsafe();

  FilePath path = GlobalHistogramAllocator::ConstructFilePath(dir_path, "bar");
  EXPECT_EQ(dir_string + "bar.pma", path.AsUTF8Unsafe());

  std::string name;
  Time stamp;
  ProcessId pid;
  EXPECT_FALSE(
      GlobalHistogramAllocator::ParseFilePath(path, &name, nullptr, nullptr));
  EXPECT_FALSE(
      GlobalHistogramAllocator::ParseFilePath(path, nullptr, &stamp, nullptr));
  EXPECT_FALSE(
      GlobalHistogramAllocator::ParseFilePath(path, nullptr, nullptr, &pid));

  path = GlobalHistogramAllocator::ConstructFilePathForUploadDir(
      dir_path, "bar", Time::FromTimeT(12345), 6789);
  EXPECT_EQ(dir_string + "bar-3039-1A85.pma", path.AsUTF8Unsafe());
  ASSERT_TRUE(
      GlobalHistogramAllocator::ParseFilePath(path, &name, &stamp, &pid));
  EXPECT_EQ(name, "bar");
  EXPECT_EQ(Time::FromTimeT(12345), stamp);
  EXPECT_EQ(static_cast<ProcessId>(6789), pid);
}

TEST_F(PersistentHistogramAllocatorTest, CreateWithFile) {
  const char temp_name[] = "CreateWithFileTest";
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file = temp_dir.GetPath().AppendASCII(temp_name);
  const size_t temp_size = 64 << 10;  // 64 KiB

  // Test creation of a new file.
  DestroyPersistentHistogramAllocator();
  GlobalHistogramAllocator::CreateWithFile(temp_file, temp_size, 0, temp_name);
  EXPECT_EQ(std::string(temp_name),
            GlobalHistogramAllocator::Get()->memory_allocator()->Name());

  // Test re-open of a possibly-existing file.
  DestroyPersistentHistogramAllocator();
  GlobalHistogramAllocator::CreateWithFile(temp_file, temp_size, 0, "");
  EXPECT_EQ(std::string(temp_name),
            GlobalHistogramAllocator::Get()->memory_allocator()->Name());

  // Test re-open of an known-existing file.
  DestroyPersistentHistogramAllocator();
  GlobalHistogramAllocator::CreateWithFile(temp_file, 0, 0, "");
  EXPECT_EQ(std::string(temp_name),
            GlobalHistogramAllocator::Get()->memory_allocator()->Name());

  // Final release so file and temp-dir can be removed.
  DestroyPersistentHistogramAllocator();
}

TEST_F(PersistentHistogramAllocatorTest, CreateSpareFile) {
  const char temp_name[] = "CreateSpareFileTest.pma";
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file = temp_dir.GetPath().AppendASCII(temp_name);
  const size_t temp_size = 64 << 10;  // 64 KiB

  ASSERT_TRUE(GlobalHistogramAllocator::CreateSpareFile(temp_file, temp_size));

  File file(temp_file, File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int64_t>(temp_size), file.GetLength());

  char buffer[256];
  for (size_t pos = 0; pos < temp_size; pos += sizeof(buffer)) {
    ASSERT_EQ(static_cast<int>(sizeof(buffer)),
              file.ReadAtCurrentPos(buffer, sizeof(buffer)));
    for (size_t i = 0; i < sizeof(buffer); ++i)
      EXPECT_EQ(0, buffer[i]);
  }
}

TEST_F(PersistentHistogramAllocatorTest, StatisticsRecorderMerge) {
  const char LinearHistogramName[] = "SRTLinearHistogram";
  const char SparseHistogramName[] = "SRTSparseHistogram";
  const size_t global_sr_initial_histogram_count =
      StatisticsRecorder::GetHistogramCount();
  const size_t global_sr_initial_bucket_ranges_count =
      StatisticsRecorder::GetBucketRanges().size();

  // Create a local StatisticsRecorder in which the newly created histogram
  // will be recorded. The global allocator must be replaced after because the
  // act of releasing will cause the active SR to forget about all histograms
  // in the relased memory.
  std::unique_ptr<StatisticsRecorder> local_sr =
      StatisticsRecorder::CreateTemporaryForTesting();
  EXPECT_EQ(0U, StatisticsRecorder::GetHistogramCount());
  GlobalHistogramAllocator* old_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  GlobalHistogramAllocator::CreateWithLocalMemory(kAllocatorMemorySize, 0, "");
  ASSERT_TRUE(GlobalHistogramAllocator::Get());

  // Create a linear histogram for merge testing.
  HistogramBase* histogram1 =
      LinearHistogram::FactoryGet(LinearHistogramName, 1, 10, 10, 0);
  ASSERT_TRUE(histogram1);
  EXPECT_EQ(1U, StatisticsRecorder::GetHistogramCount());
  histogram1->Add(3);
  histogram1->Add(1);
  histogram1->Add(4);
  histogram1->AddCount(1, 4);
  histogram1->Add(6);

  // Create a sparse histogram for merge testing.
  HistogramBase* histogram2 =
      SparseHistogram::FactoryGet(SparseHistogramName, 0);
  ASSERT_TRUE(histogram2);
  EXPECT_EQ(2U, StatisticsRecorder::GetHistogramCount());
  histogram2->Add(3);
  histogram2->Add(1);
  histogram2->Add(4);
  histogram2->AddCount(1, 4);
  histogram2->Add(6);

  // Destroy the local SR and ensure that we're back to the initial state and
  // restore the global allocator. Histograms created in the local SR will
  // become unmanaged.
  GlobalHistogramAllocator* new_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  local_sr.reset();
  EXPECT_EQ(global_sr_initial_histogram_count,
            StatisticsRecorder::GetHistogramCount());
  EXPECT_EQ(global_sr_initial_bucket_ranges_count,
            StatisticsRecorder::GetBucketRanges().size());
  GlobalHistogramAllocator::Set(old_allocator);

  // Create a "recovery" allocator using the same memory as the local one.
  PersistentHistogramAllocator recovery1(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(new_allocator->memory_allocator()->data()),
          new_allocator->memory_allocator()->size(), 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator histogram_iter1(&recovery1);

  // Get the histograms that were created locally (and forgotten) and merge
  // them into the global SR. New objects will be created.
  std::unique_ptr<HistogramBase> recovered;
  while (true) {
    recovered = histogram_iter1.GetNext();
    if (!recovered)
      break;

    recovery1.MergeHistogramDeltaToStatisticsRecorder(recovered.get());
    HistogramBase* found =
        StatisticsRecorder::FindHistogram(recovered->histogram_name());
    EXPECT_NE(recovered.get(), found);
  }
  EXPECT_EQ(global_sr_initial_histogram_count + 2,
            StatisticsRecorder::GetHistogramCount());

  // Check the merged histograms for accuracy.
  HistogramBase* found = StatisticsRecorder::FindHistogram(LinearHistogramName);
  ASSERT_TRUE(found);
  std::unique_ptr<HistogramSamples> snapshot = found->SnapshotSamples();
  EXPECT_EQ(found->SnapshotSamples()->TotalCount(), snapshot->TotalCount());
  EXPECT_EQ(1, snapshot->GetCount(3));
  EXPECT_EQ(5, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(4));
  EXPECT_EQ(1, snapshot->GetCount(6));

  found = StatisticsRecorder::FindHistogram(SparseHistogramName);
  ASSERT_TRUE(found);
  snapshot = found->SnapshotSamples();
  EXPECT_EQ(found->SnapshotSamples()->TotalCount(), snapshot->TotalCount());
  EXPECT_EQ(1, snapshot->GetCount(3));
  EXPECT_EQ(5, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(4));
  EXPECT_EQ(1, snapshot->GetCount(6));

  // Verify that the LinearHistogram's BucketRanges was registered with the
  // global SR since the recovery allocator does not specify a custom
  // RangesManager.
  ASSERT_EQ(global_sr_initial_bucket_ranges_count + 1,
            StatisticsRecorder::GetBucketRanges().size());

  // Perform additional histogram increments.
  histogram1->AddCount(1, 3);
  histogram1->Add(6);
  histogram2->AddCount(1, 3);
  histogram2->Add(7);

  // Do another merge.
  PersistentHistogramAllocator recovery2(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(new_allocator->memory_allocator()->data()),
          new_allocator->memory_allocator()->size(), 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator histogram_iter2(&recovery2);
  while (true) {
    recovered = histogram_iter2.GetNext();
    if (!recovered)
      break;
    recovery2.MergeHistogramDeltaToStatisticsRecorder(recovered.get());
  }
  EXPECT_EQ(global_sr_initial_histogram_count + 2,
            StatisticsRecorder::GetHistogramCount());

  // And verify.
  found = StatisticsRecorder::FindHistogram(LinearHistogramName);
  snapshot = found->SnapshotSamples();
  EXPECT_EQ(found->SnapshotSamples()->TotalCount(), snapshot->TotalCount());
  EXPECT_EQ(1, snapshot->GetCount(3));
  EXPECT_EQ(8, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(4));
  EXPECT_EQ(2, snapshot->GetCount(6));

  found = StatisticsRecorder::FindHistogram(SparseHistogramName);
  snapshot = found->SnapshotSamples();
  EXPECT_EQ(found->SnapshotSamples()->TotalCount(), snapshot->TotalCount());
  EXPECT_EQ(1, snapshot->GetCount(3));
  EXPECT_EQ(8, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(4));
  EXPECT_EQ(1, snapshot->GetCount(6));
  EXPECT_EQ(1, snapshot->GetCount(7));
}

// Verify that when merging histograms from an allocator with the global
// StatisticsRecorder, if the histogram has no samples to be merged, then it
// is skipped (no lookup/registration of the histogram with the SR).
TEST_F(PersistentHistogramAllocatorTest,
       StatisticsRecorderMerge_IsDefinitelyEmpty) {
  const size_t global_sr_initial_histogram_count =
      StatisticsRecorder::GetHistogramCount();
  const size_t global_sr_initial_bucket_ranges_count =
      StatisticsRecorder::GetBucketRanges().size();

  // Create a local StatisticsRecorder in which the newly created histogram
  // will be recorded. The global allocator must be replaced after because the
  // act of releasing will cause the active SR to forget about all histograms
  // in the released memory.
  std::unique_ptr<StatisticsRecorder> local_sr =
      StatisticsRecorder::CreateTemporaryForTesting();
  EXPECT_EQ(0U, StatisticsRecorder::GetHistogramCount());
  GlobalHistogramAllocator* old_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  GlobalHistogramAllocator::CreateWithLocalMemory(kAllocatorMemorySize, 0, "");
  ASSERT_TRUE(GlobalHistogramAllocator::Get());

  // Create a bunch of histograms, and call SnapshotDelta() on all of them so
  // that their next SnapshotDelta() calls return an empty HistogramSamples.
  LinearHistogram::FactoryGet("SRTLinearHistogram1", 1, 10, 10, 0);
  HistogramBase* histogram2 =
      LinearHistogram::FactoryGet("SRTLinearHistogram2", 1, 10, 10, 0);
  histogram2->Add(3);
  histogram2->SnapshotDelta();
  HistogramBase* histogram3 =
      LinearHistogram::FactoryGet("SRTLinearHistogram3", 1, 10, 10, 0);
  histogram3->Add(1);
  histogram3->Add(10);
  histogram3->SnapshotDelta();
  SparseHistogram::FactoryGet("SRTSparseHistogram1", 0);
  HistogramBase* sparse_histogram2 =
      SparseHistogram::FactoryGet("SRTSparseHistogram2", 0);
  sparse_histogram2->Add(3);
  sparse_histogram2->SnapshotDelta();
  HistogramBase* sparse_histogram3 =
      SparseHistogram::FactoryGet("SRTSparseHistogram3", 0);
  sparse_histogram3->Add(1);
  sparse_histogram3->Add(10);
  sparse_histogram3->SnapshotDelta();

  EXPECT_EQ(6U, StatisticsRecorder::GetHistogramCount());

  // Destroy the local SR and ensure that we're back to the initial state and
  // restore the global allocator. Histograms created in the local SR will
  // become unmanaged.
  GlobalHistogramAllocator* new_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  local_sr.reset();
  EXPECT_EQ(global_sr_initial_histogram_count,
            StatisticsRecorder::GetHistogramCount());
  EXPECT_EQ(global_sr_initial_bucket_ranges_count,
            StatisticsRecorder::GetBucketRanges().size());
  GlobalHistogramAllocator::Set(old_allocator);

  // Create a "recovery" allocator using the same memory as the local one.
  PersistentHistogramAllocator recovery1(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(new_allocator->memory_allocator()->data()),
          new_allocator->memory_allocator()->size(), 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator histogram_iter1(&recovery1);

  // Get the histograms that were created locally (and forgotten) and attempt
  // to merge them into the global SR. Since their delta are all empty, nothing
  // should end up being registered with the SR.
  while (true) {
    std::unique_ptr<HistogramBase> recovered = histogram_iter1.GetNext();
    if (!recovered) {
      break;
    }

    recovery1.MergeHistogramDeltaToStatisticsRecorder(recovered.get());
    HistogramBase* found =
        StatisticsRecorder::FindHistogram(recovered->histogram_name());
    EXPECT_FALSE(found);
  }
  EXPECT_EQ(global_sr_initial_histogram_count,
            StatisticsRecorder::GetHistogramCount());

  // Same as above, but with MergeHistogramFinalDeltaToStatisticsRecorder()
  // instead of MergeHistogramDeltaToStatisticsRecorder().
  PersistentHistogramAllocator recovery2(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(new_allocator->memory_allocator()->data()),
          new_allocator->memory_allocator()->size(), 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator histogram_iter2(&recovery2);
  while (true) {
    std::unique_ptr<HistogramBase> recovered = histogram_iter2.GetNext();
    if (!recovered) {
      break;
    }

    recovery2.MergeHistogramFinalDeltaToStatisticsRecorder(recovered.get());
    HistogramBase* found =
        StatisticsRecorder::FindHistogram(recovered->histogram_name());
    EXPECT_FALSE(found);
  }
  EXPECT_EQ(global_sr_initial_histogram_count,
            StatisticsRecorder::GetHistogramCount());
}

TEST_F(PersistentHistogramAllocatorTest, MultipleSameSparseHistograms) {
  const std::string kSparseHistogramName = "SRTSparseHistogram";

  // Create a temporary SR so that histograms created during this test aren't
  // leaked to other tests.
  std::unique_ptr<StatisticsRecorder> local_sr =
      StatisticsRecorder::CreateTemporaryForTesting();

  // Create a sparse histogram.
  HistogramBase* sparse = SparseHistogram::FactoryGet(kSparseHistogramName, 0);

  // Get the sparse histogram that was created above. We should have two
  // distinct objects, but both representing and pointing to the same data.
  PersistentHistogramAllocator::Iterator iter(GlobalHistogramAllocator::Get());
  std::unique_ptr<HistogramBase> sparse2;
  while (true) {
    sparse2 = iter.GetNext();
    if (!sparse2 || kSparseHistogramName == sparse2->histogram_name()) {
      break;
    }
  }
  ASSERT_TRUE(sparse2);
  EXPECT_NE(sparse, sparse2.get());

  // Verify that both objects can coexist, i.e., samples emitted from one can be
  // found by the other and vice versa.
  sparse->AddCount(1, 3);
  std::unique_ptr<HistogramSamples> snapshot =
      sparse->SnapshotUnloggedSamples();
  std::unique_ptr<HistogramSamples> snapshot2 =
      sparse2->SnapshotUnloggedSamples();
  EXPECT_EQ(snapshot->TotalCount(), 3);
  EXPECT_EQ(snapshot2->TotalCount(), 3);
  EXPECT_EQ(snapshot->GetCount(1), 3);
  EXPECT_EQ(snapshot2->GetCount(1), 3);
  snapshot = sparse->SnapshotDelta();
  snapshot2 = sparse2->SnapshotDelta();
  EXPECT_EQ(snapshot->TotalCount(), 3);
  EXPECT_EQ(snapshot2->TotalCount(), 0);
  EXPECT_EQ(snapshot->GetCount(1), 3);
  EXPECT_EQ(snapshot2->GetCount(1), 0);

  sparse2->AddCount(2, 6);
  snapshot = sparse->SnapshotUnloggedSamples();
  snapshot2 = sparse2->SnapshotUnloggedSamples();
  EXPECT_EQ(snapshot->TotalCount(), 6);
  EXPECT_EQ(snapshot2->TotalCount(), 6);
  EXPECT_EQ(snapshot->GetCount(2), 6);
  EXPECT_EQ(snapshot2->GetCount(2), 6);
  snapshot2 = sparse2->SnapshotDelta();
  snapshot = sparse->SnapshotDelta();
  EXPECT_EQ(snapshot->TotalCount(), 0);
  EXPECT_EQ(snapshot2->TotalCount(), 6);
  EXPECT_EQ(snapshot->GetCount(2), 0);
  EXPECT_EQ(snapshot2->GetCount(2), 6);
}

TEST_F(PersistentHistogramAllocatorTest, CustomRangesManager) {
  const char LinearHistogramName[] = "TestLinearHistogram";
  const size_t global_sr_initial_bucket_ranges_count =
      StatisticsRecorder::GetBucketRanges().size();

  // Create a local StatisticsRecorder in which the newly created histogram
  // will be recorded. The global allocator must be replaced after because the
  // act of releasing will cause the active SR to forget about all histograms
  // in the released memory.
  std::unique_ptr<StatisticsRecorder> local_sr =
      StatisticsRecorder::CreateTemporaryForTesting();
  EXPECT_EQ(0U, StatisticsRecorder::GetHistogramCount());
  GlobalHistogramAllocator* old_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  GlobalHistogramAllocator::CreateWithLocalMemory(kAllocatorMemorySize, 0, "");
  ASSERT_TRUE(GlobalHistogramAllocator::Get());

  // Create a linear histogram and verify it is registered with the local SR.
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      LinearHistogramName, /*minimum=*/1, /*maximum=*/10, /*bucket_count=*/10,
      /*flags=*/0);
  ASSERT_TRUE(histogram);
  EXPECT_EQ(1U, StatisticsRecorder::GetHistogramCount());
  histogram->Add(1);

  // Destroy the local SR and ensure that we're back to the initial state and
  // restore the global allocator. The histogram created in the local SR will
  // become unmanaged.
  GlobalHistogramAllocator* new_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  local_sr.reset();
  EXPECT_EQ(global_sr_initial_bucket_ranges_count,
            StatisticsRecorder::GetBucketRanges().size());
  GlobalHistogramAllocator::Set(old_allocator);

  // Create a "recovery" allocator using the same memory as the local one.
  PersistentHistogramAllocator recovery(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(new_allocator->memory_allocator()->data()),
          new_allocator->memory_allocator()->size(), 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));

  // Set a custom RangesManager for the recovery allocator so that the
  // BucketRanges are not registered with the global SR.
  RangesManager* ranges_manager = new RangesManager();
  recovery.SetRangesManager(ranges_manager);
  EXPECT_EQ(0U, ranges_manager->GetBucketRanges().size());

  // Get the histogram that was created locally (and forgotten).
  PersistentHistogramAllocator::Iterator histogram_iter1(&recovery);
  std::unique_ptr<HistogramBase> recovered = histogram_iter1.GetNext();
  ASSERT_TRUE(recovered);

  // Verify that there are no more histograms.
  ASSERT_FALSE(histogram_iter1.GetNext());

  // Expect that the histogram's BucketRanges was not registered with the global
  // statistics recorder since the recovery allocator specifies a custom
  // RangesManager.
  EXPECT_EQ(global_sr_initial_bucket_ranges_count,
            StatisticsRecorder::GetBucketRanges().size());

  EXPECT_EQ(1U, ranges_manager->GetBucketRanges().size());
}

TEST_F(PersistentHistogramAllocatorTest, RangesDeDuplication) {
  // This corresponds to the "ranges_ref" field of the PersistentHistogramData
  // structure defined (privately) inside persistent_histogram_allocator.cc.
  const int kRangesRefIndex = 5;

  // Create two histograms with the same ranges.
  HistogramBase* histogram1 =
      Histogram::FactoryGet("TestHistogram1", 1, 1000, 10, 0);
  HistogramBase* histogram2 =
      Histogram::FactoryGet("TestHistogram2", 1, 1000, 10, 0);
  const uint32_t ranges_ref = static_cast<Histogram*>(histogram1)
                                  ->bucket_ranges()
                                  ->persistent_reference();
  ASSERT_NE(0U, ranges_ref);
  EXPECT_EQ(ranges_ref, static_cast<Histogram*>(histogram2)
                            ->bucket_ranges()
                            ->persistent_reference());

  // Make sure that the persistent data record is also correct. Two histograms
  // will be fetched; other allocations are not "iterable".
  PersistentMemoryAllocator::Iterator iter(allocator_);
  uint32_t type;
  uint32_t ref1 = iter.GetNext(&type);
  uint32_t ref2 = iter.GetNext(&type);
  EXPECT_EQ(0U, iter.GetNext(&type));
  EXPECT_NE(0U, ref1);
  EXPECT_NE(0U, ref2);
  EXPECT_NE(ref1, ref2);

  uint32_t* data1 =
      allocator_->GetAsArray<uint32_t>(ref1, 0, kRangesRefIndex + 1);
  uint32_t* data2 =
      allocator_->GetAsArray<uint32_t>(ref2, 0, kRangesRefIndex + 1);
  EXPECT_EQ(ranges_ref, data1[kRangesRefIndex]);
  EXPECT_EQ(ranges_ref, data2[kRangesRefIndex]);
}

TEST_F(PersistentHistogramAllocatorTest, MovePersistentFile) {
  const char temp_name[] = "MovePersistentFileTest.pma";
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file = temp_dir.GetPath().AppendASCII(temp_name);
  const size_t temp_size = 64 << 10;  // 64 KiB

  // Initialize persistent histogram system with a known file path.
  DestroyPersistentHistogramAllocator();
  GlobalHistogramAllocator::CreateWithFile(temp_file, temp_size, 0, temp_name);
  GlobalHistogramAllocator* allocator = GlobalHistogramAllocator::Get();
  ASSERT_TRUE(allocator->HasPersistentLocation());
  EXPECT_EQ(allocator->GetPersistentLocation(), temp_file);
  EXPECT_TRUE(base::PathExists(temp_file));

  // Move the persistent file to a new directory.
  ScopedTempDir new_temp_dir;
  ASSERT_TRUE(new_temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(allocator->MovePersistentFile(new_temp_dir.GetPath()));

  // Verify that the persistent file was correctly moved |new_temp_dir|.
  FilePath new_temp_file = new_temp_dir.GetPath().AppendASCII(temp_name);
  ASSERT_TRUE(allocator->HasPersistentLocation());
  EXPECT_EQ(allocator->GetPersistentLocation(), new_temp_file);
  EXPECT_TRUE(base::PathExists(new_temp_file));
  EXPECT_FALSE(base::PathExists(temp_file));

  // Emit a histogram after moving the file.
  const char kHistogramName[] = "MovePersistentFile.Test";
  base::UmaHistogramBoolean(kHistogramName, true);

  // Release the allocator.
  DestroyPersistentHistogramAllocator();

  // Open and read the file in order to verify that |kHistogramName| was written
  // to it even after being moved.
  base::File file(new_temp_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::unique_ptr<char[]> data = std::make_unique<char[]>(temp_size);
  EXPECT_EQ(file.Read(/*offset=*/0, data.get(), temp_size),
            static_cast<int>(temp_size));

  // Create an allocator and iterator using the file's data.
  PersistentHistogramAllocator new_file_allocator(
      std::make_unique<PersistentMemoryAllocator>(
          data.get(), temp_size, 0, 0, "",
          PersistentMemoryAllocator::kReadWrite));
  PersistentHistogramAllocator::Iterator it(&new_file_allocator);

  // Verify that |kHistogramName| is in the file.
  std::unique_ptr<HistogramBase> histogram;
  bool found_histogram = false;
  while ((histogram = it.GetNext()) != nullptr) {
    if (strcmp(kHistogramName, histogram->histogram_name()) == 0) {
      found_histogram = true;
      break;
    }
  }
  EXPECT_TRUE(found_histogram);
}

}  // namespace base
