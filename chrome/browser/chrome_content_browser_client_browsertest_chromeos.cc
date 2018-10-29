// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/common/process_type.h"
#include "ui/base/ui_base_features.h"

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;

namespace {

void GetUtilityProcessPidsOnIOThread(std::vector<pid_t>* pids) {
  for (BrowserChildProcessHostIterator it(content::PROCESS_TYPE_UTILITY);
       !it.Done(); ++it) {
    pid_t pid = it.GetData().GetHandle();
    pids->push_back(pid);
  }
}

std::string ReadCmdLine(pid_t pid) {
  // Files in "/proc" are in-memory, so it's safe to do IO.
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath cmdline_file =
      base::FilePath("/proc").Append(base::IntToString(pid)).Append("cmdline");
  std::string cmdline;
  base::ReadFileToString(cmdline_file, &cmdline);
  return cmdline;
}

// We don't seem to have a string utility or STL utility for this.
bool HasSubstring(base::StringPiece str, base::StringPiece sub) {
  return str.find(sub) != base::StringPiece::npos;
}

}  // namespace

using ChromeContentBrowserClientMashTest = InProcessBrowserTest;

// Verifies that mash service child processes use in-process breakpad crash
// dumping.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientMashTest, CrashReporter) {
  // Test only applies to out-of-process.
  if (!features::IsMultiProcessMash())
    return;

  // Child process management lives on the IO thread.
  std::vector<pid_t> pids;
  base::RunLoop loop;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&GetUtilityProcessPidsOnIOThread, &pids), loop.QuitClosure());
  loop.Run();
  ASSERT_FALSE(pids.empty());

  // Iterate through all utility processes looking for mash services. We are
  // guaranteed at least ash and mus have launched because we block during
  // startup waiting for mus.
  int mash_service_count = 0;
  for (pid_t pid : pids) {
    std::string cmdline = ReadCmdLine(pid);
    ASSERT_FALSE(cmdline.empty());
    // Subprocess command lines may have their null separators replaced with
    // spaces, which makes them hard to tokenize. Just search the whole string.
    if (HasSubstring(cmdline, switches::kMashServiceName)) {
      ++mash_service_count;
      // Mash services use in-process breakpad crash dumping.
      EXPECT_TRUE(HasSubstring(cmdline, switches::kEnableCrashReporter));
    }
  }
  // There's at least one mash service, for ash.
  EXPECT_GT(mash_service_count, 0);
}
