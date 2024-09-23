// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/chromeos/tast_support/stack_sampling_recorder.h"

#include <sys/file.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/proto/stack_sampled_metrics_status/stack_sampled_metrics_status.pb.h"
#include "third_party/metrics_proto/execution_context.pb.h"

namespace chromeos::tast_support {

using ::stack_sampled_metrics_status::StackSampledMetricsStatus;

// A version of StackSamplingRecorder that overrides
// GetSuccessfullyCollectedCounts() to return a test-provided list of
// process/thread/counts.
class TestingStackSamplingRecorder : public StackSamplingRecorder {
 public:
  explicit TestingStackSamplingRecorder(base::FilePath file_path)
      : StackSamplingRecorder(std::move(file_path)) {}

  void SetSuccessfullyCollectedCounts(
      const metrics::CallStackProfileMetricsProvider::ProcessThreadCount&
          counts) {
    counts_ = counts;
  }

  // Allow tests to access the private function WriteFileHelper().
  void WriteFileHelper() { StackSamplingRecorder::WriteFileHelper(); }

 private:
  ~TestingStackSamplingRecorder() override = default;

  metrics::CallStackProfileMetricsProvider::ProcessThreadCount
  GetSuccessfullyCollectedCounts() const override {
    return counts_;
  }

  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts_;
};

class StackSamplingRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    CHECK(tmp_dir_.CreateUniqueTempDir());

    output_path_ = tmp_dir_.GetPath().Append("stack-sampling-data");
    recorder_ =
        base::MakeRefCounted<TestingStackSamplingRecorder>(output_path_);
  }

  base::ScopedTempDir tmp_dir_;
  base::FilePath output_path_;
  scoped_refptr<TestingStackSamplingRecorder> recorder_;
};

