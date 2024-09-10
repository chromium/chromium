// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using FileLocationUtilsTest = InProcessBrowserTest;

namespace policy::local_user_files {

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveGoogleDrive) {
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());
  ASSERT_FALSE(drive_integration_service->GetMountPointPath().empty());
  EXPECT_EQ(drive_integration_service->GetMountPointPath()
                .AppendASCII("root")
                .AppendASCII("folder"),
            ResolvePath("${google_drive}/folder"));
  drive_integration_service->SetEnabled(false);
  EXPECT_EQ(base::FilePath(), ResolvePath("${google_drive}/folder"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveODFS) {
  // Even without mounted ODFS, it should be resolved to the virtual path.
  EXPECT_EQ(GetODFSVirtualPath(), ResolvePath("${microsoft_onedrive}"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveRegular) {
  EXPECT_EQ(base::FilePath("/some/path"), ResolvePath("/some/path"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveEmpty) {
  EXPECT_EQ(
      file_manager::util::GetDownloadsFolderForProfile(browser()->profile()),
      ResolvePath(""));
}

}  // namespace policy::local_user_files
