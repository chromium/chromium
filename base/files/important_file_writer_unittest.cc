// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer.h"

#include <optional>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

std::string GetFileContent(const FilePath& path) {
  std::string content;
  if (!ReadFileToString(path, &content)) {
    NOTREACHED();
  }
  return content;
}

class DataSerializer : public ImportantFileWriter::DataSerializer {
 public:
  explicit DataSerializer(const std::string& data) : data_(data) {
  }

  std::optional<std::string> SerializeData() override {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    return data_;
  }

 private:
  const base::SequenceChecker sequence_checker_;
  const std::string data_;
};

class FailingDataSerializer : public ImportantFileWriter::DataSerializer {
 public:
  std::optional<std::string> SerializeData() override { return std::nullopt; }
};

class BackgroundDataSerializer
    : public ImportantFileWriter::BackgroundDataSerializer {
 public:
  explicit BackgroundDataSerializer(
      ImportantFileWriter::BackgroundDataProducerCallback
          data_producer_callback)
      : data_producer_callback_(std::move(data_producer_callback)) {
    DCHECK(data_producer_callback_);
  }

  ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override {
    EXPECT_TRUE(sequence_checker_.CalledOnValidSequence());
    return std::move(data_producer_callback_);
  }

  bool producer_callback_obtained() const {
    return data_producer_callback_.is_null();
  }

 private:
  const base::SequenceChecker sequence_checker_;
  ImportantFileWriter::BackgroundDataProducerCallback data_producer_callback_;
};

enum WriteCallbackObservationState {
  NOT_CALLED,
  CALLED_WITH_ERROR,
  CALLED_WITH_SUCCESS,
};

class WriteCallbacksObserver {
 public:
  WriteCallbacksObserver() = default;
  WriteCallbacksObserver(const WriteCallbacksObserver&) = delete;
  WriteCallbacksObserver& operator=(const WriteCallbacksObserver&) = delete;

  // Register OnBeforeWrite() and OnAfterWrite() to be called on the next write
  // of |writer|.
  void ObserveNextWriteCallbacks(ImportantFileWriter* writer);

  // Returns the |WriteCallbackObservationState| which was observed, then resets
  // it to |NOT_CALLED|.
  WriteCallbackObservationState GetAndResetObservationState();

 private:
  void OnBeforeWrite() {
    EXPECT_FALSE(before_write_called_);
    before_write_called_ = true;
  }

  void OnAfterWrite(bool success) {
    EXPECT_EQ(NOT_CALLED, after_write_observation_state_);
    after_write_observation_state_ =
        success ? CALLED_WITH_SUCCESS : CALLED_WITH_ERROR;
  }

  bool before_write_called_ = false;
  WriteCallbackObservationState after_write_observation_state_ = NOT_CALLED;
};

void WriteCallbacksObserver::ObserveNextWriteCallbacks(
    ImportantFileWriter* writer) {
  writer->RegisterOnNextWriteCallbacks(
      base::BindOnce(&WriteCallbacksObserver::OnBeforeWrite,
                     base::Unretained(this)),
      base::BindOnce(&WriteCallbacksObserver::OnAfterWrite,
                     base::Unretained(this)));
}

WriteCallbackObservationState
WriteCallbacksObserver::GetAndResetObservationState() {
  EXPECT_EQ(after_write_observation_state_ != NOT_CALLED, before_write_called_)
      << "The before-write callback should always be called before the "
         "after-write callback";

  WriteCallbackObservationState state = after_write_observation_state_;
  before_write_called_ = false;
  after_write_observation_state_ = NOT_CALLED;
  return state;
}

}  // namespace

class ImportantFileWriterTest : public testing::Test {
 public:
  ImportantFileWriterTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("test-file");
  }

 protected:
  WriteCallbacksObserver write_callback_observer_;
  FilePath file_;
  test::TaskEnvironment task_environment_;

 private:
  ScopedTempDir temp_dir_;
};

