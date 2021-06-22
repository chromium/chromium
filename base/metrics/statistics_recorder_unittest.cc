// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/metrics/sparse_histogram.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Class to make sure any manipulations we do to the min log level are
// contained (i.e., do not affect other unit tests).
class LogStateSaver {
 public:
  LogStateSaver() = default;
  LogStateSaver(const LogStateSaver&) = delete;
  LogStateSaver& operator=(const LogStateSaver&) = delete;
  ~LogStateSaver() { logging::SetMinLogLevel(old_min_log_level_); }

 private:
  int old_min_log_level_ = logging::GetMinLogLevel();
};

// Test implementation of RecordHistogramChecker interface.
class OddRecordHistogramChecker : public base::RecordHistogramChecker {
 public:
  ~OddRecordHistogramChecker() override = default;

  // base::RecordHistogramChecker:
  bool ShouldRecord(uint32_t histogram_hash) const override {
    return histogram_hash % 2;
  }
};

}  // namespace

namespace base {

using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

class StatisticsRecorderTest : public testing::TestWithParam<bool> {
 public:
  StatisticsRecorderTest(const StatisticsRecorderTest&) = delete;
  StatisticsRecorderTest& operator=(const StatisticsRecorderTest&) = delete;

 protected:
  const int32_t kAllocatorMemorySize = 64 << 10;  // 64 KiB

  StatisticsRecorderTest() : use_persistent_histogram_allocator_(GetParam()) {
    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    InitializeStatisticsRecorder();

    // Use persistent memory for histograms if so indicated by test parameter.
    if (use_persistent_histogram_allocator_) {
      GlobalHistogramAllocator::CreateWithLocalMemory(kAllocatorMemorySize, 0,
                                                      "StatisticsRecorderTest");
    }
  }

  ~StatisticsRecorderTest() override {
    GlobalHistogramAllocator::ReleaseForTesting();
    UninitializeStatisticsRecorder();
  }

  void InitializeStatisticsRecorder() {
    DCHECK(!statistics_recorder_);
    statistics_recorder_ = StatisticsRecorder::CreateTemporaryForTesting();
  }

  // Deletes the global recorder if there is any. This is used by test
  // NotInitialized to ensure a clean global state.
  void UninitializeStatisticsRecorder() {
    statistics_recorder_.reset();
    delete StatisticsRecorder::top_;
    DCHECK(!StatisticsRecorder::top_);
  }

  bool HasGlobalRecorder() { return StatisticsRecorder::top_ != nullptr; }

  Histogram* CreateHistogram(const char* name,
                             HistogramBase::Sample min,
                             HistogramBase::Sample max,
                             size_t bucket_count) {
    BucketRanges* ranges = new BucketRanges(bucket_count + 1);
    Histogram::InitializeBucketRanges(min, max, ranges);
    const BucketRanges* registered_ranges =
        StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);
    return new Histogram(name, min, max, registered_ranges);
  }

  void InitLogOnShutdown() { StatisticsRecorder::InitLogOnShutdown(); }

  bool IsVLogInitialized() { return StatisticsRecorder::is_vlog_initialized_; }

  void ResetVLogInitialized() {
    UninitializeStatisticsRecorder();
    StatisticsRecorder::is_vlog_initialized_ = false;
  }

  const bool use_persistent_histogram_allocator_;

  std::unique_ptr<StatisticsRecorder> statistics_recorder_;
  std::unique_ptr<GlobalHistogramAllocator> old_global_allocator_;

 private:
  LogStateSaver log_state_saver_;
};

// Run all HistogramTest cases with both heap and persistent memory.
INSTANTIATE_TEST_SUITE_P(Allocator, StatisticsRecorderTest, testing::Bool());

TEST_P(StatisticsRecorderTest, NotInitialized) {
  UninitializeStatisticsRecorder();
  EXPECT_FALSE(HasGlobalRecorder());

  HistogramBase* const histogram =
      CreateHistogram("TestHistogram", 1, 1000, 10);
  EXPECT_EQ(StatisticsRecorder::RegisterOrDeleteDuplicate(histogram),
            histogram);
  EXPECT_TRUE(HasGlobalRecorder());
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram));

  UninitializeStatisticsRecorder();
  EXPECT_FALSE(HasGlobalRecorder());

  BucketRanges* const ranges = new BucketRanges(3);
  ranges->ResetChecksum();
  EXPECT_EQ(StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges),
            ranges);
  EXPECT_TRUE(HasGlobalRecorder());
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(),
              UnorderedElementsAre(ranges));
}

