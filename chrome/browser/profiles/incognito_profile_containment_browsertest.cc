// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// List of file or directory prefixes that are known to be modified during an
// Incognito session.
// TODO(http://crbug.com/1234755): Audit why these files are changed.
constexpr std::array<const char*, 7> kAllowListPrefixesForAllPlatforms = {
    "/Default/GCM Store/",      "/Default/Network Action Predictor",
    "/Default/PreferredApps",   "/Default/Reporting and NEL",
    "/Default/shared_proto_db", "/Default/Trust Tokens",
    "/GrShaderCache/GPUCache"};
#if !defined(OS_MAC)
constexpr std::array<const char*, 4> kAllowListPrefixesForMac = {
    "/Default/data_reduction_proxy_leveldb", "/Default/Preferences",
    "/Default/Shortcuts", "/Default/Visited Links"};
#endif

// Structure that keeps data about a snapshotted file.
struct FileData {
  base::FilePath full_path;
  base::Time last_modified_time;
  int64_t size = 0;
  bool file_hash_is_valid = false;
  uint32_t file_hash = 0;
};

using Snapshot = std::map<std::string, FileData>;

bool ComputeFileHash(const base::FilePath& file_path, uint32_t* hash_code) {
  std::string content;
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (!base::ReadFileToString(file_path, &content))
    return false;
  *hash_code = base::Hash(content);
  return true;
}

void GetUserDirectorySnapshot(Snapshot& snapshot, bool compute_file_hashes) {
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator enumerator(
      user_data_dir, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // Remove |user_data_dir| part from path.
    std::string file =
        path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe().substr(
            user_data_dir.AsUTF8Unsafe().length());

    FileData fd;
    // TODO(http://crbug.com/1234755): Expand to newly added empty directories.
    if (!enumerator.GetInfo().IsDirectory()) {
      fd.size = enumerator.GetInfo().GetSize();
      fd.last_modified_time = enumerator.GetInfo().GetLastModifiedTime();
      fd.file_hash_is_valid =
          compute_file_hashes ? ComputeFileHash(path, &fd.file_hash) : false;
      fd.full_path = path;
      snapshot[file] = fd;
    }
  }
  return;
}

bool IsFileModified(FileData& before, FileData& after) {
  // TODO(http://crbug.com/1234755): Also consider auditing files that are
  // touched or are unreadable.
  // If it was readable before, and is readable now, compare hash codes.
  if (before.file_hash_is_valid) {
    uint32_t hash_code;
    if (!ComputeFileHash(after.full_path, &hash_code))
      return false;

    return hash_code == before.file_hash;
  }

  return false;
}

bool IsDiskStateModified(Snapshot& snapshot_before,
                         Snapshot& snapshot_after,
                         std::set<const char*>& allow_list) {
  bool modified = false;
  // TODO(http://crbug.com/1234755): Consider deleted files as well. Currently
  // we only look for added and modified files, but file deletion is also
  // modifying disk and is best to be prevented.
  for (auto& fd : snapshot_after) {
    auto before = snapshot_before.find(fd.first);
    bool is_new = (before == snapshot_before.end());
    if (is_new ||
        fd.second.last_modified_time != before->second.last_modified_time) {
      // Ignore allow-listed paths.
      if (std::any_of(allow_list.begin(), allow_list.end(),
                      [&fd](const char* prefix) {
                        return fd.first.find(prefix) == 0;
                      })) {
        continue;
      }

      // If an empty file is added or modified, ignore for now.
      // TODO(http://crbug.com/1234755): Consider newly added empty files.
      if (!fd.second.size)
        continue;

      // If data content is not changed, it can be ignored.
      if (IsFileModified(before->second, fd.second))
        continue;

      modified = true;

      LOG(ERROR) << (is_new ? "New" : "Modified") << " File " << fd.first
                 << std::string(" - Size: ") +
                        base::NumberToString(fd.second.size);
    }
  }
  return modified;
}

}  // namespace

class IncognitoProfileContainmentBrowserTest : public InProcessBrowserTest {
 public:
  IncognitoProfileContainmentBrowserTest()
      : allow_list_(std::begin(kAllowListPrefixesForAllPlatforms),
                    std::end(kAllowListPrefixesForAllPlatforms)) {
#if !defined(OS_MAC)
    allow_list_.insert(std::begin(kAllowListPrefixesForMac),
                       std::end(kAllowListPrefixesForMac));
#endif
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }

 protected:
  std::set<const char*> allow_list_;
};

// TODO(http://crbug.com/1234755): There is a lot of clutter on Windows (and a
// bit on CrOS) that need more investigation before enabling this test there.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_PRE_SimplePageLoadDoesNotModifyProfileFolder \
  DISABLED_PRE_SimplePageLoadDoesNotModifyProfileFolder
#define MAYBE_SimplePageLoadDoesNotModifyProfileFolder \
  DISABLED_SimplePageLoadDoesNotModifyProfileFolder
#else
#define MAYBE_PRE_SimplePageLoadDoesNotModifyProfileFolder \
  PRE_SimplePageLoadDoesNotModifyProfileFolder
#define MAYBE_SimplePageLoadDoesNotModifyProfileFolder \
  SimplePageLoadDoesNotModifyProfileFolder
#endif

// Open a page in a separate session to ensure all files that are created
// because of the regular profile start up are already created.
IN_PROC_BROWSER_TEST_F(IncognitoProfileContainmentBrowserTest,
                       MAYBE_PRE_SimplePageLoadDoesNotModifyProfileFolder) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/empty.html"));
}

// Test that Opening a simple page in Incognito does not modify regular profile
// directory.
// If you are storing from a "regular" (non off-the-record) profile and your CL
// breaks this test, please first check if it is intended to change profile
// state even if user did not explicitly open the browser in regular mode and if
// so, please add the file to the allow_list at the top and file a bug to follow
// up.
IN_PROC_BROWSER_TEST_F(IncognitoProfileContainmentBrowserTest,
                       MAYBE_SimplePageLoadDoesNotModifyProfileFolder) {
  // Take a snapshot of regular profile.
  Snapshot before_incognito;
  GetUserDirectorySnapshot(before_incognito, /*compute_file_hashes=*/true);

  // Run an Incognito session.
  Browser* browser = chrome::FindLastActive();
  EXPECT_TRUE(browser->profile()->IsOffTheRecord());
  ui_test_utils::NavigateToURL(browser,
                               embedded_test_server()->GetURL("/hello.html"));
  CloseBrowserSynchronously(browser);

  // Take another snapshot of regular profile and ensure it is not changed.
  // Do not compute file content hashes for faster processing. They would be
  // computed only if needed.
  Snapshot after_incognito;
  GetUserDirectorySnapshot(after_incognito, /*compute_file_hashes=*/false);
  EXPECT_FALSE(
      IsDiskStateModified(before_incognito, after_incognito, allow_list_));
}

// TODO(http://crbug.com/1234755): Add more complex naviagtions, triggering
// different APIs in "browsing_data/site_data.html" and more.
