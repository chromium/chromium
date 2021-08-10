// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer_cleaner.h"

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::ElementsAre;

namespace base {

class ImportantFileWriterCleanerTest : public ::testing::Test {
 public:
  ImportantFileWriterCleanerTest()
      : old_file_time_(ImportantFileWriterCleaner::GetInstance()
                           .GetUpperBoundTimeForTest() -
                       TimeDelta::FromMilliseconds(1)) {}

 protected:
  // Initializes and Starts the global cleaner at construction and Stops it
  // at destruction. ("Lifetime" refers to its activity rather than existence.)
  class ScopedCleanerLifetime {
   public:
    ScopedCleanerLifetime() {
      auto& instance = ImportantFileWriterCleaner::GetInstance();
      instance.Initialize();
      instance.Start();
    }
    ScopedCleanerLifetime(const ScopedCleanerLifetime&) = delete;
    ScopedCleanerLifetime& operator=(const ScopedCleanerLifetime&) = delete;
    ~ScopedCleanerLifetime() {
      ImportantFileWriterCleaner::GetInstance().Stop();
    }
  };

  void SetUp() override;
  void TearDown() override;

  const FilePath& dir_1() const { return dir_1_; }
  const FilePath& dir_1_file_new() const { return dir_1_file_new_; }
  const FilePath& dir_1_file_old() const { return dir_1_file_old_; }
  const FilePath& dir_1_file_other() const { return dir_1_file_other_; }
  const FilePath& dir_2() const { return dir_2_; }
  const FilePath& dir_2_file_new() const { return dir_2_file_new_; }
  const FilePath& dir_2_file_old() const { return dir_2_file_old_; }
  const FilePath& dir_2_file_other() const { return dir_2_file_other_; }

  void StartCleaner() {
    DCHECK(!cleaner_lifetime_.has_value());
    cleaner_lifetime_.emplace();
  }

  void StopCleaner() {
    DCHECK(cleaner_lifetime_.has_value());
    cleaner_lifetime_.reset();
  }

  void CreateNewFileInDir(const FilePath& dir, FilePath& path) {
    File file = CreateAndOpenTemporaryFileInDir(dir, &path);
    ASSERT_TRUE(file.IsValid());
  }

  void CreateOldFileInDir(const FilePath& dir, FilePath& path) {
    File file = CreateAndOpenTemporaryFileInDir(dir, &path);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.SetTimes(Time::Now(), old_file_time_));
  }

  void CreateOldFile(const FilePath& path) {
    File file(path, File::FLAG_CREATE | File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.SetTimes(Time::Now(), old_file_time_));
  }

  ScopedTempDir temp_dir_;
  test::TaskEnvironment task_environment_;

 private:
  const Time old_file_time_;
  FilePath dir_1_;
  FilePath dir_2_;
  FilePath dir_1_file_new_;
  FilePath dir_1_file_old_;
  FilePath dir_1_file_other_;
  FilePath dir_2_file_new_;
  FilePath dir_2_file_old_;
  FilePath dir_2_file_other_;
  absl::optional<ScopedCleanerLifetime> cleaner_lifetime_;
};

void ImportantFileWriterCleanerTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // Create two directories that will hold files to be cleaned.
  dir_1_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dir_1"));
  ASSERT_TRUE(CreateDirectory(dir_1_));
  dir_2_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dir_2"));
  ASSERT_TRUE(CreateDirectory(dir_2_));

  // Create some old and new files in each dir.
  ASSERT_NO_FATAL_FAILURE(CreateNewFileInDir(dir_1_, dir_1_file_new_));

  ASSERT_NO_FATAL_FAILURE(CreateOldFileInDir(dir_1_, dir_1_file_old_));

  dir_1_file_other_ = dir_1_.Append(FILE_PATH_LITERAL("other.nottmp"));
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_other_));

  ASSERT_NO_FATAL_FAILURE(CreateNewFileInDir(dir_2_, dir_2_file_new_));

  ASSERT_NO_FATAL_FAILURE(CreateOldFileInDir(dir_2_, dir_2_file_old_));

  dir_2_file_other_ = dir_2_.Append(FILE_PATH_LITERAL("other.nottmp"));
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_2_file_other_));
}

void ImportantFileWriterCleanerTest::TearDown() {
  cleaner_lifetime_.reset();
  task_environment_.RunUntilIdle();
  ImportantFileWriterCleaner::GetInstance().UninitializeForTesting();
  EXPECT_TRUE(temp_dir_.Delete());
}

// Tests that adding a directory without initializing the cleaner does nothing.
TEST_F(ImportantFileWriterCleanerTest, NotInitializedNoOpAdd) {
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_TRUE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that adding a directory without starting the cleaner does nothing.
TEST_F(ImportantFileWriterCleanerTest, NotStartedNoOpAdd) {
  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_TRUE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that starting and stopping does no harm.
TEST_F(ImportantFileWriterCleanerTest, StartStop) {
  StartCleaner();
  StopCleaner();
}

// Tests that adding a directory then starting the cleaner works.
TEST_F(ImportantFileWriterCleanerTest, AddStart) {
  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  StartCleaner();
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from the added dir.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that adding multiple directories before starting cleans both.
TEST_F(ImportantFileWriterCleanerTest, AddAddStart) {
  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  ImportantFileWriterCleaner::AddDirectory(dir_2());
  StartCleaner();
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from both added dirs.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_FALSE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that starting the cleaner then adding a directory works.
TEST_F(ImportantFileWriterCleanerTest, StartAdd) {
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from the added dir.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that starting the cleaner twice doesn't cause it to clean twice.
TEST_F(ImportantFileWriterCleanerTest, StartTwice) {
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  // Recreate the old file that was just cleaned.
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_old()));

  // Start again and make sure it wasn't cleaned again.
  ImportantFileWriterCleaner::GetInstance().Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(dir_1_file_old()));
}

// Tests that adding a dir twice doesn't cause it to clean twice.
TEST_F(ImportantFileWriterCleanerTest, AddTwice) {
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  // Recreate the old file that was just cleaned.
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_old()));

  // Add the directory again and make sure nothing else is cleaned.
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(dir_1_file_old()));
}

// Tests that AddDirectory called from another thread properly bounces back to
// the main thread for processing.
TEST_F(ImportantFileWriterCleanerTest, StartAddFromOtherThread) {
  StartCleaner();

  // Add from the ThreadPool and wait for it to finish.
  TestWaitableEvent waitable_event;
  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                         ImportantFileWriterCleaner::AddDirectory(dir_1());
                         waitable_event.Signal();
                       }));
  waitable_event.Wait();

  // Allow the cleaner to run.
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from the added dir.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that adding a directory while a session is processing a previous
// directory works.
TEST_F(ImportantFileWriterCleanerTest, AddStartAdd) {
  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_2());
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from both added dirs.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_FALSE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests stopping while the background task is running.
TEST_F(ImportantFileWriterCleanerTest, StopWhileRunning) {
  ImportantFileWriterCleaner::GetInstance().Initialize();

  // Create a great many old files in dir1.
  for (int i = 0; i < 100; ++i) {
    FilePath path;
    CreateOldFileInDir(dir_1(), path);
  }

  ImportantFileWriterCleaner::AddDirectory(dir_1());
  StartCleaner();

  // It's possible that the background task will quickly delete all 100 files.
  // In all likelihood, though, the stop flag will be read and processed before
  // then. Either case is a success.
  StopCleaner();
  task_environment_.RunUntilIdle();
}

}  // namespace base
