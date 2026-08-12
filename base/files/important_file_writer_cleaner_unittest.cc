// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer_cleaner.h"

#include <optional>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

namespace {

constexpr FilePath::StringViewType kTempFilePrefix =
    FILE_PATH_LITERAL("LocalState");

}  // namespace

class ImportantFileWriterCleanerTest : public ::testing::Test {
 public:
  ImportantFileWriterCleanerTest()
      : old_file_time_(ImportantFileWriterCleaner::GetInstance()
                           .GetUpperBoundTimeForTest() -
                       Milliseconds(1)) {}

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
  const FilePath& dir_1_file_with_prefix_new() const {
    return dir_1_file_with_prefix_new_;
  }
  const FilePath& dir_1_file_with_prefix_old() const {
    return dir_1_file_with_prefix_old_;
  }
  const FilePath& dir_1_file_other() const { return dir_1_file_other_; }
  const FilePath& dir_2() const { return dir_2_; }
  const FilePath& dir_2_file_new() const { return dir_2_file_new_; }
  const FilePath& dir_2_file_old() const { return dir_2_file_old_; }
  const FilePath& dir_2_file_with_prefix_new() const {
    return dir_2_file_with_prefix_new_;
  }
  const FilePath& dir_2_file_with_prefix_old() const {
    return dir_2_file_with_prefix_old_;
  }
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

  void CreateNewFileInDirWithPrefix(const FilePath& dir, FilePath& path) {
    File file = CreateAndOpenTemporaryFileInDir(
        dir, &path, /*additional_flags=*/0, kTempFilePrefix);
    ASSERT_TRUE(file.IsValid());
  }

  void CreateOldFileInDirWithPrefix(const FilePath& dir, FilePath& path) {
    File file = CreateAndOpenTemporaryFileInDir(
        dir, &path, /*additional_flags=*/0, kTempFilePrefix);
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
  FilePath dir_1_file_with_prefix_new_;
  FilePath dir_1_file_with_prefix_old_;
  FilePath dir_1_file_other_;
  FilePath dir_2_file_new_;
  FilePath dir_2_file_old_;
  FilePath dir_2_file_with_prefix_new_;
  FilePath dir_2_file_with_prefix_old_;
  FilePath dir_2_file_other_;
  std::optional<ScopedCleanerLifetime> cleaner_lifetime_;
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

  ASSERT_NO_FATAL_FAILURE(
      CreateNewFileInDirWithPrefix(dir_1_, dir_1_file_with_prefix_new_));

  ASSERT_NO_FATAL_FAILURE(
      CreateOldFileInDirWithPrefix(dir_1_, dir_1_file_with_prefix_old_));

  dir_1_file_other_ = dir_1_.Append(FILE_PATH_LITERAL("other.nottmp"));
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_other_));

  ASSERT_NO_FATAL_FAILURE(CreateNewFileInDir(dir_2_, dir_2_file_new_));

  ASSERT_NO_FATAL_FAILURE(CreateOldFileInDir(dir_2_, dir_2_file_old_));

  ASSERT_NO_FATAL_FAILURE(
      CreateNewFileInDirWithPrefix(dir_2_, dir_2_file_with_prefix_new_));

  ASSERT_NO_FATAL_FAILURE(
      CreateOldFileInDirWithPrefix(dir_2_, dir_2_file_with_prefix_old_));

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
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that adding a directory without starting the cleaner does nothing.
TEST_F(ImportantFileWriterCleanerTest, NotStartedNoOpAdd) {
  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_TRUE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_old()));
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
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_old()));
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
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_FALSE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_2_file_with_prefix_old()));
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
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_2_file_other()));
}

// Tests that starting the cleaner twice doesn't cause it to clean twice.
TEST_F(ImportantFileWriterCleanerTest, StartTwice) {
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  // Recreate the old file that was just cleaned.
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_old()));
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_with_prefix_old()));

  // Start again and make sure it wasn't cleaned again.
  ImportantFileWriterCleaner::GetInstance().Start();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_old()));
}

// Tests that adding a dir twice doesn't cause it to clean twice.
TEST_F(ImportantFileWriterCleanerTest, AddTwice) {
  StartCleaner();
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  // Recreate the old file that was just cleaned.
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_old()));
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(dir_1_file_with_prefix_old()));

  // Add the directory again and make sure nothing else is cleaned.
  ImportantFileWriterCleaner::AddDirectory(dir_1());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_old()));
}

// Tests that AddDirectory called from another thread properly bounces back to
// the main thread for processing.
TEST_F(ImportantFileWriterCleanerTest, StartAddFromOtherThread) {
  StartCleaner();

  // Add from the ThreadPool and wait for it to finish.
  TestWaitableEvent waitable_event;
  ThreadPool::PostTask(FROM_HERE, BindLambdaForTesting([&] {
                         ImportantFileWriterCleaner::AddDirectory(dir_1());
                         waitable_event.Signal();
                       }));
  waitable_event.Wait();

  // Allow the cleaner to run.
  task_environment_.RunUntilIdle();

  // The old file should have been cleaned from the added dir.
  EXPECT_TRUE(PathExists(dir_1_file_new()));
  EXPECT_FALSE(PathExists(dir_1_file_old()));
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_TRUE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_old()));
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
  EXPECT_TRUE(PathExists(dir_1_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_1_file_with_prefix_old()));
  EXPECT_TRUE(PathExists(dir_1_file_other()));
  EXPECT_TRUE(PathExists(dir_2_file_new()));
  EXPECT_FALSE(PathExists(dir_2_file_old()));
  EXPECT_TRUE(PathExists(dir_2_file_with_prefix_new()));
  EXPECT_FALSE(PathExists(dir_2_file_with_prefix_old()));
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

