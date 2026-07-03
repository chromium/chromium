// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_metrics_recorder.h"

#include <array>
#include <cstddef>
#include <memory>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

constexpr std::string_view kBaseLockHistogramPrefix =
    "Scheduling.ContendedLockAcquisitionTime.BaseLock.";
constexpr std::string_view kPartitionAllocLockHistogramPrefix =
    "Scheduling.ContendedLockAcquisitionTime.PartitionAllocLock.";

struct NoAllocationAsserter {
  void OnAllocation(
      const base::allocator::dispatcher::AllocationNotificationData&
          notification_data) {
    ADD_FAILURE() << "Unexpected allocation during recording.";
  }
  void OnFree(const base::allocator::dispatcher::FreeNotificationData&
                  notification_data) {}
};

class LockMetricsRecorderTest : public testing::Test {
 public:
  LockMetricsRecorderTest() = default;

 protected:
  LockMetricsRecorder lock_metrics_recorder_{
      base::PassKey<LockMetricsRecorderTest>(), "LockMetricsRecorderTest"};

  LockMetricsRecorder allocation_recorder_{
      base::PassKey<LockMetricsRecorderTest>(), "NoAllocationsDuringRecording"};

  base::HistogramTester histogram_tester_;

  int GetBaseLockSampleCount(std::string_view thread_name) const {
    return histogram_tester_
        .GetHistogramSamplesSinceCreation(
            StrCat({kBaseLockHistogramPrefix, thread_name}))
        ->TotalCount();
  }

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

// Test that samples are classified internally by type
TEST_F(LockMetricsRecorderTest, SamplesClassifiedByLockType) {
  constexpr size_t kSamplesRecordedPerType = 3;
  size_t i;

  for (i = 0; i < kSamplesRecordedPerType; i++) {
    lock_metrics_recorder_.RecordLockAcquisitionTime(
        Microseconds(i), LockMetricsRecorder::LockType::kBaseLock);
    lock_metrics_recorder_.RecordLockAcquisitionTime(
        Milliseconds(i), LockMetricsRecorder::LockType::kPartitionAllocLock);
  }

  histogram_tester_.ExpectTotalCount(
      StrCat({kBaseLockHistogramPrefix, "LockMetricsRecorderTest"}),
      kSamplesRecordedPerType);

  histogram_tester_.ExpectTotalCount(
      StrCat({kPartitionAllocLockHistogramPrefix, "LockMetricsRecorderTest"}),
      kSamplesRecordedPerType);
}

// Test if ScopedLockAcquisitionTimer records a sample as expected
TEST_F(LockMetricsRecorderTest, ScopedLockAcquisitionTimerRecordsSample) {
  {
    auto timer = LockMetricsRecorder::ScopedLockAcquisitionTimer::CreateForTest(
        &lock_metrics_recorder_);
    PlatformThread::Sleep(Microseconds(500));
  }

  EXPECT_EQ(GetBaseLockSampleCount("LockMetricsRecorderTest"), 1);
}

// Verify that no allocations occur when recording samples directly to
// histograms.
TEST_F(LockMetricsRecorderTest, NoAllocationsDuringRecording) {
  auto& dispatcher = base::allocator::dispatcher::Dispatcher::GetInstance();
  NoAllocationAsserter asserter;
  dispatcher.InitializeForTesting(&asserter);

  allocation_recorder_.RecordLockAcquisitionTime(
      Microseconds(1), LockMetricsRecorder::LockType::kBaseLock);
  allocation_recorder_.RecordLockAcquisitionTime(
      Milliseconds(1), LockMetricsRecorder::LockType::kPartitionAllocLock);

  dispatcher.ResetForTesting();
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

class IsolatedTestThread : public PlatformThread::Delegate {
 public:
  IsolatedTestThread(std::string_view thread_name, base::OnceClosure task)
      : thread_name_(thread_name), task_(std::move(task)) {}

  void ThreadMain() override {
    LockMetricsRecorder::EnableRecordingOnCurrentThread(thread_name_);
    std::move(task_).Run();
  }

 private:
  std::string_view thread_name_;
  base::OnceClosure task_;
};

void RunOnIsolatedThread(std::string_view thread_name, base::OnceClosure task) {
  IsolatedTestThread thread(thread_name, std::move(task));
  PlatformThreadHandle handle;
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  PlatformThread::Join(handle);
}

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

// Enables lock metrics recording for the current thread without subsampling.
class BaseLockMetricsTest : public testing::Test {
 public:
  BaseLockMetricsTest() {
    LockMetricsRecorder::EnableRecordingOnCurrentThread("BaseLockMetricsTest");
  }

  void SetUp() override {
    ASSERT_TRUE(LockMetricsRecorder::GetForCurrentThread()
                    ->ShouldRecordLockAcquisitionTime());
  }

  void TearDown() override {
    LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();
  }

 protected:
  base::HistogramTester histogram_tester_;

  int GetBaseLockSampleCount(std::string_view thread_name) const {
    return histogram_tester_
        .GetHistogramSamplesSinceCreation(
            StrCat({kBaseLockHistogramPrefix, thread_name}))
        ->TotalCount();
  }

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

}  // namespace

// Test that samples are recorded when there is contention on the lock.
TEST_F(BaseLockMetricsTest, SamplesRecordedWhenContended) {
  MakeThreadsContendOnLock();

  EXPECT_GE(GetBaseLockSampleCount("BaseLockMetricsTest"), 1);
}

// Test that samples are correctly written to histograms.
TEST_F(BaseLockMetricsTest, RecordLockAcquisitionTimesWritesToHistograms) {
  constexpr std::string_view kThreadName = "MetricsTestThread";

  RunOnIsolatedThread(
      kThreadName, base::BindLambdaForTesting([]() {
        auto* recorder = LockMetricsRecorder::GetForCurrentThread();

        // Record some samples
        recorder->RecordLockAcquisitionTime(
            Microseconds(100), LockMetricsRecorder::LockType::kBaseLock);
        recorder->RecordLockAcquisitionTime(
            Microseconds(200), LockMetricsRecorder::LockType::kBaseLock);
        recorder->RecordLockAcquisitionTime(
            Milliseconds(1),
            LockMetricsRecorder::LockType::kPartitionAllocLock);
      }));

  // Verify hits on both histograms. Use >= since organic lock contention
  // during test execution can register extra samples.
  EXPECT_GE(GetBaseLockSampleCount(kThreadName), 2);

  std::unique_ptr<HistogramSamples> pa_samples =
      histogram_tester_.GetHistogramSamplesSinceCreation(
          StrCat({kPartitionAllocLockHistogramPrefix, kThreadName}));
  EXPECT_GE(pa_samples->TotalCount(), 1);
}

// Test that different threads use separate thread-local storage for lock
// metrics and don't contaminate each other's buckets.
TEST_F(BaseLockMetricsTest, ThreadHistogramIsolation) {
  constexpr size_t kSamples = 5;
  constexpr TimeDelta kSampleValue = Microseconds(1);
  constexpr std::string_view kBackgroundThreadName = "BackgroundThread";
  constexpr std::string_view kMainThreadSuffix = "BaseLockMetricsTest";

  // Disable recording for the main thread so we can later confirm no samples
  // are recorded for it.
  LockMetricsRecorder::DisableRecordingOnCurrentThreadForTesting();

  RunOnIsolatedThread(
      kBackgroundThreadName, base::BindLambdaForTesting([&]() {
        // Record samples
        for (size_t i = 0; i < kSamples; ++i) {
          LockMetricsRecorder::GetForCurrentThread()->RecordLockAcquisitionTime(
              kSampleValue, LockMetricsRecorder::LockType::kBaseLock);
        }
      }));

  // Verify the background thread wrote to its histogram.
  EXPECT_GE(GetBaseLockSampleCount(kBackgroundThreadName), kSamples);

  // Verify the main thread's histogram was not contaminated.
  EXPECT_EQ(GetBaseLockSampleCount(kMainThreadSuffix), 0);
}

// Test that different threads with the same histogram suffix
// record to the same histogram.
TEST_F(BaseLockMetricsTest, SameHistogramSuffixForThreads) {
  constexpr std::string_view kHistogramSuffix = "SameSuffix";
  constexpr size_t kSamples = 10;

  RunOnIsolatedThread(
      kHistogramSuffix, base::BindLambdaForTesting([&]() {
        for (size_t i = 0; i < kSamples; ++i) {
          LockMetricsRecorder::GetForCurrentThread()->RecordLockAcquisitionTime(
              Microseconds(1), LockMetricsRecorder::LockType::kBaseLock);
        }
      }));

  RunOnIsolatedThread(
      kHistogramSuffix, base::BindLambdaForTesting([&]() {
        for (size_t i = 0; i < kSamples; ++i) {
          LockMetricsRecorder::GetForCurrentThread()->RecordLockAcquisitionTime(
              Microseconds(1), LockMetricsRecorder::LockType::kBaseLock);
        }
      }));

  EXPECT_GE(GetBaseLockSampleCount(kHistogramSuffix), 2 * kSamples);
}

// Test that concurrent recording from multiple threads with the same name
// doesn't deadlock or crash due to reentrancy/lock contention.
TEST_F(BaseLockMetricsTest, ConcurrentRecordingStressTest) {
  constexpr size_t kNumThreads = 4;
  constexpr size_t kIterations = 1000;
  constexpr std::string_view kSharedThreadName = "HammerThread";

  std::array<std::unique_ptr<IsolatedTestThread>, kNumThreads> delegates;
  std::array<PlatformThreadHandle, kNumThreads> handles;

  for (size_t i = 0; i < kNumThreads; ++i) {
    delegates[i] = std::make_unique<IsolatedTestThread>(
        kSharedThreadName, base::BindLambdaForTesting([&]() {
          auto* recorder = LockMetricsRecorder::GetForCurrentThread();
          for (size_t iter = 0; iter < kIterations; ++iter) {
            recorder->RecordLockAcquisitionTime(
                Microseconds(1), LockMetricsRecorder::LockType::kBaseLock);
          }
        }));
    ASSERT_TRUE(PlatformThread::Create(0, delegates[i].get(), &handles[i]));
  }

  for (size_t i = 0; i < kNumThreads; ++i) {
    PlatformThread::Join(handles[i]);
  }

  // Use >= to account for organic lock contention during concurrent test
  // execution.
  EXPECT_GE(GetBaseLockSampleCount(kSharedThreadName),
            kNumThreads * kIterations);
}

}  // namespace base