TEST_P(StatisticsRecorderTest, RegisterBucketRanges) {
  std::vector<const BucketRanges*> registered_ranges;

  BucketRanges* ranges1 = new BucketRanges(3);
  ranges1->ResetChecksum();
  BucketRanges* ranges2 = new BucketRanges(4);
  ranges2->ResetChecksum();

  // Register new ranges.
  EXPECT_EQ(ranges1,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges1));
  EXPECT_EQ(ranges2,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges2));
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));

  // Register some ranges again.
  EXPECT_EQ(ranges1,
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges1));
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));

  // Make sure the ranges is still the one we know.
  ASSERT_EQ(3u, ranges1->size());
  EXPECT_EQ(0, ranges1->range(0));
  EXPECT_EQ(0, ranges1->range(1));
  EXPECT_EQ(0, ranges1->range(2));

  // Register ranges with same values.
  BucketRanges* ranges3 = new BucketRanges(3);
  ranges3->ResetChecksum();
  EXPECT_EQ(ranges1,  // returning ranges1
            StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges3));
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(),
              UnorderedElementsAre(ranges1, ranges2));
}

TEST_P(StatisticsRecorderTest, RegisterHistogram) {
  // Create a Histogram that was not registered.
  Histogram* const histogram1 = CreateHistogram("TestHistogram1", 1, 1000, 10);

  EXPECT_THAT(StatisticsRecorder::GetHistograms(), IsEmpty());

  // Register the Histogram.
  EXPECT_EQ(histogram1,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram1));
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1));

  // Register the same Histogram again.
  EXPECT_EQ(histogram1,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram1));
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1));

  // Register another Histogram with the same name.
  Histogram* const histogram2 = CreateHistogram("TestHistogram1", 1, 1000, 10);
  EXPECT_NE(histogram1, histogram2);
  EXPECT_EQ(histogram1,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram2));
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1));

  // Register another Histogram with a different name.
  Histogram* const histogram3 = CreateHistogram("TestHistogram0", 1, 1000, 10);
  EXPECT_NE(histogram1, histogram3);
  EXPECT_EQ(histogram3,
            StatisticsRecorder::RegisterOrDeleteDuplicate(histogram3));
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1, histogram3));
}

TEST_P(StatisticsRecorderTest, FindHistogram) {
  HistogramBase* histogram1 = Histogram::FactoryGet(
      "TestHistogram1", 1, 1000, 10, HistogramBase::kNoFlags);
  HistogramBase* histogram2 = Histogram::FactoryGet(
      "TestHistogram2", 1, 1000, 10, HistogramBase::kNoFlags);

  EXPECT_EQ(histogram1, StatisticsRecorder::FindHistogram("TestHistogram1"));
  EXPECT_EQ(histogram2, StatisticsRecorder::FindHistogram("TestHistogram2"));
  EXPECT_FALSE(StatisticsRecorder::FindHistogram("TestHistogram"));

  // Create a new global allocator using the same memory as the old one. Any
  // old one is kept around so the memory doesn't get released.
  old_global_allocator_ = GlobalHistogramAllocator::ReleaseForTesting();
  if (use_persistent_histogram_allocator_) {
    GlobalHistogramAllocator::CreateWithPersistentMemory(
        const_cast<void*>(old_global_allocator_->data()),
        old_global_allocator_->length(), 0, old_global_allocator_->Id(),
        old_global_allocator_->Name());
  }

  // Reset statistics-recorder to validate operation from a clean start.
  UninitializeStatisticsRecorder();
  InitializeStatisticsRecorder();

  if (use_persistent_histogram_allocator_) {
    EXPECT_TRUE(StatisticsRecorder::FindHistogram("TestHistogram1"));
    EXPECT_TRUE(StatisticsRecorder::FindHistogram("TestHistogram2"));
  } else {
    EXPECT_FALSE(StatisticsRecorder::FindHistogram("TestHistogram1"));
    EXPECT_FALSE(StatisticsRecorder::FindHistogram("TestHistogram2"));
  }
  EXPECT_FALSE(StatisticsRecorder::FindHistogram("TestHistogram"));
}

