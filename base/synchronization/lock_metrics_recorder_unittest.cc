// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/lock_metrics_recorder.h"

#include <array>
#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {
class LockMetricsRecorderTest : public testing::Test {
 public:
  LockMetricsRecorderTest() { lock_metrics_recorder_.SetTargetCurrentThread(); }

 protected:
  LockMetricsRecorder lock_metrics_recorder_;

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};
}  // namespace

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

  size_t base_lock_num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock, [&](const TimeDelta& sample) {
        EXPECT_EQ(Microseconds(base_lock_num_samples), sample);
        base_lock_num_samples++;
      });
  EXPECT_EQ(base_lock_num_samples, kSamplesRecordedPerType);

  size_t pa_lock_num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      LockMetricsRecorder::LockType::kPartitionAllocLock,
      [&](const TimeDelta& sample) {
        EXPECT_EQ(Milliseconds(pa_lock_num_samples), sample);
        pa_lock_num_samples++;
      });
  EXPECT_EQ(pa_lock_num_samples, kSamplesRecordedPerType);
}

// Test that recording while iterating through the ring buffer is not permitted.
TEST_F(LockMetricsRecorderTest, TestRecordingWhileIterating) {
  EXPECT_TRUE(lock_metrics_recorder_.ShouldRecordLockAcquisitionTime());
  lock_metrics_recorder_.RecordLockAcquisitionTime(
      Microseconds(1), LockMetricsRecorder::LockType::kBaseLock);
  lock_metrics_recorder_.ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock, [&](const TimeDelta& sample) {
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
        Microseconds(i), LockMetricsRecorder::LockType::kBaseLock);
  }
  size_t num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock, [&](const TimeDelta& sample) {
        // The oldest `kExtraSamples` are expected to be overwritten, leaving us
        // with samples starting at `kExtraSamples` microseconds.
        EXPECT_EQ(sample, Microseconds(num_samples + kExtraSamples));
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
          Microseconds(j + num_samples),
          LockMetricsRecorder::LockType::kBaseLock);
    }
    lock_metrics_recorder_.ForEachSample(
        LockMetricsRecorder::LockType::kBaseLock, [&](const TimeDelta& sample) {
          EXPECT_EQ(sample, Microseconds(num_samples));
          num_samples++;
        });
    EXPECT_EQ(num_samples - num_samples_prev, kSamplesPerIteration);
  }
}

// Test if ScopedLockAcquisitionTimer records a sample as expected
TEST_F(LockMetricsRecorderTest, ScopedLockAcquisitionTimerRecordsSample) {
  size_t num_samples = 0;
  lock_metrics_recorder_.ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock,
      [&](const TimeDelta& sample) { num_samples++; });
  EXPECT_EQ(num_samples, 0);

  {
    auto timer = LockMetricsRecorder::ScopedLockAcquisitionTimer::CreateForTest(
        &lock_metrics_recorder_);
    PlatformThread::Sleep(Microseconds(500));
  }
  lock_metrics_recorder_.ForEachSample(LockMetricsRecorder::LockType::kBaseLock,
                                       [&](const TimeDelta& sample) {
                                         EXPECT_GT(sample, Microseconds(500));
                                         num_samples++;
                                       });
  EXPECT_EQ(num_samples, 1);
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

// Creates a LockMetricsRecorder object to record lock metrics for the current
// thread without subsampling and sets it to record lock metrics for Lock
class BaseLockMetricsTest : public testing::Test {
 public:
  BaseLockMetricsTest() {
    LockMetricsRecorder::Get()->SetTargetCurrentThread();
  }

  void SetUp() override {
    ASSERT_TRUE(LockMetricsRecorder::Get()->ShouldRecordLockAcquisitionTime());
  }

 private:
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;
};

}  // namespace

// Test that no samples are recorded when there is no contention on the lock.
TEST_F(BaseLockMetricsTest, NoSamplesRecordedWhenUncontended) {
  Lock lock;

  {
    AutoLock auto_lock(lock);
  }

  LockMetricsRecorder::Get()->ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock,
      [](const TimeDelta& sample) { GTEST_FAIL() << "No samples expected"; });
}

// Test that samples are recorded when there is contention on the lock.
TEST_F(BaseLockMetricsTest, SamplesRecordedWhenContended) {
  MakeThreadsContendOnLock();
  bool did_record_sample = false;
  LockMetricsRecorder::Get()->ForEachSample(
      LockMetricsRecorder::LockType::kBaseLock,
      [&](const TimeDelta&) { did_record_sample = true; });
  EXPECT_TRUE(did_record_sample);
}

}  // namespace base