TEST_F(StackSamplingRecorderTest, ProducesValidFile) {
  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts;
  counts[metrics::BROWSER_PROCESS][metrics::MAIN_THREAD] = 5;
  counts[metrics::BROWSER_PROCESS][metrics::FILE_THREAD] = 7;
  counts[metrics::BROWSER_PROCESS][metrics::IO_THREAD] = 9;
  counts[metrics::RENDERER_PROCESS][metrics::MAIN_THREAD] = 11;
  counts[metrics::UTILITY_PROCESS][metrics::IO_THREAD] = 13;
  recorder_->SetSuccessfullyCollectedCounts(counts);

  recorder_->WriteFileHelper();

  base::File result_file(output_path_,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(result_file.IsValid());

  StackSampledMetricsStatus result;
  EXPECT_TRUE(result.ParseFromFileDescriptor(result_file.GetPlatformFile()));

  // base::test::EqualsProto() doesn't work well on MessageLite's. Validate
  // `result` manually.
  EXPECT_EQ(result.process_type_to_thread_count_map().size(), 3UL);

  ASSERT_TRUE(result.process_type_to_thread_count_map().contains(
      metrics::BROWSER_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& browser_thread_count =
      result.process_type_to_thread_count_map().at(metrics::BROWSER_PROCESS);
  EXPECT_EQ(browser_thread_count.thread_type_to_success_count().size(), 3UL);
  ASSERT_TRUE(browser_thread_count.thread_type_to_success_count().contains(
      metrics::MAIN_THREAD));
  EXPECT_EQ(browser_thread_count.thread_type_to_success_count().at(
                metrics::MAIN_THREAD),
            5);
  ASSERT_TRUE(browser_thread_count.thread_type_to_success_count().contains(
      metrics::FILE_THREAD));
  EXPECT_EQ(browser_thread_count.thread_type_to_success_count().at(
                metrics::FILE_THREAD),
            7);
  ASSERT_TRUE(browser_thread_count.thread_type_to_success_count().contains(
      metrics::IO_THREAD));
  EXPECT_EQ(browser_thread_count.thread_type_to_success_count().at(
                metrics::IO_THREAD),
            9);

  ASSERT_TRUE(result.process_type_to_thread_count_map().contains(
      metrics::RENDERER_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& renderer_thread_count =
      result.process_type_to_thread_count_map().at(metrics::RENDERER_PROCESS);
  EXPECT_EQ(renderer_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(renderer_thread_count.thread_type_to_success_count().contains(
      metrics::MAIN_THREAD));
  EXPECT_EQ(renderer_thread_count.thread_type_to_success_count().at(
                metrics::MAIN_THREAD),
            11);

  ASSERT_TRUE(result.process_type_to_thread_count_map().contains(
      metrics::UTILITY_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& utility_thread_count =
      result.process_type_to_thread_count_map().at(metrics::UTILITY_PROCESS);
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(utility_thread_count.thread_type_to_success_count().contains(
      metrics::IO_THREAD));
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().at(
                metrics::IO_THREAD),
            13);
}

TEST_F(StackSamplingRecorderTest, ProducesValidEmptyFileIfNoCounts) {
  recorder_->WriteFileHelper();

  base::File result_file(output_path_,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(result_file.IsValid());

  StackSampledMetricsStatus result;
  EXPECT_TRUE(result.ParseFromFileDescriptor(result_file.GetPlatformFile()));
  EXPECT_EQ(result.process_type_to_thread_count_map().size(), 0UL);
}

TEST_F(StackSamplingRecorderTest, TruncatesExistingFileToEmpty) {
  // Add some existing data.
  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts;
  counts[metrics::BROWSER_PROCESS][metrics::MAIN_THREAD] = 5;
  counts[metrics::BROWSER_PROCESS][metrics::FILE_THREAD] = 7;
  counts[metrics::BROWSER_PROCESS][metrics::IO_THREAD] = 9;
  counts[metrics::RENDERER_PROCESS][metrics::MAIN_THREAD] = 11;
  counts[metrics::UTILITY_PROCESS][metrics::IO_THREAD] = 13;
  recorder_->SetSuccessfullyCollectedCounts(counts);

  recorder_->WriteFileHelper();

  // Rewrite with no count.
  recorder_->SetSuccessfullyCollectedCounts({});
  recorder_->WriteFileHelper();

  int64_t file_size = -1;
  EXPECT_TRUE(base::GetFileSize(output_path_, &file_size));
  EXPECT_EQ(file_size, 0L);
}

TEST_F(StackSamplingRecorderTest, TruncatesExistingFileWithLessData) {
  // Add some existing data.
  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts;
  counts[metrics::BROWSER_PROCESS][metrics::MAIN_THREAD] = 5;
  counts[metrics::BROWSER_PROCESS][metrics::FILE_THREAD] = 7;
  counts[metrics::BROWSER_PROCESS][metrics::IO_THREAD] = 9;
  counts[metrics::RENDERER_PROCESS][metrics::MAIN_THREAD] = 11;
  counts[metrics::UTILITY_PROCESS][metrics::IO_THREAD] = 13;
  recorder_->SetSuccessfullyCollectedCounts(counts);

  recorder_->WriteFileHelper();

  // Rewrite with a different, smaller set of counts.
  counts.clear();
  counts[metrics::GPU_PROCESS][metrics::FILE_THREAD] = 15;
  counts[metrics::UTILITY_PROCESS][metrics::MAIN_THREAD] = 17;

  recorder_->SetSuccessfullyCollectedCounts(counts);
  recorder_->WriteFileHelper();

  base::File result_file(output_path_,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(result_file.IsValid());

  // Confirm that result only contains the new counts, not the old counts.
  StackSampledMetricsStatus result;
  EXPECT_TRUE(result.ParseFromFileDescriptor(result_file.GetPlatformFile()));

  // base::test::EqualsProto() doesn't work well on MessageLite's. Validate
  // `result` manually.
  EXPECT_EQ(result.process_type_to_thread_count_map().size(), 2UL);

  ASSERT_TRUE(
      result.process_type_to_thread_count_map().contains(metrics::GPU_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& gpu_thread_count =
      result.process_type_to_thread_count_map().at(metrics::GPU_PROCESS);
  EXPECT_EQ(gpu_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(gpu_thread_count.thread_type_to_success_count().contains(
      metrics::FILE_THREAD));
  EXPECT_EQ(
      gpu_thread_count.thread_type_to_success_count().at(metrics::FILE_THREAD),
      15);

  ASSERT_TRUE(result.process_type_to_thread_count_map().contains(
      metrics::UTILITY_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& utility_thread_count =
      result.process_type_to_thread_count_map().at(metrics::UTILITY_PROCESS);
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(utility_thread_count.thread_type_to_success_count().contains(
      metrics::MAIN_THREAD));
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().at(
                metrics::MAIN_THREAD),
            17);
}

TEST_F(StackSamplingRecorderTest, DoesNotCrashOnUnwritableFile) {
  // Will prevent normal file opens from working:
  ASSERT_TRUE(base::CreateDirectory(output_path_));

  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts;
  counts[metrics::GPU_PROCESS][metrics::FILE_THREAD] = 15;
  counts[metrics::UTILITY_PROCESS][metrics::MAIN_THREAD] = 17;
  recorder_->SetSuccessfullyCollectedCounts(counts);

  recorder_->WriteFileHelper();
}

// Called on a second thread to make sure the file doesn't change while locked.
// Specifically:
// 1. Writes a known pattern into the file
// 2. Locks the file
// 3. Signals that it has locked the file
// 4. Waits for the other thread to try & write to the file
// 5. CHECKs that the file still contains the original pattern
static void LockFileAndPreventChange(base::FilePath path,
                                     base::WaitableEvent* waitable) {
  // 1. Write a known pattern into the file
  base::File file(path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                            base::File::FLAG_WRITE);
  CHECK(file.IsValid());
  constexpr char kPattern[] = "Not a valid proto";
  CHECK_EQ(file.Write(0, kPattern, sizeof(kPattern)),
           static_cast<int>(sizeof(kPattern)));

  // 2. Lock the file
  CHECK_EQ(HANDLE_EINTR(flock(file.GetPlatformFile(), LOCK_EX)), 0);

  // 3. Signal that it has locked the file
  waitable->Signal();

  // 4. Wait for the other thread to try & write to the file.
  //
  // Note: Yes, using Sleep here. There's no way for the other thread to signal
  // to this thread "I've started trying to lock the file and I am now blocked
  // waiting for the file to be unlocked." This can lead to anti-flakes (cases
  // where the test passes even thought the code-under-test is wrong -- if the
  // other thread doesn't start the write until the sleep is done) but shouldn't
  // lead to flakes.
  base::PlatformThreadBase::Sleep(base::Seconds(5));

  // 5. CHECK that the file still contains the original pattern.
  char buffer[sizeof(kPattern) + 1];
  CHECK_EQ(file.Read(0, buffer, sizeof(buffer)),
           static_cast<int>(sizeof(kPattern)));
  CHECK_EQ(std::string(buffer), std::string(kPattern));
}

TEST_F(StackSamplingRecorderTest, DoesNotWriteToLockedFile) {
  metrics::CallStackProfileMetricsProvider::ProcessThreadCount counts;
  counts[metrics::GPU_PROCESS][metrics::FILE_THREAD] = 15;
  counts[metrics::UTILITY_PROCESS][metrics::MAIN_THREAD] = 17;
  recorder_->SetSuccessfullyCollectedCounts(counts);

  base::WaitableEvent waitable;
  base::Thread thread("locker_thread");
  ASSERT_TRUE(thread.StartAndWaitForTesting());

  ASSERT_TRUE(thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LockFileAndPreventChange, output_path_, &waitable)));

  // Wait for the locker to have locked the file.
  waitable.Wait();

  // Try to write. This should only return once the locker thread has released
  // the file lock.
  recorder_->WriteFileHelper();

  // The write should still have been successful once the lock is released.
  base::File result_file(output_path_,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(result_file.IsValid());

  // Confirm that result only contains the new counts, not the old counts.
  StackSampledMetricsStatus result;
  EXPECT_TRUE(result.ParseFromFileDescriptor(result_file.GetPlatformFile()));

  // base::test::EqualsProto() doesn't work well on MessageLite's. Validate
  // `result` manually.
  EXPECT_EQ(result.process_type_to_thread_count_map().size(), 2UL);

  ASSERT_TRUE(
      result.process_type_to_thread_count_map().contains(metrics::GPU_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& gpu_thread_count =
      result.process_type_to_thread_count_map().at(metrics::GPU_PROCESS);
  EXPECT_EQ(gpu_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(gpu_thread_count.thread_type_to_success_count().contains(
      metrics::FILE_THREAD));
  EXPECT_EQ(
      gpu_thread_count.thread_type_to_success_count().at(metrics::FILE_THREAD),
      15);

  ASSERT_TRUE(result.process_type_to_thread_count_map().contains(
      metrics::UTILITY_PROCESS));
  const StackSampledMetricsStatus::ThreadCountMap& utility_thread_count =
      result.process_type_to_thread_count_map().at(metrics::UTILITY_PROCESS);
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().size(), 1UL);
  ASSERT_TRUE(utility_thread_count.thread_type_to_success_count().contains(
      metrics::MAIN_THREAD));
  EXPECT_EQ(utility_thread_count.thread_type_to_success_count().at(
                metrics::MAIN_THREAD),
            17);
}

}  // namespace chromeos::tast_support