TEST_P(StatisticsRecorderTest, WithName) {
  Histogram::FactoryGet("TestHistogram1", 1, 1000, 10, Histogram::kNoFlags);
  Histogram::FactoryGet("TestHistogram2", 1, 1000, 10, Histogram::kNoFlags);
  Histogram::FactoryGet("TestHistogram3", 1, 1000, 10, Histogram::kNoFlags);

  const auto histograms = StatisticsRecorder::GetHistograms();
  EXPECT_THAT(histograms, SizeIs(3));
  EXPECT_THAT(StatisticsRecorder::WithName(histograms, ""), SizeIs(3));
  EXPECT_THAT(StatisticsRecorder::WithName(histograms, "Test"), SizeIs(3));
  EXPECT_THAT(StatisticsRecorder::WithName(histograms, "1"), SizeIs(1));
  EXPECT_THAT(StatisticsRecorder::WithName(histograms, "hello"), IsEmpty());
}

TEST_P(StatisticsRecorderTest, RegisterHistogramWithFactoryGet) {
  EXPECT_THAT(StatisticsRecorder::GetHistograms(), IsEmpty());

  // Create a histogram.
  HistogramBase* const histogram1 = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1));

  // Get an existing histogram.
  HistogramBase* const histogram2 = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  EXPECT_EQ(histogram1, histogram2);
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1));

  // Create a LinearHistogram.
  HistogramBase* const histogram3 = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1, histogram3));

  // Create a BooleanHistogram.
  HistogramBase* const histogram4 = BooleanHistogram::FactoryGet(
      "TestBooleanHistogram", HistogramBase::kNoFlags);
  EXPECT_THAT(StatisticsRecorder::GetHistograms(),
              UnorderedElementsAre(histogram1, histogram3, histogram4));

  // Create a CustomHistogram.
  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  HistogramBase* const histogram5 = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, HistogramBase::kNoFlags);
  EXPECT_THAT(
      StatisticsRecorder::GetHistograms(),
      UnorderedElementsAre(histogram1, histogram3, histogram4, histogram5));
}

TEST_P(StatisticsRecorderTest, RegisterHistogramWithMacros) {
  // Macros cache pointers and so tests that use them can only be run once.
  // Stop immediately if this test has run previously.
  static bool already_run = false;
  if (already_run)
    return;
  already_run = true;

  StatisticsRecorder::Histograms registered_histograms;

  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogramCounts", 1, 1000000, 50, HistogramBase::kNoFlags);

  // The histogram we got from macro is the same as from FactoryGet.
  LOCAL_HISTOGRAM_COUNTS("TestHistogramCounts", 30);
  registered_histograms = StatisticsRecorder::GetHistograms();
  ASSERT_EQ(1u, registered_histograms.size());
  EXPECT_EQ(histogram, registered_histograms[0]);

  LOCAL_HISTOGRAM_TIMES("TestHistogramTimes", TimeDelta::FromDays(1));
  LOCAL_HISTOGRAM_ENUMERATION("TestHistogramEnumeration", 20, 200);

  EXPECT_THAT(StatisticsRecorder::GetHistograms(), SizeIs(3));
}

TEST_P(StatisticsRecorderTest, BucketRangesSharing) {
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(), IsEmpty());

  Histogram::FactoryGet("Histogram", 1, 64, 8, HistogramBase::kNoFlags);
  Histogram::FactoryGet("Histogram2", 1, 64, 8, HistogramBase::kNoFlags);
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(), SizeIs(1));

  Histogram::FactoryGet("Histogram3", 1, 64, 16, HistogramBase::kNoFlags);
  EXPECT_THAT(StatisticsRecorder::GetBucketRanges(), SizeIs(2));
}

