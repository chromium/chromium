// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_metrics_recorder.h"

#include <array>
#include <cstddef>
#include <memory>

#include "base/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class LockMetricsRecorderTest : public testing::Test {
 public:
  LockMetricsRecorderTest() = default;

 protected:
  static const LockMetricTag* GetTag(const LockMetricTagList& tags,
                                     size_t index) {
    return tags[index];
  }

  LockMetricsRecorder lock_metrics_recorder_{
      base::PassKey<LockMetricsRecorderTest>(), "LockMetricsRecorderTest"};

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

namespace {

const LockMetricTag& GetTestCoreLockMetricTag() {
  static constinit LockMetricTag tag("BaseLock.TestCoreLockTag");
  return tag;
}

const LockMetricTag& GetTestCustomLockMetricTag() {
  static constinit LockMetricTag tag("BaseLock.TestCustomLockTag");
  return tag;
}

class IsolatedTestThread : public PlatformThread::Delegate {
 public:
  IsolatedTestThread(std::string_view thread_name, base::OnceClosure task)
      : thread_name_(thread_name), task_(std::move(task)) {}

  void ThreadMain() override {
    LockMetricsRecorder::EnableRecordingOnCurrentThread(thread_name_);
    std::move(task_).Run();
  }

 private:
  const std::string_view thread_name_;
  base::OnceClosure task_;
};

void RecordAndVerifySampleOnCurrentThread(TimeDelta duration) {
  LockMetricsRecorder* recorder = LockMetricsRecorder::GetForCurrentThread();
  ASSERT_NE(recorder, nullptr);

  recorder->RecordLockAcquisitionTime(
      {duration, Lock::GetBaseLockMetricTagList()});

  size_t num_samples = 0;
  recorder->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        EXPECT_EQ(sample.wait_time, duration);
        num_samples++;
      });
  EXPECT_EQ(num_samples, 1u);
}

}  // namespace

// Test that samples are classified internally by lock type.
TEST_F(LockMetricsRecorderTest, SamplesClassifiedByLockType) {
  constexpr size_t kSamplesRecordedPerType = 3;

  const LockMetricTag& base_lock_metric_tag = Lock::GetBaseLockMetricTag();
  const LockMetricTag& test_core_lock_tag = GetTestCoreLockMetricTag();
  const LockMetricTag& test_custom_lock_tag = GetTestCustomLockMetricTag();

  for (size_t i = 0; i < kSamplesRecordedPerType; i++) {
    lock_metrics_recorder_.RecordLockAcquisitionTime(
        {Microseconds(i),
         LockMetricTagList{base_lock_metric_tag, test_core_lock_tag}});

    lock_metrics_recorder_.RecordLockAcquisitionTime(
        {Milliseconds(i),
         LockMetricTagList{base_lock_metric_tag, test_custom_lock_tag}});
  }

  size_t test_core_lock_num_samples = 0;
  size_t test_custom_lock_num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        if (GetTag(sample.tags, 1) &&
            GetTag(sample.tags, 1)->hash() == test_core_lock_tag.hash()) {
          EXPECT_EQ(Microseconds(test_core_lock_num_samples), sample.wait_time);
          test_core_lock_num_samples++;
        } else if (GetTag(sample.tags, 1) && GetTag(sample.tags, 1)->hash() ==
                                                 test_custom_lock_tag.hash()) {
          EXPECT_EQ(Milliseconds(test_custom_lock_num_samples),
                    sample.wait_time);
          test_custom_lock_num_samples++;
        } else {
          GTEST_FAIL() << "Unexpected lock type";
        }
      });
  EXPECT_EQ(test_core_lock_num_samples, kSamplesRecordedPerType);
  EXPECT_EQ(test_custom_lock_num_samples, kSamplesRecordedPerType);
}

