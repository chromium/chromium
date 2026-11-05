// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/internal_linux.h"

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal {
namespace {

TEST(InternalLinuxTest, GetProcPidDir) {
  EXPECT_EQ(GetProcPidDir(123), FilePath("/proc/123"));

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_EQ(GetProcPidDir(123, temp_dir.GetPath()),
            temp_dir.GetPath().Append("123"));
}

TEST(InternalLinuxTest, ReadProcStatsSubstituteProcDir) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath& proc_dir = temp_dir.GetPath();

  FilePath valid_dir = GetProcPidDir(100, proc_dir);
  ASSERT_TRUE(CreateDirectory(valid_dir));
  constexpr std::string_view kStatContents = "100 (my_app) S 1 50 1 0 -1\n";
  ASSERT_TRUE(WriteFile(valid_dir.Append("stat"), kStatContents));

  std::string buffer;
  ASSERT_TRUE(ReadProcStats(100, proc_dir, &buffer));
  EXPECT_EQ(buffer, kStatContents);

  // Empty stat file returns false and clears `buffer`.
  FilePath empty_dir = GetProcPidDir(200, proc_dir);
  ASSERT_TRUE(CreateDirectory(empty_dir));
  ASSERT_TRUE(WriteFile(empty_dir.Append("stat"), ""));
  EXPECT_FALSE(ReadProcStats(200, proc_dir, &buffer));
  EXPECT_TRUE(buffer.empty());

  // Missing entry returns false.
  EXPECT_FALSE(ReadProcStats(300, proc_dir, &buffer));
}

}  // namespace
}  // namespace base::internal
