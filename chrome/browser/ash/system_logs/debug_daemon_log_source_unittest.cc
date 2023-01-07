// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

constexpr char kNotAvailable[] = "<not available>";

class DebugDaemonLogSourceTest : public ::testing::Test {};

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFile) {
  constexpr char kLogContent[] = "logloglog\n";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath temp_dir_path = temp_dir.GetPath();
  ASSERT_TRUE(CreateDirectory(temp_dir_path.AppendASCII("dir")));

  // Prepare a directory and log file in it.
  base::FilePath log_file("dir/log.txt");
  base::FilePath log_path = temp_dir_path.Append(log_file);
  ASSERT_TRUE(base::WriteFile(log_path, kLogContent));

  auto result = ReadUserLogFile(log_path);
  EXPECT_EQ(kLogContent, result);
}

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFile_FileNotExisting) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Prepare a directory but no log file in it.
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  ASSERT_TRUE(CreateDirectory(temp_dir_path.AppendASCII("dir")));

  base::FilePath log_file("dir/log.txt");
  base::FilePath log_path = temp_dir_path.Append(log_file);
  ASSERT_FALSE(base::PathExists(log_path));

  auto result = ReadUserLogFile(log_path);
  EXPECT_EQ(kNotAvailable, result);
}

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFile_DirectoryNotExisting) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Prepare neither directory nor log file.
  base::FilePath log_file("dir/log.txt");
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  base::FilePath log_path = temp_dir_path.Append(log_file);
  ASSERT_FALSE(base::PathExists(log_path));

  auto result = ReadUserLogFile(log_path);
  EXPECT_EQ(kNotAvailable, result);
}

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFilePattern) {
  constexpr char kLog1Content[] = "logloglog #1\n";
  constexpr char kLog2Content[] = "logloglog #2\n";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Prepare a directory.
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  ASSERT_TRUE(CreateDirectory(temp_dir_path.AppendASCII("dir")));

  // Prepare an older log file with mtime of yesterday.
  base::FilePath log_path_not_latest =
      temp_dir_path.AppendASCII("dir/log1.txt");
  ASSERT_TRUE(base::WriteFile(log_path_not_latest, kLog1Content));
  base::Time yesterday = base::Time::Now() - base::Days(1);
  ASSERT_TRUE(base::File(log_path_not_latest,
                         base::File::FLAG_OPEN | base::File::FLAG_READ)
                  .SetTimes(yesterday, yesterday));

  // Prepare a newer log file with mtime of now.
  base::FilePath log_path_latest = temp_dir_path.AppendASCII("dir/log2.txt");
  ASSERT_TRUE(base::WriteFile(log_path_latest, kLog2Content));

  base::FilePath log_path_pattern("dir/log?.txt");
  auto result = ReadUserLogFilePattern(temp_dir_path.Append(log_path_pattern));

  // Confirm that the newer log file is detected.
  EXPECT_EQ(kLog2Content, result);
}

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFilePattern_FileNotExists) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Prepare a directory but no log file in it.
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  ASSERT_TRUE(CreateDirectory(temp_dir_path.AppendASCII("dir")));

  base::FilePath log_path_pattern("dir/log?.txt");
  auto result = ReadUserLogFilePattern(temp_dir_path.Append(log_path_pattern));
  EXPECT_EQ(kNotAvailable, result);
}

TEST_F(DebugDaemonLogSourceTest, ReadUserLogFilePattern_DirectoryNotExists) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Prepare neither directory nor log file.
  base::FilePath log_path_pattern("dir/log?.txt");
  const base::FilePath temp_dir_path = temp_dir.GetPath();
  auto result = ReadUserLogFilePattern(temp_dir_path.Append(log_path_pattern));
  EXPECT_EQ(kNotAvailable, result);
}

}  // namespace system_logs