TEST_F(ImportantFileWriterTest, Basic) {
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  writer.WriteNow("foo");
  RunLoop().RunUntilIdle();

  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, WriteWithObserver) {
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());

  // Confirm that the observer is invoked.
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow("foo");
  RunLoop().RunUntilIdle();

  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));

  // Confirm that re-installing the observer works for another write.
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow("bar");
  RunLoop().RunUntilIdle();

  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("bar", GetFileContent(writer.path()));

  // Confirm that writing again without re-installing the observer doesn't
  // result in a notification.
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  writer.WriteNow("baz");
  RunLoop().RunUntilIdle();

  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("baz", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, FailedWriteWithObserver) {
  // Use an invalid file path (relative paths are invalid) to get a
  // FILE_ERROR_ACCESS_DENIED error when trying to write the file.
  ImportantFileWriter writer(FilePath().AppendASCII("bad/../path"),
                             SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow("foo");
  RunLoop().RunUntilIdle();

  // Confirm that the write observer was invoked with its boolean parameter set
  // to false.
  EXPECT_EQ(CALLED_WITH_ERROR,
            write_callback_observer_.GetAndResetObservationState());
  EXPECT_FALSE(PathExists(writer.path()));
}

TEST_F(ImportantFileWriterTest, CallbackRunsOnWriterThread) {
  base::Thread file_writer_thread("ImportantFileWriter test thread");
  file_writer_thread.Start();
  ImportantFileWriter writer(file_, file_writer_thread.task_runner());
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());

  // Block execution on |file_writer_thread| to verify that callbacks are
  // executed on it.
  base::WaitableEvent wait_helper(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  file_writer_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                base::Unretained(&wait_helper)));

  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow("foo");
  RunLoop().RunUntilIdle();

  // Expect the callback to not have been executed before the
  // |file_writer_thread| is unblocked.
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());

  wait_helper.Signal();
  file_writer_thread.FlushForTesting();

  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, ScheduleWrite) {
  constexpr TimeDelta kCommitInterval = Seconds(12345);
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, SingleThreadTaskRunner::GetCurrentDefault(),
                             kCommitInterval);
  EXPECT_EQ(0u, writer.previous_data_size());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  ASSERT_TRUE(timer.IsRunning());
  EXPECT_EQ(kCommitInterval, timer.GetCurrentDelay());
  timer.Fire();
  EXPECT_FALSE(writer.HasPendingWrite());
  EXPECT_FALSE(timer.IsRunning());
  RunLoop().RunUntilIdle();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
  EXPECT_EQ(3u, writer.previous_data_size());
}

TEST_F(ImportantFileWriterTest, DoScheduledWrite) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  writer.DoScheduledWrite();
  EXPECT_FALSE(writer.HasPendingWrite());
  RunLoop().RunUntilIdle();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, BatchingWrites) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  writer.SetTimerForTesting(&timer);
  DataSerializer foo("foo"), bar("bar"), baz("baz");
  writer.ScheduleWrite(&foo);
  writer.ScheduleWrite(&bar);
  writer.ScheduleWrite(&baz);
  ASSERT_TRUE(timer.IsRunning());
  timer.Fire();
  RunLoop().RunUntilIdle();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("baz", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, ScheduleWrite_FailToSerialize) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  FailingDataSerializer serializer;
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  ASSERT_TRUE(timer.IsRunning());
  timer.Fire();
  EXPECT_FALSE(writer.HasPendingWrite());
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(PathExists(writer.path()));
}

TEST_F(ImportantFileWriterTest, ScheduleWrite_WriteNow) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  writer.WriteNow("bar");
  EXPECT_FALSE(writer.HasPendingWrite());
  EXPECT_FALSE(timer.IsRunning());

  RunLoop().RunUntilIdle();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("bar", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, DoScheduledWrite_FailToSerialize) {
  base::HistogramTester histogram_tester;
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  FailingDataSerializer serializer;
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());

  writer.DoScheduledWrite();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_FALSE(writer.HasPendingWrite());
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(PathExists(writer.path()));
  // We don't record metrics in case the serialization fails.
  histogram_tester.ExpectTotalCount("ImportantFile.SerializationDuration", 0);
  histogram_tester.ExpectTotalCount("ImportantFile.WriteDuration", 0);
}

