// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
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
// For ChromeOS, a copy of all members of |kAllowListPrefixesForAllPlatforms|
// that start with "/Default" is added to the allow list, replacing "/Default"
// with "/test-user".
// TODO(http://crbug.com/1234755): Add audit comment (or fix the issue) for all
// paths that do not have a comment.
const char* kAllowListPrefixesForAllPlatforms[] = {
    "/Default/data_reduction_proxy_leveldb",
    "/Default/Extension State",
    "/Default/GCM Store/",
    "/Default/Network Action Predictor",
    "/Default/Preferences",
    "/Default/PreferredApps",
    "/Default/Reporting and NEL",
    "/Default/shared_proto_db",
    "/Default/Trust Tokens",
    "/Default/Shortcuts",
    "/GrShaderCache/GPUCache",
    "/Local State"};
#if BUILDFLAG(IS_MAC)
const char* kAllowListPrefixesForPlatform[] = {"/Default/Visited Links"};
#elif BUILDFLAG(IS_WIN)
const char* kAllowListPrefixesForPlatform[] = {
    "/Default/databases-off-the-record",
    "/Default/heavy_ad_intervention_opt_out.db", "/Default/Top Sites",
    "/GrShaderCache/old_GPUCache",

    // This file only contains the path to the latest executable of Chrome,
    // therefore it's safe to be written in Incognito.
    "/Last Browser"};
#elif BUILDFLAG(IS_CHROMEOS)
const char* kAllowListPrefixesForPlatform[] = {
    "/Default/Local Storage/leveldb/CURRENT",
    "/Default/Site Characteristics Database", "/Default/Sync Data/LevelDB",
    "/test-user/.variations-list.txt"};
#elif BUILDFLAG(IS_LINUX)
const char* kAllowListPrefixesForPlatform[] = {"/Default/Web Data"};
#else
const char* kAllowListPrefixesForPlatform[] = {};
#endif

// List of directory prefixes that are known to be added as an empty directory
// during an Incognito session.
// TODO(http://crbug.com/1234755): Add audit comment (or fix the issue) for all
// paths that do not have a comment.
const char* kAllowListEmptyDirectoryPrefixesForAllPlatforms[] = {
    "/Default/AutofillStrikeDatabase",
    "/Default/Download Service",
    "/Default/Feature Engagement Tracker",
    "/Default/GCM Store/Encryption",
    "/Default/optimization_guide_hint_cache_store",
    "/Default/optimization_guide_model_and_features_store",
    "/Default/shared_proto_db/metadata",
    "/test-user"};

// Structure that keeps data about a snapshotted file.
struct FileData {
  base::FilePath full_path;
  base::Time last_modified_time;
  int64_t size = 0;
  bool file_hash_is_valid = false;
  uint32_t file_hash = 0;
};

struct Snapshot {
  std::unordered_map<std::string, FileData> files;
  std::unordered_set<std::string> directories;
};

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
    std::string reduced_path =
        path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe().substr(
            user_data_dir.AsUTF8Unsafe().length());

    if (enumerator.GetInfo().IsDirectory()) {
      snapshot.directories.insert(reduced_path);
    } else {
      FileData fd;
      fd.size = enumerator.GetInfo().GetSize();
      fd.last_modified_time = enumerator.GetInfo().GetLastModifiedTime();
      fd.file_hash_is_valid =
          compute_file_hashes ? ComputeFileHash(path, &fd.file_hash) : false;
      fd.full_path = path;
      snapshot.files[reduced_path] = fd;
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

    return hash_code != before.file_hash;
  }

  return false;
}

bool AreDirectoriesModified(Snapshot& snapshot_before,
                            Snapshot& snapshot_after,
                            std::set<std::string>& allow_list) {
  bool modified = false;

  // Check for new directories.
  for (const std::string& directory : snapshot_after.directories) {
    if (!base::Contains(snapshot_before.directories, directory)) {
      // If a file/prefix in this directory is allowlisted, ignore directory
      // addition.
      if (base::ranges::any_of(allow_list,
                               [&directory](const std::string& prefix) {
                                 return prefix.find(directory) == 0;
                               })) {
        continue;
      }

      // If directory is specifically allow list, ignore.
      if (base::ranges::any_of(
              kAllowListEmptyDirectoryPrefixesForAllPlatforms,
              [&directory](const std::string& allow_listed_directory) {
                return directory.find(allow_listed_directory) == 0;
              })) {
        continue;
      }

      LOG(ERROR) << "New directory: " << directory;
      modified = true;
    }
  }

  return modified;
}