// Test that recording while iterating through the ring buffer is not permitted.
TEST_F(LockMetricsRecorderTest, TestRecordingWhileIterating) {
  EXPECT_TRUE(lock_metrics_recorder_.ShouldRecordLockAcquisitionTime());
  lock_metrics_recorder_.RecordLockAcquisitionTime(
      {Microseconds(1), LockMetricTagList{GetTestCoreLockMetricTag()}});
  lock_metrics_recorder_.ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        EXPECT_FALSE(lock_metrics_recorder_.ShouldRecordLockAcquisitionTime());
      });
  EXPECT_TRUE(lock_metrics_recorder_.ShouldRecordLockAcquisitionTime());
}

// Test that writing more samples there is space for in the internal buffer of
// lock metrics recorder overwrites the oldest samples.
TEST_F(LockMetricsRecorderTest, TestSampleOverwrite) {
  // Size of lock metric recorder's internal buffer.
  constexpr size_t kBufferSize = LockMetricsRecorder::kMaxSamples;
  // The number of additional samples written.
  constexpr size_t kExtraSamples = 5;

  // The i-th sample has value i microseconds to allow us to check the age of
  // the sample.
  for (size_t i = 0; i < kBufferSize + kExtraSamples; i++) {
    lock_metrics_recorder_.RecordLockAcquisitionTime(
        {Microseconds(i), LockMetricTagList{GetTestCoreLockMetricTag()}});
  }
  size_t num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        // The oldest `kExtraSamples` are expected to be overwritten, leaving
        // us with samples starting at `kExtraSamples` microseconds.
        EXPECT_EQ(sample.wait_time, Microseconds(num_samples + kExtraSamples));
        num_samples++;
      });
  EXPECT_EQ(num_samples, kBufferSize);
}

// Test that samples are iterated over exactly once.
TEST_F(LockMetricsRecorderTest, TestSamplesIteratedOverExactlyOnce) {
  constexpr size_t kSamplesPerIteration = 10;
  static_assert(kSamplesPerIteration <= LockMetricsRecorder::kMaxSamples);

  size_t num_samples = 0;
  for (size_t i = 0; i < 2; i++) {
    const size_t num_samples_prev = num_samples;
    // The j-th sample has value i microseconds to allow us to check the age of
    // the sample.
    for (size_t j = 0; j < kSamplesPerIteration; j++) {
      lock_metrics_recorder_.RecordLockAcquisitionTime(
          {Microseconds(j + num_samples),
           LockMetricTagList{GetTestCoreLockMetricTag()}});
    }
    lock_metrics_recorder_.ForEachSample(
        [&](const LockMetricsRecorder::LockMetricSample& sample) {
          EXPECT_EQ(sample.wait_time, Microseconds(num_samples));
          num_samples++;
        });
    EXPECT_EQ(num_samples - num_samples_prev, kSamplesPerIteration);
  }
}

// Test if ScopedLockAcquisitionTimer records a sample as expected
TEST_F(LockMetricsRecorderTest, ScopedLockAcquisitionTimerRecordsSample) {
  size_t num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        num_samples++;
      });
  EXPECT_EQ(num_samples, 0);

  {
    auto timer = LockMetricsRecorder::ScopedLockAcquisitionTimer::CreateForTest(
        &lock_metrics_recorder_,
        LockMetricTagList{Lock::GetBaseLockMetricTag(),
                          GetTestCoreLockMetricTag(),
                          GetTestCustomLockMetricTag()});
    PlatformThread::Sleep(Microseconds(500));
  }
  lock_metrics_recorder_.ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        EXPECT_GT(sample.wait_time, Microseconds(500));
        num_samples++;
      });
  EXPECT_EQ(num_samples, 1);
}