// Tests that when the target file identified by a temp file's name prefix is
// missing, the most recent matching temp file is preserved as a recovery
// candidate while older matching temp files are still cleaned.
TEST_F(ImportantFileWriterCleanerTest,
       PreservesLatestPrefixedTempFileWhenTargetMissing) {
  const FilePath dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("recover_missing"));
  ASSERT_TRUE(CreateDirectory(dir));

  const Time upper_bound =
      ImportantFileWriterCleaner::GetInstance().GetUpperBoundTimeForTest();

  auto create_prefixed_temp = [&](Time mtime, FilePath* out_path) {
    File file = CreateAndOpenTemporaryFileInDir(dir, out_path,
                                                /*additional_flags=*/0,
                                                kTempFilePrefix);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.SetTimes(Time::Now(), mtime));
  };

  FilePath old_older;
  FilePath old_newer;
  // Both files are older than the cleaner's upper bound, but `old_newer` has a
  // strictly larger mtime than `old_older`.
  ASSERT_NO_FATAL_FAILURE(
      create_prefixed_temp(upper_bound - Seconds(1), &old_older));
  ASSERT_NO_FATAL_FAILURE(
      create_prefixed_temp(upper_bound - Milliseconds(1), &old_newer));

  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir);
  StartCleaner();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(old_newer));
  EXPECT_FALSE(PathExists(old_older));
}

// Tests that when the target file identified by a temp file's name prefix
// exists, all matching temp files are cleaned (no preservation happens).
TEST_F(ImportantFileWriterCleanerTest,
       DoesNotPreservePrefixedTempFilesWhenTargetExists) {
  const FilePath dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("recover_present"));
  ASSERT_TRUE(CreateDirectory(dir));

  // Create the target file so that no recovery candidate needs to be kept.
  const FilePath target = dir.Append(kTempFilePrefix);
  ASSERT_NO_FATAL_FAILURE(CreateOldFile(target));

  FilePath old_1;
  FilePath old_2;
  ASSERT_NO_FATAL_FAILURE(CreateOldFileInDirWithPrefix(dir, old_1));
  ASSERT_NO_FATAL_FAILURE(CreateOldFileInDirWithPrefix(dir, old_2));

  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir);
  StartCleaner();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(PathExists(target));
  EXPECT_FALSE(PathExists(old_1));
  EXPECT_FALSE(PathExists(old_2));
}

// Tests that preservation is done independently per name prefix: each distinct
// prefix keeps its own latest matching temp file when its target is missing.
TEST_F(ImportantFileWriterCleanerTest,
       PreservesLatestPrefixedTempFilePerPrefix) {
  static constexpr FilePath::StringViewType kPrefixA = kTempFilePrefix;
  static constexpr FilePath::StringViewType kPrefixB =
      FILE_PATH_LITERAL("Preferences");

  const FilePath dir =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("recover_multi"));
  ASSERT_TRUE(CreateDirectory(dir));

  auto create_old = [&](FilePath::StringViewType prefix, Time mtime,
                        FilePath* out_path) {
    File file = CreateAndOpenTemporaryFileInDir(dir, out_path,
                                                /*additional_flags=*/0, prefix);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.SetTimes(Time::Now(), mtime));
  };

  const Time upper_bound =
      ImportantFileWriterCleaner::GetInstance().GetUpperBoundTimeForTest();
  const Time t_older = upper_bound - Seconds(1);
  const Time t_newer = upper_bound - Milliseconds(1);

  FilePath a_older;
  FilePath a_newer;
  FilePath b_older;
  FilePath b_newer;
  ASSERT_NO_FATAL_FAILURE(create_old(kPrefixA, t_older, &a_older));
  ASSERT_NO_FATAL_FAILURE(create_old(kPrefixA, t_newer, &a_newer));
  ASSERT_NO_FATAL_FAILURE(create_old(kPrefixB, t_older, &b_older));
  ASSERT_NO_FATAL_FAILURE(create_old(kPrefixB, t_newer, &b_newer));

  ImportantFileWriterCleaner::GetInstance().Initialize();
  ImportantFileWriterCleaner::AddDirectory(dir);
  StartCleaner();
  task_environment_.RunUntilIdle();

  // Each prefix retains its own latest candidate; older ones are cleaned.
  EXPECT_FALSE(PathExists(a_older));
  EXPECT_TRUE(PathExists(a_newer));
  EXPECT_FALSE(PathExists(b_older));
  EXPECT_TRUE(PathExists(b_newer));
}

}  // namespace base