TEST_P(StatisticsRecorderTest, ToJSON) {
  Histogram::FactoryGet("TestHistogram1", 1, 1000, 50, HistogramBase::kNoFlags)
      ->Add(30);
  Histogram::FactoryGet("TestHistogram1", 1, 1000, 50, HistogramBase::kNoFlags)
      ->Add(40);
  Histogram::FactoryGet("TestHistogram2", 1, 1000, 50, HistogramBase::kNoFlags)
      ->Add(30);
  Histogram::FactoryGet("TestHistogram2", 1, 1000, 50, HistogramBase::kNoFlags)
      ->Add(40);

  std::string json(StatisticsRecorder::ToJSON(JSON_VERBOSITY_LEVEL_FULL));

  // Check for valid JSON.
  absl::optional<Value> root = JSONReader::Read(json);
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_dict());

  // No query should be set.
  ASSERT_FALSE(root->FindKey("query"));

  const Value* histogram_list = root->FindListKey("histograms");

  ASSERT_TRUE(histogram_list);
  ASSERT_EQ(2u, histogram_list->GetList().size());

  // Examine the first histogram.
  const Value& histogram_dict = histogram_list->GetList()[0];
  ASSERT_TRUE(histogram_dict.is_dict());

  auto sample_count = histogram_dict.FindIntKey("count");
  ASSERT_TRUE(sample_count);
  EXPECT_EQ(2, *sample_count);

  const Value* buckets_list = histogram_dict.FindListKey("buckets");
  ASSERT_TRUE(buckets_list);
  EXPECT_EQ(2u, buckets_list->GetList().size());

  // Check the serialized JSON with a different verbosity level.
  json = StatisticsRecorder::ToJSON(JSON_VERBOSITY_LEVEL_OMIT_BUCKETS);
  root = JSONReader::Read(json);
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->is_dict());
  histogram_list = root->FindListKey("histograms");
  ASSERT_TRUE(histogram_list);
  ASSERT_EQ(2u, histogram_list->GetList().size());
  const Value& histogram_dict2 = histogram_list->GetList()[0];
  ASSERT_TRUE(histogram_dict2.is_dict());
  sample_count = histogram_dict2.FindIntKey("count");
  ASSERT_TRUE(sample_count);
  EXPECT_EQ(2, *sample_count);
  buckets_list = histogram_dict2.FindListKey("buckets");
  // Bucket information should be omitted.
  ASSERT_FALSE(buckets_list);
}

TEST_P(StatisticsRecorderTest, IterationTest) {
  Histogram::FactoryGet("IterationTest1", 1, 64, 16, HistogramBase::kNoFlags);
  Histogram::FactoryGet("IterationTest2", 1, 64, 16, HistogramBase::kNoFlags);

  auto histograms = StatisticsRecorder::GetHistograms();
  EXPECT_THAT(histograms, SizeIs(2));
  histograms = StatisticsRecorder::NonPersistent(std::move(histograms));
  EXPECT_THAT(histograms, SizeIs(use_persistent_histogram_allocator_ ? 0 : 2));

  // Create a new global allocator using the same memory as the old one. Any
  // old one is kept around so the memory doesn't get released.
  old_global_allocator_ = GlobalHistogramAllocator::ReleaseForTesting();
  if (use_persistent_histogram_allocator_) {
    GlobalHistogramAllocator::CreateWithPersistentMemory(
        const_cast<void*>(old_global_allocator_->data()),
        old_global_allocator_->length(), 0, old_global_allocator_->Id(),
        old_global_allocator_->Name());
  }

  // Reset statistics-recorder to validate operation from a clean start.
  UninitializeStatisticsRecorder();
  InitializeStatisticsRecorder();

  histograms = StatisticsRecorder::GetHistograms();
  EXPECT_THAT(histograms, SizeIs(use_persistent_histogram_allocator_ ? 2 : 0));
  histograms = StatisticsRecorder::NonPersistent(std::move(histograms));
  EXPECT_THAT(histograms, IsEmpty());
}

namespace {

// CallbackCheckWrapper is simply a convenient way to check and store that
// a callback was actually run.
struct CallbackCheckWrapper {
  CallbackCheckWrapper()
      : called(false),
        last_histogram_name(""),
        last_name_hash(HashMetricName("")),
        last_histogram_value(0) {}

  void OnHistogramChanged(const char* histogram_name,
                          uint64_t name_hash,
                          base::HistogramBase::Sample histogram_value) {
    called = true;
    last_histogram_name = histogram_name;
    last_name_hash = name_hash;
    last_histogram_value = histogram_value;
  }

  bool called;
  const char* last_histogram_name;
  uint64_t last_name_hash;
  base::HistogramBase::Sample last_histogram_value;
};

}  // namespace

