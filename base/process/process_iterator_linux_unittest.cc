// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_iterator.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

void WriteFakeProcEntry(const FilePath& proc_dir,
                        ProcessId pid,
                        cstring_view comm,
                        char state,
                        ProcessId ppid,
                        ProcessId pgrp,
                        std::string_view cmdline,
                        const FilePath& exe_target = FilePath()) {
  FilePath dir = proc_dir.Append(NumberToString(pid));
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(dir.Append("stat"),
                        StringPrintf("%d (%s) %c %d %d 1 0 -1\n", pid,
                                     comm.c_str(), state, ppid, pgrp)));
  ASSERT_TRUE(WriteFile(dir.Append("cmdline"), cmdline));
  if (!exe_target.empty()) {
    ASSERT_TRUE(CreateSymbolicLink(exe_target, dir.Append("exe")));
  }
}

TEST(ProcessIteratorLinuxTest, IteratesSubstituteProcDir) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath& proc_dir = temp_dir.GetPath();

  // Entry with spaces and parentheses in comm, multiple NUL-separated args,
  // and an exe symlink.
  WriteFakeProcEntry(proc_dir, /*pid=*/100, "my (app) name", 'S', /*ppid=*/1,
                     /*pgrp=*/50, std::string_view("my_app\0--foo=bar\0", 17),
                     FilePath("/usr/bin/my_app"));
  // Entry with an empty cmdline and no exe symlink.
  WriteFakeProcEntry(proc_dir, /*pid=*/200, "worker", 'R', /*ppid=*/100,
                     /*pgrp=*/50, "");
  // Zombie processes are skipped.
  WriteFakeProcEntry(proc_dir, /*pid=*/300, "zombie", 'Z', /*ppid=*/100,
                     /*pgrp=*/50, "zombie");
  // Incomplete entries (e.g. a process that exited mid-scan) are skipped.
  FilePath missing_cmdline = proc_dir.Append("400");
  ASSERT_TRUE(CreateDirectory(missing_cmdline));
  ASSERT_TRUE(WriteFile(missing_cmdline.Append("stat"),
                        "400 (exiting) S 1 1 1 0 -1\n"));
  FilePath missing_stat = proc_dir.Append("500");
  ASSERT_TRUE(CreateDirectory(missing_stat));
  ASSERT_TRUE(WriteFile(missing_stat.Append("cmdline"), "exiting"));
  // Non-PID entries are skipped.
  ASSERT_TRUE(CreateDirectory(proc_dir.Append("self")));
  ASSERT_TRUE(WriteFile(proc_dir.Append("uptime"), "1 1\n"));

  ProcessIterator iter(/*filter=*/nullptr, proc_dir);
  ProcessIterator::ProcessEntries entries = iter.Snapshot();
  ASSERT_EQ(entries.size(), 2u);

  const ProcessEntry* entry_100 = nullptr;
  const ProcessEntry* entry_200 = nullptr;
  for (const ProcessEntry& entry : entries) {
    if (entry.pid() == 100) {
      entry_100 = &entry;
    } else if (entry.pid() == 200) {
      entry_200 = &entry;
    }
  }
  ASSERT_TRUE(entry_100);
  EXPECT_EQ(entry_100->parent_pid(), 1);
  EXPECT_EQ(entry_100->gid(), 50);
  EXPECT_STREQ(entry_100->exe_file(), "my_app");
  EXPECT_THAT(entry_100->cmd_line_args(), ElementsAre("my_app", "--foo=bar"));

  ASSERT_TRUE(entry_200);
  EXPECT_EQ(entry_200->parent_pid(), 100);
  EXPECT_EQ(entry_200->gid(), 50);
  EXPECT_STREQ(entry_200->exe_file(), "");
  EXPECT_THAT(entry_200->cmd_line_args(), IsEmpty());
}

TEST(ProcessIteratorLinuxTest, AppliesFilter) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath& proc_dir = temp_dir.GetPath();

  WriteFakeProcEntry(proc_dir, /*pid=*/10, "init_child", 'S', /*ppid=*/1,
                     /*pgrp=*/10, "a");
  WriteFakeProcEntry(proc_dir, /*pid=*/11, "grandchild", 'S', /*ppid=*/10,
                     /*pgrp=*/10, "b");
  WriteFakeProcEntry(proc_dir, /*pid=*/12, "grandchild2", 'S', /*ppid=*/10,
                     /*pgrp=*/10, "c");

  class ParentPidFilter : public ProcessFilter {
   public:
    explicit ParentPidFilter(ProcessId ppid) : ppid_(ppid) {}
    bool Includes(const ProcessEntry& entry) const override {
      return entry.parent_pid() == ppid_;
    }

   private:
    ProcessId ppid_;
  };

  ParentPidFilter filter(10);
  ProcessIterator iter(&filter, proc_dir);
  std::vector<ProcessId> pids;
  while (const ProcessEntry* entry = iter.NextProcessEntry()) {
    pids.push_back(entry->pid());
  }
  EXPECT_THAT(pids, UnorderedElementsAre(11, 12));
}

TEST(ProcessIteratorLinuxTest, MissingProcDir) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ProcessIterator iter(/*filter=*/nullptr,
                       temp_dir.GetPath().Append("nonexistent"));
  EXPECT_THAT(iter.Snapshot(), IsEmpty());
}

}  // namespace
}  // namespace base
