// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {

constexpr char kExampleUrl[] = "https://example1.com";

}  // namespace

class FilesPolicyNotificationManagerBrowserTest : public InProcessBrowserTest {
 public:
  FilesPolicyNotificationManagerBrowserTest() = default;
  FilesPolicyNotificationManagerBrowserTest(
      const FilesPolicyNotificationManagerBrowserTest&) = delete;
  FilesPolicyNotificationManagerBrowserTest& operator=(
      const FilesPolicyNotificationManagerBrowserTest&) = delete;
  ~FilesPolicyNotificationManagerBrowserTest() override = default;
};

// (b/273269211): This is a test for the crash that happens upon showing a
// warning dialog when a file is moved to Google Drive.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_ComponentDestination) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<DlpConfidentialFile> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), std::move(warning_files),
                       DlpFileDestination(data_controls::Component::kDrive),
                       dlp::FileAction::kMove);
}

// (b/277594200): This is a test for the crash that happens upon showing a
// warning dialog when a file is dragged to a webpage.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_UrlDestination) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<DlpConfidentialFile> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), std::move(warning_files),
                       DlpFileDestination(kExampleUrl), dlp::FileAction::kMove);
}

// (b/281495499): This is a test for the crash that happens upon showing a
// warning dialog for downloads.
IN_PROC_BROWSER_TEST_F(FilesPolicyNotificationManagerBrowserTest,
                       WarningDialog_Download) {
  auto* fpnm = FilesPolicyNotificationManagerFactory::GetForBrowserContext(
      browser()->profile());
  ASSERT_TRUE(fpnm);
  std::vector<DlpConfidentialFile> warning_files;
  warning_files.emplace_back(base::FilePath("file1.txt"));
  fpnm->ShowDlpWarning(base::DoNothing(), std::move(warning_files),
                       DlpFileDestination(data_controls::Component::kDrive),
                       dlp::FileAction::kDownload);
}

}  // namespace policy