// Test that recording is only enabled for threads matching the parameter
// filter.
TEST(LockMetricsRecorderFeatureTest, FilterByThreadName) {
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      base::features::kRecordLockAcquisitionTime);
  LockMetricsRecorder::SetAllowedThreadsForTesting({"AllowedThread"});

  // Attempt to enable recording on an allowed thread.
  LockMetricsRecorder::EnableRecordingOnCurrentThread("AllowedThread");
  RecordAndVerifySampleOnCurrentThread(Microseconds(100));
  LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();

  // Attempt to enable recording on a blocked thread.
  LockMetricsRecorder::EnableRecordingOnCurrentThread("BlockedThread");
  EXPECT_EQ(LockMetricsRecorder::GetForCurrentThread(), nullptr);

  // Instantiating a ScopedLockAcquisitionTimer on a blocked thread should
  // not record anything.
  {
    LockMetricsRecorder::ScopedLockAcquisitionTimer timer(
        LockMetricTagList{GetTestCoreLockMetricTag()});
    PlatformThread::Sleep(Microseconds(500));
  }

  EXPECT_EQ(LockMetricsRecorder::GetForCurrentThread(), nullptr);

  LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();
}

// Test that different threads are able to record samples when they are allowed
// to by the thread name filter.
TEST(LockMetricsRecorderFeatureTest, MultipleThreadsFilterByThreadName) {
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;

  constexpr size_t kNumThreads = 3;
  const std::vector<std::string> kAllowedThreads = {
      "AllowedThread1", "AllowedThread2", "AllowedThread3"};

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      base::features::kRecordLockAcquisitionTime);

  LockMetricsRecorder::SetAllowedThreadsForTesting(kAllowedThreads);

  std::array<std::unique_ptr<IsolatedTestThread>, kNumThreads> delegates;
  std::array<PlatformThreadHandle, kNumThreads> handles;

  // Verify that allowed threads are able to record samples.
  for (size_t i = 0; i < kNumThreads; ++i) {
    delegates[i] = std::make_unique<IsolatedTestThread>(
        kAllowedThreads[i], base::BindLambdaForTesting([]() {
          RecordAndVerifySampleOnCurrentThread(Microseconds(1));
        }));
    ASSERT_TRUE(PlatformThread::Create(0, delegates[i].get(), &handles[i]));
  }

  for (size_t i = 0; i < kNumThreads; ++i) {
    PlatformThread::Join(handles[i]);
  }
}

namespace {
class MetricsRecorderTestThread : public PlatformThread::Delegate {
 public:
  MetricsRecorderTestThread(Lock* lock, WaitableEvent* should_start)
      : lock_(lock), should_start_(should_start) {}

  void ThreadMain() override {
    // Signal that this thread has taken the lock, then go to sleep for a
    // long-time holding the lock to make sure the other thread takes the slow
    // path of acquire and will record a sample
    AutoLock auto_lock(*lock_);
    should_start_->Signal();
    PlatformThread::Sleep(Seconds(1));
  }

 private:
  raw_ptr<Lock> lock_;
  raw_ptr<WaitableEvent> should_start_;
};

// Two threads try to acquire the lock with very high-probability of lock
// contention.
void MakeThreadsContendOnLock() {
  Lock lock;
  PlatformThreadHandle handle;
  WaitableEvent event;
  MetricsRecorderTestThread thread(&lock, &event);

  // Create another thread and wait for it to acquire the lock before trying to
  // acquire the lock to create contention.
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  event.Wait();
  {
    AutoLock auto_lock(lock);
  }

  PlatformThread::Join(handle);
}

// Creates a `LockMetricsRecorder` object to record lock metrics for the current
// thread to histograms without subsampling, and enables it to record lock
// metrics for different lock types.
class LockMetricsEnabledTest : public testing::Test {
 public:
  LockMetricsEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        base::features::kRecordLockAcquisitionTime);
    LockMetricsRecorder::SetAllowedThreadsForTesting(
        {"LockMetricsEnabledTest", "BackgroundThread", "HammerThread",
         "MetricsTestThread"});

    LockMetricsRecorder::EnableRecordingOnCurrentThread(
        "LockMetricsEnabledTest");
  }

  void SetUp() override {
    ASSERT_TRUE(LockMetricsRecorder::GetForCurrentThread()
                    ->ShouldRecordLockAcquisitionTime());
  }

  void TearDown() override {
    LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

}  // namespace

