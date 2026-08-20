// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/tagged_metric_lock.h"

#include "base/features.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock_metrics_recorder.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const LockMetricTag& GetTestCoreLockMetricTag() {
  static constinit LockMetricTag tag("TestCoreLockTag");
  return tag;
}

const LockMetricTag& GetTestCustomLockMetricTag() {
  static constinit LockMetricTag tag("TestCustomLockTag");
  return tag;
}

// Helper to perform a loop of Acquire/Try/Release operations and track
// acquisition count.
void AcquireAndTryLoop(TaggedMetricLock& lock,
                       size_t iterations,
                       size_t& acquired_count) {
  for (size_t i = 0; i < iterations; ++i) {
    lock.Acquire();
    acquired_count++;
    lock.Release();

    if (lock.Try()) {
      acquired_count++;
      lock.Release();
    }
  }
}

class BasicTaggedLockTestThread : public PlatformThread::Delegate {
 public:
  explicit BasicTaggedLockTestThread(TaggedMetricLock* lock) : lock_(lock) {}

  BasicTaggedLockTestThread(const BasicTaggedLockTestThread&) = delete;
  BasicTaggedLockTestThread& operator=(const BasicTaggedLockTestThread&) =
      delete;

  void ThreadMain() override { AcquireAndTryLoop(*lock_, 10, acquired_); }

  size_t acquired() const { return acquired_; }

 private:
  raw_ptr<TaggedMetricLock> lock_;
  size_t acquired_ = 0;
};

}  // namespace

class TaggedMetricLockTest : public ::testing::Test {
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
  static constexpr char kThreadName[] = "TestThread";

  // Helper thread to safely create lock contention.
  class TestThread : public PlatformThread::Delegate {
   public:
    explicit TestThread(TaggedMetricLock* lock) : lock_(lock) {
      EXPECT_TRUE(PlatformThread::Create(0, this, &handle_));
    }

    ~TestThread() override { PlatformThread::Join(handle_); }

    void ThreadMain() override {
      TaggedAutoLock auto_lock(*lock_);
      lock_acquired_event_.Signal();
      // Sleep for enough time to ensure the other thread takes the slow path
      // of acquire and will record a sample.
      PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }

    // Wait for the thread to signal that it has acquired the lock and then
    // acquire the lock on the main thread to create contention.
    void CreateLockContention() {
      lock_acquired_event_.Wait();
      TaggedAutoLock auto_lock(*lock_);
    }

   private:
    raw_ptr<TaggedMetricLock> lock_;
    WaitableEvent lock_acquired_event_;
    PlatformThreadHandle handle_;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample_;

  const LockMetricTag& base_lock_tag() const {
    return Lock::GetBaseLockMetricTag();
  }
  const LockMetricTag& test_core_lock_tag() const {
    return GetTestCoreLockMetricTag();
  }
  const LockMetricTag& test_custom_lock_tag() const {
    return GetTestCustomLockMetricTag();
  }

