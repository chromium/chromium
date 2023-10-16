// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class NotificationPlatformBridgeTest
    : public testing::TestWithParam<base::FilePath> {};

TEST_P(NotificationPlatformBridgeTest, ProfileIdConversion) {
  content::BrowserTaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath profile_basename = GetParam();

  // Create a profile.
  TestingProfile::Builder profile_builder;
  profile_builder.SetPath(temp_dir.GetPath().Append(profile_basename));
  std::unique_ptr<Profile> profile = profile_builder.Build();
  ASSERT_EQ(profile_basename, profile->GetBaseName());

  // Convert to profile ID and back.
  std::string profile_id =
      NotificationPlatformBridge::GetProfileId(profile.get());
  EXPECT_EQ(
      profile_basename,
      NotificationPlatformBridge::GetProfileBaseNameFromProfileId(profile_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NotificationPlatformBridgeTest,
    testing::Values(
        // Default profile directories.
        base::FilePath(FILE_PATH_LITERAL("Default")),
        base::FilePath(FILE_PATH_LITERAL("Profile 7")),
        // Custom directory.
        base::FilePath(FILE_PATH_LITERAL("CustomDir")),
        // Non-ASCII string.
        base::FilePath(FILE_PATH_LITERAL("\u0645\u0635\u0631"))));