// Test that no samples are recorded when there is no contention on the lock.
TEST_F(LockMetricsEnabledTest, NoSamplesRecordedWhenUncontended) {
  Lock lock;

  {
    AutoLock auto_lock(lock);
  }

  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [](const LockMetricsRecorder::LockMetricSample& sample) {
        GTEST_FAIL() << "No samples expected";
      });
}

// Test that samples are recorded when there is contention on the lock.
TEST_F(LockMetricsEnabledTest, SamplesRecordedWhenContended) {
  MakeThreadsContendOnLock();
  bool did_record_sample = false;
  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        did_record_sample = true;
      });
  EXPECT_TRUE(did_record_sample);
}

// Test that samples are correctly flushed to histograms.
TEST_F(LockMetricsEnabledTest, ReportLockAcquisitionTimesFlushesToHistograms) {
  constexpr std::string_view kThreadName = "MetricsTestThread";
  base::HistogramTester histogram_tester;
  static const LockMetricsRecorder::LockMetricSample kBaseLockMetricSample = {
      Microseconds(100), Lock::GetBaseLockMetricTagList()};
  static const LockMetricsRecorder::LockMetricSample kTestLockMetricSample = {
      Milliseconds(1), LockMetricTagList{GetTestCoreLockMetricTag()}};

  IsolatedTestThread background_thread(
      kThreadName, base::BindLambdaForTesting([]() {
        LockMetricsRecorder* recorder =
            LockMetricsRecorder::GetForCurrentThread();

        // Record some samples
        recorder->RecordLockAcquisitionTime(kBaseLockMetricSample);
        recorder->RecordLockAcquisitionTime(kBaseLockMetricSample);
        recorder->RecordLockAcquisitionTime(kTestLockMetricSample);

        // Flush to histograms
        recorder->ReportLockAcquisitionTimes();

        // Verify buffer is now empty (flushed)
        size_t remaining_samples = 0;
        recorder->ForEachSample(
            [&](const LockMetricsRecorder::LockMetricSample& sample) {
              remaining_samples++;
            });
        EXPECT_EQ(remaining_samples, 0u);
      }));

  PlatformThreadHandle handle;
  ASSERT_TRUE(PlatformThread::Create(0, &background_thread, &handle));
  PlatformThread::Join(handle);

  // Verify hits on both histograms. Use >= since organic lock contention
  // during test execution can register extra samples.
  std::unique_ptr<HistogramSamples> base_samples =
      histogram_tester.GetHistogramSamplesSinceCreation(StrCat(
          {"Scheduling.ContendedLockAcquisitionTime.BaseLock.", kThreadName}));
  EXPECT_GE(base_samples->TotalCount(), 2);

  std::unique_ptr<HistogramSamples> test_samples =
      histogram_tester.GetHistogramSamplesSinceCreation(StrCat(
          {"Scheduling.ContendedLockAcquisitionTime.BaseLock.TestCoreLockTag.",
           kThreadName}));
  EXPECT_GE(test_samples->TotalCount(), 1);
}