  static const LockMetricTag* GetTag(const LockMetricTagList& tags,
                                     size_t index) {
    return tags[index];
  }
};

// Test TaggedMetricLock getters for core_tag and custom_tag.
TEST_F(TaggedMetricLockTest, GettersReturnCorrectTags) {
  TaggedMetricLock lock_single_tag(test_core_lock_tag());
  EXPECT_EQ(lock_single_tag.core_tag().hash(), test_core_lock_tag().hash());
  EXPECT_FALSE(lock_single_tag.custom_tag().has_value());

  TaggedMetricLock lock_both_tags(test_core_lock_tag(), test_custom_lock_tag());
  EXPECT_EQ(lock_both_tags.core_tag().hash(), test_core_lock_tag().hash());
  EXPECT_TRUE(lock_both_tags.custom_tag().has_value());
  EXPECT_EQ(lock_both_tags.custom_tag()->hash(), test_custom_lock_tag().hash());
}

// Test TaggedMetricLock direct Try, AssertAcquired, AssertNotHeld, and Release.
TEST_F(TaggedMetricLockTest, DirectAcquireAndRelease) {
  TaggedMetricLock tagged_lock(test_core_lock_tag());
  tagged_lock.AssertNotHeld();

  EXPECT_TRUE(tagged_lock.Try());
  tagged_lock.AssertAcquired();

  tagged_lock.Release();
  tagged_lock.AssertNotHeld();
}

// Test TaggedAutoLock acquires on creation and releases on scope exit.
TEST_F(TaggedMetricLockTest, TaggedAutoLockRAII) {
  TaggedMetricLock tagged_lock(test_core_lock_tag());
  tagged_lock.AssertNotHeld();

  {
    TaggedAutoLock auto_lock(tagged_lock);
    tagged_lock.AssertAcquired();
  }

  tagged_lock.AssertNotHeld();
}

// Test TaggedMetricLock basic functionality on different threads.
TEST_F(TaggedMetricLockTest, ConcurrentAcquireAndRelease) {
  TaggedMetricLock tagged_lock(test_core_lock_tag());
  BasicTaggedLockTestThread thread(&tagged_lock);
  PlatformThreadHandle handle;

  constexpr size_t kAcquiredCount = 10;
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));

  size_t main_acquired = 0;
  AcquireAndTryLoop(tagged_lock, kAcquiredCount, main_acquired);

  PlatformThread::Join(handle);

  EXPECT_GE(main_acquired, kAcquiredCount);
  EXPECT_GE(thread.acquired(), kAcquiredCount);
}

// Test TaggedMetricLock contention records core_tag and custom_tag.
TEST_F(TaggedMetricLockTest, ContentionRecordsBothTags) {
  TaggedMetricLock tagged_lock(test_core_lock_tag(), test_custom_lock_tag());

  TestThread thread(&tagged_lock);
  thread.CreateLockContention();

  // Iterate through recorded samples to find the expected tags.
  // Filter explicitly rather than asserting on every sample, as organic lock
  // contention from internal process locks during test execution can place
  // extra samples into the thread-local buffer. Without the filter, the test
  // is flaky.
  bool did_record_sample = false;
  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        if (GetTag(sample.tags, 0) &&
            GetTag(sample.tags, 0)->hash() == base_lock_tag().hash() &&
            GetTag(sample.tags, 1) &&
            GetTag(sample.tags, 1)->hash() == test_core_lock_tag().hash() &&
            GetTag(sample.tags, 2) &&
            GetTag(sample.tags, 2)->hash() == test_custom_lock_tag().hash()) {
          did_record_sample = true;
        }
      });

  EXPECT_TRUE(did_record_sample);
}

// Test multiple TaggedMetricLocks log their respective tags.
TEST_F(TaggedMetricLockTest, MultipleLocksLogDistinctTags) {
  TaggedMetricLock lock1(test_core_lock_tag());
  TaggedMetricLock lock2(test_custom_lock_tag());

  {
    TestThread thread1(&lock1);
    thread1.CreateLockContention();
  }
  {
    TestThread thread2(&lock2);
    thread2.CreateLockContention();
  }

  bool lock1_logged = false;
  bool lock2_logged = false;

  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&](const LockMetricsRecorder::LockMetricSample& sample) {
        if (GetTag(sample.tags, 1) &&
            GetTag(sample.tags, 1)->hash() == test_core_lock_tag().hash()) {
          lock1_logged = true;
        } else if (GetTag(sample.tags, 1) &&
                   GetTag(sample.tags, 1)->hash() ==
                       test_custom_lock_tag().hash()) {
          lock2_logged = true;
        }
      });

  EXPECT_TRUE(lock1_logged);
  EXPECT_TRUE(lock2_logged);
}

// Test uncontended TaggedMetricLock records no samples.
TEST_F(TaggedMetricLockTest, TaggedMetricLockUncontendedRecordsNoSamples) {
  TaggedMetricLock tagged_lock(test_core_lock_tag(), test_custom_lock_tag());

  {
    TaggedAutoLock auto_lock(tagged_lock);
  }

  size_t sample_count = 0;
  LockMetricsRecorder::GetForCurrentThread()->ForEachSample(
      [&sample_count](const LockMetricsRecorder::LockMetricSample& sample) {
        sample_count++;
      });
  EXPECT_EQ(sample_count, 0u);
}

}  // namespace base