bool AreFilesModified(Snapshot& snapshot_before,
                      Snapshot& snapshot_after,
                      std::set<std::string>& allow_list) {
  bool modified = false;

  // TODO(http://crbug.com/1234755): Consider deleted files as well. Currently
  // we only look for added and modified files, but file deletion is also
  // modifying disk and is best to be prevented.
  for (auto& fd : snapshot_after.files) {
    auto before = snapshot_before.files.find(fd.first);
    bool is_new = (before == snapshot_before.files.end());
    if (is_new ||
        fd.second.last_modified_time != before->second.last_modified_time) {
      // Ignore allow-listed paths.
      if (base::ranges::any_of(allow_list, [&fd](const std::string& prefix) {
            return fd.first.find(prefix) == 0;
          })) {
        continue;
      }

      // If an empty file is added or modified, ignore for now.
      // TODO(http://crbug.com/1234755): Consider newly added empty files.
      if (!fd.second.size)
        continue;

      // If data content is not changed, it can be ignored.
      if (!is_new && !IsFileModified(before->second, fd.second))
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
#if BUILDFLAG(IS_CHROMEOS)
    // These prefixes are allowed twice, once under "Default" and once under
    // "test-user".
    std::set<std::string> test_folder;
    const int offset = strlen("/Default");
    for (std::string prefix : allow_list_) {
      if (prefix.find("/Default/") == 0) {
        test_folder.insert(std::string("/test-user") + prefix.substr(offset));
      }
    }
    allow_list_.insert(test_folder.begin(), test_folder.end());
#endif

    for (const char* platform_prefix : kAllowListPrefixesForPlatform) {
      allow_list_.emplace(platform_prefix);
    }
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
  std::set<std::string> allow_list_;
};

// Open a page in a separate session to ensure all files that are created
// because of the regular profile start up are already created.
IN_PROC_BROWSER_TEST_F(IncognitoProfileContainmentBrowserTest,
                       PRE_StoringDataDoesNotModifyProfileFolder) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
}

// Test that calling several data storage APIs does not modify regular profile
// directory.
// If you are storing from a "regular" (non off-the-record) profile and your CL
// breaks this test, please first check if it is intended to change profile
// state even if user did not explicitly open the browser in regular mode and if
// so, please add the file to the allow_list at the top and file a bug to follow
// up.
// TODO(crbug.com/40809832): Flakes on Win 7.
IN_PROC_BROWSER_TEST_F(IncognitoProfileContainmentBrowserTest,
                       DISABLED_StoringDataDoesNotModifyProfileFolder) {
  // Take a snapshot of regular profile.
  Snapshot before_incognito;
  GetUserDirectorySnapshot(before_incognito, /*compute_file_hashes=*/true);

  // Run an Incognito session.
  Browser* browser = chrome::FindLastActive();
  EXPECT_TRUE(browser->profile()->IsOffTheRecord());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser,
      embedded_test_server()->GetURL("/browsing_data/site_data.html")));

  const std::vector<std::string> kStorageTypes{
      "CacheStorage", "Cookie",        "FileSystem",    "IndexedDb",
      "LocalStorage", "ServiceWorker", "SessionCookie", "WebSql"};

  for (const std::string& type : kStorageTypes) {
    ASSERT_TRUE(
        content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                        "set" + type + "()")
            .ExtractBool())
        << "Couldn't create data for: " << type;
  }

  CloseBrowserSynchronously(browser);

  // Take another snapshot of regular profile and ensure it is not changed.
  // Do not compute file content hashes for faster processing. They would be
  // computed only if needed.
  Snapshot after_incognito;
  GetUserDirectorySnapshot(after_incognito, /*compute_file_hashes=*/false);
  EXPECT_FALSE(
      AreFilesModified(before_incognito, after_incognito, allow_list_));

  // TODO(http://crbug.com/1234755): Change to EXPECT_FALSE.
  if (AreDirectoriesModified(before_incognito, after_incognito, allow_list_)) {
    LOG(ERROR) << "Empty directories added.";
  }
}

// TODO(http://crbug.com/1234755): Add more complex naviagtions, triggering
// different APIs in "browsing_data/site_data.html" and more.