// Test that different threads use separate thread-local storage for lock
// metrics.
TEST_F(LockMetricsEnabledTest, ThreadLocalBufferIsolation) {
  static constexpr size_t kSamples = 5;
  static constexpr TimeDelta kBackgroundThreadSampleValue = Microseconds(1);
  constexpr TimeDelta kMainThreadSampleValue = Microseconds(3);

  static const LockMetricsRecorder::LockMetricSample kBackgroundThreadSample = {
      kBackgroundThreadSampleValue,
      LockMetricTagList{Lock::GetBaseLockMetricTag()}};
  static const LockMetricsRecorder::LockMetricSample kMainThreadSample = {
      kMainThreadSampleValue, LockMetricTagList{Lock::GetBaseLockMetricTag()}};

  IsolatedTestThread background_thread(
      "BackgroundThread", base::BindLambdaForTesting([]() {
        // Record samples
        for (size_t i = 0; i < kSamples; ++i) {
          LockMetricsRecorder::GetForCurrentThread()->RecordLockAcquisitionTime(
              kBackgroundThreadSample);
        }

        // Verify this thread only sees its own samples
        size_t count = 0;
        LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
            [&](const LockMetricsRecorder::LockMetricSample& sample) {
              EXPECT_EQ(sample.wait_time, kBackgroundThreadSampleValue);
              count++;
            });
        EXPECT_EQ(count, kSamples);
      }));

  PlatformThreadHandle background_thread_handle;

  ASSERT_TRUE(
      PlatformThread::Create(0, &background_thread, &background_thread_handle));

  PlatformThread::Join(background_thread_handle);

  // Check that the main thread did not record any samples
  size_t main_thread_count = 0;
  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        main_thread_count++;
      });
  EXPECT_EQ(main_thread_count, 0u);

  // Record some samples on the main thread
  for (size_t i = 0; i < kSamples; ++i) {
    LockMetricsRecorder::GetForCurrentThread()->RecordLockAcquisitionTime(
        kMainThreadSample);
  }

  // Verify that the main thread records samples correctly
  size_t main_thread_count_after = 0;
  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        EXPECT_EQ(sample.wait_time, kMainThreadSampleValue);
        main_thread_count_after++;
      });
  EXPECT_EQ(main_thread_count_after, kSamples);
}

// Test that concurrent reporting from multiple threads with the same name
// doesn't deadlock or crash due to reentrancy/lock contention.
TEST_F(LockMetricsEnabledTest, ConcurrentReportingStressTest) {
  constexpr size_t kNumThreads = 4;
  static constexpr size_t kIterations = 1000;
  constexpr std::string_view kSharedThreadName = "HammerThread";
  static const LockMetricsRecorder::LockMetricSample base_lock_metric_sample = {
      Microseconds(1), Lock::GetBaseLockMetricTagList()};
  base::HistogramTester histogram_tester;

  std::array<std::unique_ptr<IsolatedTestThread>, kNumThreads> delegates;
  std::array<PlatformThreadHandle, kNumThreads> handles;

  for (size_t i = 0; i < kNumThreads; ++i) {
    delegates[i] = std::make_unique<IsolatedTestThread>(
        kSharedThreadName, base::BindLambdaForTesting([]() {
          auto* recorder = LockMetricsRecorder::GetForCurrentThread();
          for (size_t iter = 0; iter < kIterations; ++iter) {
            recorder->RecordLockAcquisitionTime(base_lock_metric_sample);
            recorder->ReportLockAcquisitionTimes();
          }
        }));
    ASSERT_TRUE(PlatformThread::Create(0, delegates[i].get(), &handles[i]));
  }

  for (size_t i = 0; i < kNumThreads; ++i) {
    PlatformThread::Join(handles[i]);
  }

  std::unique_ptr<HistogramSamples> samples =
      histogram_tester.GetHistogramSamplesSinceCreation(
          StrCat({"Scheduling.ContendedLockAcquisitionTime.BaseLock.",
                  kSharedThreadName}));

  // Use >= to account for organic lock contention during concurrent test
  // execution.
  EXPECT_GE(samples->TotalCount(), kNumThreads * kIterations);
}

class LockMetricTagTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        base::features::kRecordLockAcquisitionTime);
    LockMetricsRecorder::SetAllowedThreadsForTesting({kThreadName});
    LockMetricsRecorder::EnableRecordingOnCurrentThread(kThreadName);
  }

  void TearDown() override {
    LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();
  }

 protected:
  static constexpr std::string_view kBaseLockPrefix =
      "Scheduling.ContendedLockAcquisitionTime.BaseLock.";
  static constexpr char kThreadName[] = "MetricTagTestThread";
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  const LockMetricTag base_lock_metric_tag_ = Lock::GetBaseLockMetricTag();
  const LockMetricTag test_core_lock_metric_tag_ = GetTestCoreLockMetricTag();
  const LockMetricTag test_custom_lock_metric_tag_ =
      GetTestCustomLockMetricTag();

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

