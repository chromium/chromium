// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/ash/system/procfs_util.h"

namespace ash {
namespace system {

class ProcfsUtilTest : public testing::Test {
 public:
  ProcfsUtilTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    slash_proc_ = temp_dir_.GetPath().Append("proc");
    CHECK(base::CreateDirectory(slash_proc_));
  }

  ProcfsUtilTest(const ProcfsUtilTest&) = delete;
  ProcfsUtilTest& operator=(const ProcfsUtilTest&) = delete;

  ~ProcfsUtilTest() override = default;

  // Write |contents| to file |file_name| under /proc.
  void WriteContentsToFile(const std::string& contents,
                           const std::string& file_name) {
    base::FilePath file_path = slash_proc_.Append(file_name);
    EXPECT_TRUE(base::WriteFile(file_path, contents));
  }

  // Create a directory |dir_name| under /proc and write |contents| to file
  // |file_name| under this directory.
  void WriteContentsToFileUnderSubdir(const std::string& contents,
                                      const std::string& dir_name,
                                      const std::string& file_name) {
    base::FilePath dir = slash_proc_.Append(dir_name);
    CHECK(base::CreateDirectory(dir));
    base::FilePath file = dir.Append(file_name);
    EXPECT_TRUE(base::WriteFile(file, contents));
  }

 protected:
  base::FilePath slash_proc_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ProcfsUtilTest, GetSingleProcStatSuccess) {
  pid_t pid = 201664;
  std::string stat_contents =
      "201664 (cinnamon) R 8052 7598 7598 0 -1 4194304 534106 9235 13 7 466410 "
      "80831 119 33 20 0 6 0 51830217 2267480064 130441 184467440737095    "
      "51615 93909892808704 93909892818200 140737448703360 0 0 0 0 16781312 "
      "16384 0 0 0 17 17 0 0 1 0 0 93909894917032 93909894918272 939099184    "
      "98816 140737448704343 140737448704372 140737448704372 140737448706022 0";
  SingleProcStat expected = {.pid = pid,
                             .name = "cinnamon",
                             .ppid = 8052,
                             .utime = 466410,
                             .stime = 80831,
                             .rss = 130441};
  WriteContentsToFileUnderSubdir(stat_contents, base::NumberToString(pid),
                                 "stat");
  EXPECT_EQ(expected,
            GetSingleProcStat(
                slash_proc_.Append(base::NumberToString(pid)).Append("stat"))
                .value());
}

TEST_F(ProcfsUtilTest, GetCpuTimeJiffiesSuccess) {
  std::string contents =
      "cpu  107994940 2443611 104428507 5684138545 584527 0 738084 0 0 0\n"
      "cpu0 18429992 58198 36166540 67167135 4456 0 301562 0 0 0\n"
      "cpu1 1703401 63770 633494 119688031 10157 0 231948 0 0 0\n"
      "cpu2 1663060 53891 869722 120301343 6427 0 36396 0 0 0\n"
      "cpu3 1589143 42113 800045 120506135 10204 0 10237 0 0 0\n"
      "cpu4 1520958 51244 723693 120652995 11833 0 5132 0 0 0\n";
  int64_t expected =
      107994940 + 2443611 + 104428507 + 5684138545 + 584527 + 0 + 738084 + 0;
  WriteContentsToFile(contents, "stat");
  EXPECT_EQ(expected, GetCpuTimeJiffies(slash_proc_.Append("stat")).value());
}

}  // namespace system
}  // namespace ash