TEST_P(StatisticsRecorderTest,
       AddHistogramCallbackBeforeHistogramRegistration) {
  test::TaskEnvironment task_environment;
  const char* histogram_name = "TestHistogram";
  CallbackCheckWrapper callback_wrapper;

  auto callback =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper)));
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  HistogramBase* const histogram = CreateHistogram(histogram_name, 1, 1000, 10);
  EXPECT_EQ(StatisticsRecorder::RegisterOrDeleteDuplicate(histogram),
            histogram);

  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists,
            base::HistogramBase::kCallbackExists);
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());
}

TEST_P(StatisticsRecorderTest,
       RemoveHistogramCallbackBeforeHistogramRegistrationWithMultipleClients) {
  test::TaskEnvironment task_environment;
  const char* histogram_name = "TestHistogram";
  CallbackCheckWrapper callback_wrapper1;
  CallbackCheckWrapper callback_wrapper2;

  auto callback1 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper1)));
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  auto callback2 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper2)));
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  callback1.reset();
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  callback2.reset();
  EXPECT_FALSE(base::StatisticsRecorder::have_active_callbacks());

  HistogramBase* const histogram = CreateHistogram(histogram_name, 1, 1000, 10);
  EXPECT_EQ(StatisticsRecorder::RegisterOrDeleteDuplicate(histogram),
            histogram);

  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists, 0);
  EXPECT_FALSE(base::StatisticsRecorder::have_active_callbacks());
}

TEST_P(StatisticsRecorderTest, AddHistogramCallbackWithMultipleClients) {
  test::TaskEnvironment task_environment;
  std::string histogram_name = "TestHistogram";
  HistogramBase* histogram = Histogram::FactoryGet(histogram_name, 1, 1000, 10,
                                                   HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram);

  CallbackCheckWrapper callback_wrapper1;
  CallbackCheckWrapper callback_wrapper2;

  auto callback1 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper1)));

  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists,
            base::HistogramBase::kCallbackExists);
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  auto callback2 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper2)));

  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists,
            base::HistogramBase::kCallbackExists);
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  histogram->Add(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_wrapper1.called);
  histogram->Add(1);
  EXPECT_TRUE(callback_wrapper2.called);
}

TEST_P(StatisticsRecorderTest, RemoveHistogramCallbackWithMultipleClients) {
  test::TaskEnvironment task_environment;
  std::string histogram_name = "TestHistogram";
  HistogramBase* histogram = Histogram::FactoryGet(histogram_name, 1, 1000, 10,
                                                   HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram);

  CallbackCheckWrapper callback_wrapper1;
  CallbackCheckWrapper callback_wrapper2;

  auto callback1 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper1)));
  auto callback2 =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper2)));

  callback1.reset();
  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists,
            base::HistogramBase::kCallbackExists);
  EXPECT_TRUE(base::StatisticsRecorder::have_active_callbacks());

  callback2.reset();
  EXPECT_EQ(histogram->flags() & base::HistogramBase::kCallbackExists, 0);
  EXPECT_FALSE(base::StatisticsRecorder::have_active_callbacks());

  histogram->Add(1);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(callback_wrapper1.called);
  EXPECT_FALSE(callback_wrapper2.called);
}