// Test that LockMetricTag objects have unique indices and names.
TEST_F(LockMetricTagTest, UniqueIndicesAndNames) {
  EXPECT_EQ(base_lock_metric_tag_.name(), "BaseLock");
  EXPECT_EQ(test_core_lock_metric_tag_.name(), "BaseLock.TestCoreLockTag");
  EXPECT_EQ(test_custom_lock_metric_tag_.name(), "BaseLock.TestCustomLockTag");

  EXPECT_NE(base_lock_metric_tag_.hash(), test_core_lock_metric_tag_.hash());
  EXPECT_NE(base_lock_metric_tag_.hash(), test_custom_lock_metric_tag_.hash());
  EXPECT_NE(test_core_lock_metric_tag_.hash(),
            test_custom_lock_metric_tag_.hash());
}

// Verify that a sample is recorded with both a core tag and a custom tag.
TEST_F(LockMetricTagTest, ResolvesTaggedLockHistograms) {
  {
    LockMetricsRecorder::ScopedLockAcquisitionTimer metrics_timer(
        LockMetricTagList{base_lock_metric_tag_, test_core_lock_metric_tag_,
                          test_custom_lock_metric_tag_});
  }

  LockMetricsRecorder::GetForCurrentThread()->ReportLockAcquisitionTimes();

  histogram_tester_.ExpectTotalCount(StrCat({kBaseLockPrefix, kThreadName}), 1);
  histogram_tester_.ExpectTotalCount(
      StrCat({kBaseLockPrefix, "TestCoreLockTag.", kThreadName}), 1);
  histogram_tester_.ExpectTotalCount(
      StrCat({kBaseLockPrefix, "TestCustomLockTag.", kThreadName}), 1);
}

// Verify histogram caching and sample accumulation across multiple
// sequential reporting flushes.
TEST_F(LockMetricTagTest, CachesAndAccumulatesHistogramsAcrossFlushes) {
  constexpr size_t kNumBothTagSamples = 3;
  constexpr size_t kNumCoreOnlySamples = 2;

  // Record samples with both core and custom tags.
  for (size_t i = 0; i < kNumBothTagSamples; ++i) {
    LockMetricsRecorder::ScopedLockAcquisitionTimer metrics_timer(
        LockMetricTagList{base_lock_metric_tag_, test_core_lock_metric_tag_,
                          test_custom_lock_metric_tag_});
  }
  LockMetricsRecorder::GetForCurrentThread()->ReportLockAcquisitionTimes();

  // Record additional samples with core tag only.
  for (size_t i = 0; i < kNumCoreOnlySamples; ++i) {
    LockMetricsRecorder::ScopedLockAcquisitionTimer metrics_timer(
        LockMetricTagList{base_lock_metric_tag_, test_core_lock_metric_tag_});
  }
  LockMetricsRecorder::GetForCurrentThread()->ReportLockAcquisitionTimes();

  // Base lock histogram should accumulate all 5 samples.
  histogram_tester_.ExpectTotalCount(StrCat({kBaseLockPrefix, kThreadName}),
                                     kNumBothTagSamples + kNumCoreOnlySamples);

  // CoreTag histogram should accumulate all 5 samples.
  histogram_tester_.ExpectTotalCount(
      StrCat({kBaseLockPrefix, "TestCoreLockTag.", kThreadName}),
      kNumBothTagSamples + kNumCoreOnlySamples);

  // CustomTag histogram should reflect only the 3 samples.
  histogram_tester_.ExpectTotalCount(
      StrCat({kBaseLockPrefix, "TestCustomLockTag.", kThreadName}),
      kNumBothTagSamples);
}

}  // namespace base
