// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
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

  bool SerializeData(std::string* output) override {
    output->assign(data_);
    return true;
  }

 private:
  const std::string data_;
};

class FailingDataSerializer : public ImportantFileWriter::DataSerializer {
 public:
  bool SerializeData(std::string* output) override { return false; }
};

enum WriteCallbackObservationState {
  NOT_CALLED,
  CALLED_WITH_ERROR,
  CALLED_WITH_SUCCESS,
};

class WriteCallbacksObserver {
 public:
  WriteCallbacksObserver() = default;

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

  DISALLOW_COPY_AND_ASSIGN(WriteCallbacksObserver);
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
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  writer.WriteNow(std::make_unique<std::string>("foo"));
  RunLoop().RunUntilIdle();

  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, WriteWithObserver) {
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());

  // Confirm that the observer is invoked.
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow(std::make_unique<std::string>("foo"));
  RunLoop().RunUntilIdle();

  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));

  // Confirm that re-installing the observer works for another write.
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow(std::make_unique<std::string>("bar"));
  RunLoop().RunUntilIdle();

  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("bar", GetFileContent(writer.path()));

  // Confirm that writing again without re-installing the observer doesn't
  // result in a notification.
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  writer.WriteNow(std::make_unique<std::string>("baz"));
  RunLoop().RunUntilIdle();

  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("baz", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, FailedWriteWithObserver) {
  // Use an invalid file path (relative paths are invalid) to get a
  // FILE_ERROR_ACCESS_DENIED error when trying to write the file.
  ImportantFileWriter writer(FilePath().AppendASCII("bad/../path"),
                             ThreadTaskRunnerHandle::Get());
  EXPECT_FALSE(PathExists(writer.path()));
  EXPECT_EQ(NOT_CALLED, write_callback_observer_.GetAndResetObservationState());
  write_callback_observer_.ObserveNextWriteCallbacks(&writer);
  writer.WriteNow(std::make_unique<std::string>("foo"));
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
  writer.WriteNow(std::make_unique<std::string>("foo"));
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
  constexpr TimeDelta kCommitInterval = TimeDelta::FromSeconds(12345);
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get(),
                             kCommitInterval);
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
}

TEST_F(ImportantFileWriterTest, DoScheduledWrite) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
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
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
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
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
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
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
  writer.SetTimerForTesting(&timer);
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  writer.WriteNow(std::make_unique<std::string>("bar"));
  EXPECT_FALSE(writer.HasPendingWrite());
  EXPECT_FALSE(timer.IsRunning());

  RunLoop().RunUntilIdle();
  ASSERT_TRUE(PathExists(writer.path()));
  EXPECT_EQ("bar", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, DoScheduledWrite_FailToSerialize) {
  MockOneShotTimer timer;
  ImportantFileWriter writer(file_, ThreadTaskRunnerHandle::Get());
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
}

TEST_F(ImportantFileWriterTest, WriteFileAtomicallyHistogramSuffixTest) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(PathExists(file_));
  EXPECT_TRUE(ImportantFileWriter::WriteFileAtomically(file_, "baz", "test"));
  EXPECT_TRUE(PathExists(file_));
  EXPECT_EQ("baz", GetFileContent(file_));
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError", 0);
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError.test", 0);

  FilePath invalid_file_ = FilePath().AppendASCII("bad/../non_existent/path");
  EXPECT_FALSE(PathExists(invalid_file_));
  EXPECT_FALSE(
      ImportantFileWriter::WriteFileAtomically(invalid_file_, nullptr));
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError", 1);
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError.test", 0);
  EXPECT_FALSE(
      ImportantFileWriter::WriteFileAtomically(invalid_file_, nullptr, "test"));
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError", 1);
  histogram_tester.ExpectTotalCount("ImportantFile.FileCreateError.test", 1);
}

}  // namespace base