TEST_F(ImportantFileWriterTest, ScheduleWriteWithBackgroundDataSerializer) {
  base::HistogramTester histogram_tester;
  base::Thread file_writer_thread("ImportantFileWriter test thread");
  file_writer_thread.Start();
  constexpr TimeDelta kCommitInterval = Seconds(12345);
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, file_writer_thread.task_runner(),
                             kCommitInterval);
  EXPECT_EQ(0u, writer.previous_data_size());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  ASSERT_FALSE(file_writer_thread.task_runner()->RunsTasksInCurrentSequence());
  BackgroundDataSerializer serializer(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        EXPECT_TRUE(
            file_writer_thread.task_runner()->RunsTasksInCurrentSequence());
        return "foo";
      }));
  writer.ScheduleWriteWithBackgroundDataSerializer(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  EXPECT_FALSE(serializer.producer_callback_obtained());
  ASSERT_TRUE(timer.IsRunning());
  EXPECT_EQ(kCommitInterval, timer.GetCurrentDelay());

  timer.Fire();
  EXPECT_FALSE(writer.HasPendingWrite());
  EXPECT_TRUE(serializer.producer_callback_obtained());
  EXPECT_FALSE(timer.IsRunning());
  file_writer_thread.FlushForTesting();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
  histogram_tester.ExpectTotalCount("ImportantFile.SerializationDuration", 1);
  histogram_tester.ExpectTotalCount("ImportantFile.WriteDuration", 1);
}

TEST_F(ImportantFileWriterTest,
       ScheduleWriteWithBackgroundDataSerializer_FailToSerialize) {
  base::HistogramTester histogram_tester;
  base::Thread file_writer_thread("ImportantFileWriter test thread");
  file_writer_thread.Start();
  constexpr TimeDelta kCommitInterval = Seconds(12345);
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, file_writer_thread.task_runner(),
                             kCommitInterval);
  EXPECT_EQ(0u, writer.previous_data_size());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  ASSERT_FALSE(file_writer_thread.task_runner()->RunsTasksInCurrentSequence());
  BackgroundDataSerializer serializer(
      base::BindLambdaForTesting([&]() -> std::optional<std::string> {
        EXPECT_TRUE(
            file_writer_thread.task_runner()->RunsTasksInCurrentSequence());
        return std::nullopt;
      }));
  writer.ScheduleWriteWithBackgroundDataSerializer(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  EXPECT_FALSE(serializer.producer_callback_obtained());
  EXPECT_TRUE(timer.IsRunning());

  timer.Fire();
  EXPECT_FALSE(timer.IsRunning());
  EXPECT_TRUE(serializer.producer_callback_obtained());
  EXPECT_FALSE(writer.HasPendingWrite());
  file_writer_thread.FlushForTesting();
  EXPECT_FALSE(PathExists(writer.path()));
  // We record the foreground serialization metric despite later failure in
  // background sequence.
  histogram_tester.ExpectTotalCount("ImportantFile.SerializationDuration", 1);
  histogram_tester.ExpectTotalCount("ImportantFile.WriteDuration", 0);
}

// Test that the chunking to avoid very large writes works.
TEST_F(ImportantFileWriterTest, WriteLargeFile) {
  // One byte larger than kMaxWriteAmount.
  const std::string large_data(8 * 1024 * 1024 + 1, 'g');
  EXPECT_FALSE(PathExists(file_));
  EXPECT_TRUE(ImportantFileWriter::WriteFileAtomically(file_, large_data));
  std::string actual;
  EXPECT_TRUE(ReadFileToString(file_, &actual));
  EXPECT_EQ(large_data, actual);
}

// Verify that a UMA metric for the serialization duration is recorded.
TEST_F(ImportantFileWriterTest, SerializationDuration) {
  base::HistogramTester histogram_tester;
  ImportantFileWriter writer(file_,
                             SingleThreadTaskRunner::GetCurrentDefault());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  writer.DoScheduledWrite();
  RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount("ImportantFile.SerializationDuration", 1);
  histogram_tester.ExpectTotalCount("ImportantFile.WriteDuration", 1);
}

// Verify that a UMA metric for the serialization duration is recorded if the
// ImportantFileWriter has a custom histogram suffix.
TEST_F(ImportantFileWriterTest, SerializationDurationWithCustomSuffix) {
  base::HistogramTester histogram_tester;
  ImportantFileWriter writer(file_, SingleThreadTaskRunner::GetCurrentDefault(),
                             "Foo");
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  writer.DoScheduledWrite();
  RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount("ImportantFile.SerializationDuration.Foo",
                                    1);
  histogram_tester.ExpectTotalCount("ImportantFile.WriteDuration.Foo", 1);
}

}  // namespace base
