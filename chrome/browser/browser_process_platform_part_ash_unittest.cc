// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_ash.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserProcessPlatformPartAshTest : public testing::Test {
 public:
  BrowserProcessPlatformPartAshTest() = default;
  BrowserProcessPlatformPartAshTest(const BrowserProcessPlatformPartAshTest&) =
      delete;
  BrowserProcessPlatformPartAshTest& operator=(
      const BrowserProcessPlatformPartAshTest&) = delete;
  ~BrowserProcessPlatformPartAshTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<BrowserProcessPlatformPart> process_part_ =
      TestingBrowserProcess::GetGlobal()->platform_part();
};

TEST_F(BrowserProcessPlatformPartAshTest, RestoresURLsForRegularProfiles) {
  BrowserProcessPlatformPartTestApi test_api(process_part_);
  EXPECT_TRUE(test_api.CanRestoreUrlsForProfile(&profile_));
}

TEST_F(BrowserProcessPlatformPartAshTest,
       DoesNotRestoreURLsForIncognitoProfiles) {
  Profile* incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  BrowserProcessPlatformPartTestApi test_api(process_part_);
  EXPECT_FALSE(test_api.CanRestoreUrlsForProfile(incognito_profile));
}
