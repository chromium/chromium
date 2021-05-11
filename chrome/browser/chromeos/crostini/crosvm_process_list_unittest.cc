// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/chromeos/crostini/crosvm_process_list.h"

namespace crostini {

class CrosvmProcessListTest : public testing::Test {
 public:
  CrosvmProcessListTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    slash_proc_ = temp_dir_.GetPath().Append("proc");
    CHECK(base::CreateDirectory(slash_proc_));
  }
  ~CrosvmProcessListTest() override = default;

  // Create a directory |dir_name| under /proc and write |contents| to file
  // |file_name| under this directory.
  void WriteContentsToFileUnderSubdir(const std::string& contents,
                                      const std::string& dir_name,
                                      const std::string& file_name) {
    base::FilePath dir = slash_proc_.Append(dir_name);
    CHECK(base::CreateDirectory(dir));
    base::FilePath file = dir.Append(file_name);
    EXPECT_EQ(static_cast<int>(contents.size()),
              base::WriteFile(file, contents.c_str(), contents.size()));
  }

 protected:
  base::FilePath slash_proc_;

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(CrosvmProcessListTest);
};

TEST_F(CrosvmProcessListTest, ConciergeIsTheOnlyCrosvmProcess) {
  pid_t concierge_pid = 7404;
  std::string stat_contents =
      "7404 (vm_concierge) S 1 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  PidStatMap expected = {{concierge_pid,
                          {.pid = concierge_pid,
                           .name = "vm_concierge",
                           .ppid = 1,
                           .utime = 1489,
                           .stime = 1333,
                           .rss = 3149}}};
  WriteContentsToFileUnderSubdir(stat_contents,
                                 base::NumberToString(concierge_pid), "stat");
  EXPECT_EQ(expected, GetCrosvmPidStatMap(slash_proc_));
}

TEST_F(CrosvmProcessListTest, ConciergeNotRunning) {
  EXPECT_TRUE(GetCrosvmPidStatMap(slash_proc_).empty());
}

TEST_F(CrosvmProcessListTest, SkipOtherDirAndFile) {
  base::FilePath other_dir = slash_proc_.Append("other_dir");
  CHECK(base::CreateDirectory(other_dir));

  WriteContentsToFileUnderSubdir("other contents", "other_dir", "other_file");
  EXPECT_TRUE(GetCrosvmPidStatMap(slash_proc_).empty());
}

TEST_F(CrosvmProcessListTest, SkipOtherProcess) {
  pid_t concierge_pid = 7404;
  std::string stat_contents =
      "7404 (vm_concierge) S 1 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  WriteContentsToFileUnderSubdir(stat_contents,
                                 base::NumberToString(concierge_pid), "stat");

  pid_t other_pid = 1111;
  std::string other_stat_contents =
      "1111 (other) S 1 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  WriteContentsToFileUnderSubdir(other_stat_contents,
                                 base::NumberToString(other_pid), "stat");

  PidStatMap expected = {{concierge_pid,
                          {.pid = concierge_pid,
                           .name = "vm_concierge",
                           .ppid = 1,
                           .utime = 1489,
                           .stime = 1333,
                           .rss = 3149}}};
  EXPECT_EQ(expected, GetCrosvmPidStatMap(slash_proc_));
}

TEST_F(CrosvmProcessListTest, ChildrenAreIncluded) {
  pid_t concierge_pid = 2222;
  std::string stat_contents =
      "2222 (vm_concierge) S 1 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  WriteContentsToFileUnderSubdir(stat_contents,
                                 base::NumberToString(concierge_pid), "stat");

  pid_t child_pid = 3333;
  std::string child_stat_contents =
      "3333 (child) S 2222 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  WriteContentsToFileUnderSubdir(child_stat_contents,
                                 base::NumberToString(child_pid), "stat");

  pid_t grand_child_pid = 4444;
  std::string grand_child_stat_contents =
      "4444 (grand_child) S 3333 7403 7403 0 -1 4210944 854 0 0 0 1489 1333 0 "
      "0 "
      "20 0 4 0 67733375 271028224 3149 18446744073709551615 96193202642944 "
      "96193203221904 140726650459504 140726650457840 134557843346867 0 81920 "
      "0 0 0 0 0 17 2 0 0 0 0 0 96193203229904 96193203257392 96193216815104 "
      "140726650461974 140726650461996 140726650461996 140726650462178 0";
  WriteContentsToFileUnderSubdir(grand_child_stat_contents,
                                 base::NumberToString(grand_child_pid), "stat");

  PidStatMap expected = {{concierge_pid,
                          {.pid = concierge_pid,
                           .name = "vm_concierge",
                           .ppid = 1,
                           .utime = 1489,
                           .stime = 1333,
                           .rss = 3149}},
                         {child_pid,
                          {.pid = 3333,
                           .name = "child",
                           .ppid = concierge_pid,
                           .utime = 1489,
                           .stime = 1333,
                           .rss = 3149}},
                         {grand_child_pid,
                          {.pid = 4444,
                           .name = "grand_child",
                           .ppid = child_pid,
                           .utime = 1489,
                           .stime = 1333,
                           .rss = 3149}}};
  EXPECT_EQ(expected, GetCrosvmPidStatMap(slash_proc_));
}
}  // namespace crostini
