// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

class LaunchUtilsTest : public testing::Test {
 protected:
  apps::AppLaunchParams CreateLaunchParams(
      apps::mojom::LaunchContainer container,
      WindowOpenDisposition disposition,
      bool preferred_container,
      apps::mojom::LaunchContainer fallback_container =
          apps::mojom::LaunchContainer::kLaunchContainerNone) {
    return apps::CreateAppIdLaunchParamsWithEventFlags(
        app_id,
        apps::GetEventFlags(container, disposition, preferred_container),
        apps::mojom::AppLaunchSource::kSourceChromeInternal,
        display::kInvalidDisplayId, fallback_container);
  }

  std::string app_id = "aaa";
};

TEST_F(LaunchUtilsTest, WindowContainerAndWindowDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndForegoundTabDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndBackgoundTabDisposition) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithTab) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto preferred_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithWindow) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto preferred_container =
      apps::mojom::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB, params.disposition);
}

TEST_F(LaunchUtilsTest, UseIntentFullUrlInLaunchParams) {
  auto container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;

  const GURL url = GURL("https://example.com/?query=1#frag");
  auto intent = apps_util::CreateIntentFromUrl(url);

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, apps::GetEventFlags(container, disposition, true),
      apps::mojom::AppLaunchSource::kSourceIntentUrl,
      display::kInvalidDisplayId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow, std::move(intent));

  EXPECT_EQ(url, params.override_url);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoAppID) {
  // Validate an empty vector is returned if there is
  // no AppID specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoFiles) {
  // Validate an empty vector is returned if there are
  // no files specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_SingleFile) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_MultipleFiles) {
  // Validate a vector with size 2 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  command_line.AppendArg("filename2");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 2U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
  EXPECT_EQ(launch_files[1], base::FilePath(FILE_PATH_LITERAL("filename2")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_FileProtocol) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter. This uses
  // the file protocol to reference the file.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("file://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0],
            base::FilePath(FILE_PATH_LITERAL("file://filename")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_CustomProtocol) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter. This uses
  // a test protocol.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("web+test://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0],
            base::FilePath(FILE_PATH_LITERAL("web+test://filename")));
}
