// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_factory.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace policy::local_user_files {

namespace {

const user_manager::User& ActiveUser() {
  return CHECK_DEREF(user_manager::UserManager::Get()->GetActiveUser());
}

}  // namespace

using FileLocationUtilsTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveGoogleDrive) {
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->GetProfile());
  ASSERT_FALSE(drive_integration_service->GetMountPointPath().empty());
  EXPECT_EQ(drive_integration_service->GetMountPointPath()
                .AppendASCII("root")
                .AppendASCII("folder"),
            ResolvePath(ActiveUser(), "${google_drive}/folder"));
  drive_integration_service->SetEnabled(false);
  EXPECT_EQ(base::FilePath(),
            ResolvePath(ActiveUser(), "${google_drive}/folder"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveODFS) {
  // Even without mounted ODFS, it should be resolved to the virtual path.
  EXPECT_EQ(GetODFSVirtualPath(),
            ResolvePath(ActiveUser(), "${microsoft_onedrive}"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveRegular) {
  EXPECT_EQ(base::FilePath("/some/path"),
            ResolvePath(ActiveUser(), "/some/path"));
}

IN_PROC_BROWSER_TEST_F(FileLocationUtilsTest, ResolveEmpty) {
  EXPECT_EQ(file_manager::util::GetDownloadsFolder(ActiveUser()),
            ResolvePath(ActiveUser(), ""));
}

}  // namespace policy::local_user_files
