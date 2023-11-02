// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/drive/drive_integration_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {
// Downloads a file named |file| and expects it to be saved to |dir|, which
// must be empty.
void DownloadAndVerifyFile(Browser* browser,
                           const base::FilePath& dir,
                           const base::FilePath& file) {
  net::EmbeddedTestServer embedded_test_server;
  base::FilePath test_data_directory;
  GetTestDataDirectory(&test_data_directory);
  embedded_test_server.ServeFilesFromDirectory(test_data_directory);
  ASSERT_TRUE(embedded_test_server.Start());
  content::DownloadManager* download_manager =
      browser->profile()->GetDownloadManager();
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  GURL url(embedded_test_server.GetURL("/" + file.MaybeAsASCII()));
  base::FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(base::PathExists(downloaded));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  observer.WaitForFinished();
  EXPECT_EQ(1u,
            observer.NumDownloadsSeenInState(download::DownloadItem::COMPLETE));
  EXPECT_TRUE(base::PathExists(downloaded));
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES);
  EXPECT_EQ(file, enumerator.Next().BaseName());
  EXPECT_EQ(base::FilePath(), enumerator.Next());
}

}  // namespace

// Verifies that the download directory can be forced by policy.
IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory) {
  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);

  base::FilePath initial_dir =
      DownloadPrefs(browser()->profile()).DownloadPath();

  // Verify that downloads end up on the default directory.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir, file);
  base::DieFileDie(initial_dir.Append(file), false);

  // Override the download directory with the policy and verify a download.
  base::FilePath forced_dir = initial_dir.AppendASCII("forced");

  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(forced_dir.AsUTF8Unsafe()), nullptr);
  UpdateProviderPolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir, file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(base::PathExists(initial_dir.Append(file)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verifies that the download directory can be forced to Google Drive by policy.
IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory_Drive) {
  // Override the download directory with the policy.
  {
    PolicyMap policies;
    policies.Set(key::kDownloadDirectory, POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value("${google_drive}/"), nullptr);
    UpdateProviderPolicy(policies);

    EXPECT_EQ(drive::DriveIntegrationServiceFactory::FindForProfile(
                  browser()->profile())
                  ->GetMountPointPath()
                  .AppendASCII("root"),
              DownloadPrefs(browser()->profile())
                  .DownloadPath()
                  .StripTrailingSeparators());
  }

  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value("${google_drive}/Downloads"), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(drive::DriveIntegrationServiceFactory::FindForProfile(
                browser()->profile())
                ->GetMountPointPath()
                .AppendASCII("root/Downloads"),
            DownloadPrefs(browser()->profile())
                .DownloadPath()
                .StripTrailingSeparators());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