// Check that callback is used.
TEST_P(StatisticsRecorderTest, CallbackUsedTest) {
  test::TaskEnvironment task_environment;
  {
    HistogramBase* histogram = Histogram::FactoryGet(
        "TestHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
    EXPECT_TRUE(histogram);

    CallbackCheckWrapper callback_wrapper;

    auto callback = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        "TestHistogram",
        base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                            base::Unretained(&callback_wrapper)));

    histogram->Add(1);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_wrapper.called);
    EXPECT_STREQ(callback_wrapper.last_histogram_name, "TestHistogram");
    EXPECT_EQ(callback_wrapper.last_name_hash, HashMetricName("TestHistogram"));
    EXPECT_EQ(callback_wrapper.last_histogram_value, 1);
  }

  {
    HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
        "TestLinearHistogram", 1, 1000, 10, HistogramBase::kNoFlags);

    CallbackCheckWrapper callback_wrapper;

    auto callback = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        "TestLinearHistogram",
        base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                            base::Unretained(&callback_wrapper)));

    linear_histogram->Add(1);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_wrapper.called);
    EXPECT_STREQ(callback_wrapper.last_histogram_name, "TestLinearHistogram");
    EXPECT_EQ(callback_wrapper.last_name_hash,
              HashMetricName("TestLinearHistogram"));
    EXPECT_EQ(callback_wrapper.last_histogram_value, 1);
  }

  {
    std::vector<int> custom_ranges;
    custom_ranges.push_back(1);
    custom_ranges.push_back(5);
    HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
        "TestCustomHistogram", custom_ranges, HistogramBase::kNoFlags);

    CallbackCheckWrapper callback_wrapper;

    auto callback = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        "TestCustomHistogram",
        base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                            base::Unretained(&callback_wrapper)));

    custom_histogram->Add(1);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_wrapper.called);
    EXPECT_STREQ(callback_wrapper.last_histogram_name, "TestCustomHistogram");
    EXPECT_EQ(callback_wrapper.last_name_hash,
              HashMetricName("TestCustomHistogram"));
    EXPECT_EQ(callback_wrapper.last_histogram_value, 1);
  }

  {
    HistogramBase* custom_histogram = SparseHistogram::FactoryGet(
        "TestSparseHistogram", HistogramBase::kNoFlags);

    CallbackCheckWrapper callback_wrapper;

    auto callback = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        "TestSparseHistogram",
        base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                            base::Unretained(&callback_wrapper)));

    custom_histogram->Add(1);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_wrapper.called);
    EXPECT_STREQ(callback_wrapper.last_histogram_name, "TestSparseHistogram");
    EXPECT_EQ(callback_wrapper.last_name_hash,
              HashMetricName("TestSparseHistogram"));
    EXPECT_EQ(callback_wrapper.last_histogram_value, 1);
  }
}

// Check that setting a callback before the histogram exists works.
TEST_P(StatisticsRecorderTest, CallbackUsedBeforeHistogramCreatedTest) {
  test::TaskEnvironment task_environment;
  CallbackCheckWrapper callback_wrapper;

  auto callback =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          "TestHistogram",
          base::BindRepeating(&CallbackCheckWrapper::OnHistogramChanged,
                              base::Unretained(&callback_wrapper)));

  HistogramBase* histogram = Histogram::FactoryGet("TestHistogram", 1, 1000, 10,
                                                   HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram);
  histogram->Add(1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_wrapper.called);
  EXPECT_STREQ(callback_wrapper.last_histogram_name, "TestHistogram");
  EXPECT_EQ(callback_wrapper.last_name_hash, HashMetricName("TestHistogram"));
  EXPECT_EQ(callback_wrapper.last_histogram_value, 1);
}

TEST_P(StatisticsRecorderTest, GlobalCallbackCalled) {
  HistogramBase* histogram = Histogram::FactoryGet("TestHistogram", 1, 1000, 10,
                                                   HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram);

  // This is a static rather than passing the variable to the lambda
  // as a reference capture, as only stateless lambdas can be cast to a raw
  // function pointer.
  static size_t callback_callcount;
  callback_callcount = 0;
  auto callback = [](const char* histogram_name, uint64_t name_hash,
                     HistogramBase::Sample sample) {
    EXPECT_STREQ(histogram_name, "TestHistogram");
    EXPECT_EQ(sample, 1);
    ++callback_callcount;
  };

  base::StatisticsRecorder::SetGlobalSampleCallback(callback);

  // Test that adding a histogram sample calls our callback.
  histogram->Add(1);
  EXPECT_EQ(callback_callcount, 1u);

  // Test that the callback gets correctly unregistered.
  base::StatisticsRecorder::SetGlobalSampleCallback(nullptr);
  histogram->Add(2);
  EXPECT_EQ(callback_callcount, 1u);
}

TEST_P(StatisticsRecorderTest, LogOnShutdownNotInitialized) {
  ResetVLogInitialized();
  logging::SetMinLogLevel(logging::LOG_WARNING);
  InitializeStatisticsRecorder();
  EXPECT_FALSE(VLOG_IS_ON(1));
  EXPECT_FALSE(IsVLogInitialized());
  InitLogOnShutdown();
  EXPECT_FALSE(IsVLogInitialized());
}

TEST_P(StatisticsRecorderTest, LogOnShutdownInitializedExplicitly) {
  ResetVLogInitialized();
  logging::SetMinLogLevel(logging::LOG_WARNING);
  InitializeStatisticsRecorder();
  EXPECT_FALSE(VLOG_IS_ON(1));
  EXPECT_FALSE(IsVLogInitialized());
  logging::SetMinLogLevel(logging::LOG_VERBOSE);
  EXPECT_TRUE(VLOG_IS_ON(1));
  InitLogOnShutdown();
  EXPECT_TRUE(IsVLogInitialized());
}

TEST_P(StatisticsRecorderTest, LogOnShutdownInitialized) {
  ResetVLogInitialized();
  logging::SetMinLogLevel(logging::LOG_VERBOSE);
  InitializeStatisticsRecorder();
  EXPECT_TRUE(VLOG_IS_ON(1));
  EXPECT_TRUE(IsVLogInitialized());
}

class TestHistogramProvider : public StatisticsRecorder::HistogramProvider {
 public:
  explicit TestHistogramProvider(
      std::unique_ptr<PersistentHistogramAllocator> allocator)
      : allocator_(std::move(allocator)) {
    StatisticsRecorder::RegisterHistogramProvider(weak_factory_.GetWeakPtr());
  }
  TestHistogramProvider(const TestHistogramProvider&) = delete;
  TestHistogramProvider& operator=(const TestHistogramProvider&) = delete;

  void MergeHistogramDeltas() override {
    PersistentHistogramAllocator::Iterator hist_iter(allocator_.get());
    while (true) {
      std::unique_ptr<base::HistogramBase> histogram = hist_iter.GetNext();
      if (!histogram)
        break;
      allocator_->MergeHistogramDeltaToStatisticsRecorder(histogram.get());
    }
  }

 private:
  std::unique_ptr<PersistentHistogramAllocator> allocator_;
  WeakPtrFactory<TestHistogramProvider> weak_factory_{this};
};

TEST_P(StatisticsRecorderTest, ImportHistogramsTest) {
  // Create a second SR to create some histograms for later import.
  std::unique_ptr<StatisticsRecorder> temp_sr =
      StatisticsRecorder::CreateTemporaryForTesting();

  // Extract any existing global allocator so a new one can be created.
  std::unique_ptr<GlobalHistogramAllocator> old_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();

  // Create a histogram inside a new allocator for testing.
  GlobalHistogramAllocator::CreateWithLocalMemory(kAllocatorMemorySize, 0, "");
  HistogramBase* histogram = LinearHistogram::FactoryGet("Foo", 1, 10, 11, 0);
  histogram->Add(3);

  // Undo back to the starting point.
  std::unique_ptr<GlobalHistogramAllocator> new_allocator =
      GlobalHistogramAllocator::ReleaseForTesting();
  GlobalHistogramAllocator::Set(std::move(old_allocator));
  temp_sr.reset();

  // Create a provider that can supply histograms to the current SR.
  TestHistogramProvider provider(std::move(new_allocator));

  // Verify that the created histogram is no longer known.
  ASSERT_FALSE(StatisticsRecorder::FindHistogram(histogram->histogram_name()));

  // Now test that it merges.
  StatisticsRecorder::ImportProvidedHistograms();
  HistogramBase* found =
      StatisticsRecorder::FindHistogram(histogram->histogram_name());
  ASSERT_TRUE(found);
  EXPECT_NE(histogram, found);
  std::unique_ptr<HistogramSamples> snapshot = found->SnapshotSamples();
  EXPECT_EQ(1, snapshot->TotalCount());
  EXPECT_EQ(1, snapshot->GetCount(3));

  // Finally, verify that updates can also be merged.
  histogram->Add(3);
  histogram->Add(5);
  StatisticsRecorder::ImportProvidedHistograms();
  snapshot = found->SnapshotSamples();
  EXPECT_EQ(3, snapshot->TotalCount());
  EXPECT_EQ(2, snapshot->GetCount(3));
  EXPECT_EQ(1, snapshot->GetCount(5));
}

TEST_P(StatisticsRecorderTest, RecordHistogramChecker) {
  // Before record checker is set all histograms should be recorded.
  EXPECT_TRUE(StatisticsRecorder::ShouldRecordHistogram(1));
  EXPECT_TRUE(StatisticsRecorder::ShouldRecordHistogram(2));

  auto record_checker = std::make_unique<OddRecordHistogramChecker>();
  StatisticsRecorder::SetRecordChecker(std::move(record_checker));
  EXPECT_TRUE(StatisticsRecorder::ShouldRecordHistogram(1));
  EXPECT_FALSE(StatisticsRecorder::ShouldRecordHistogram(2));
}

}  // namespace base
