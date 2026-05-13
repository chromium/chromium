// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {

namespace {

TEST(ShouldShowAnimatedIdentityOnOpeningWindow, ReturnsFalseForNewWindow) {
  // Setup a testing profile manager with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  std::string name("testing_profile");
  TestingProfile* profile = profile_manager.CreateTestingProfile(
      name, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      base::UTF8ToUTF16(name), 0,
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());

  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));

  // Animation is shown once.
  RecordAnimatedIdentityTriggered(profile);

  // Wait a few seconds.
  task_environment.FastForwardBy(base::Seconds(6));

  // Animation is not shown again in a new window.
  EXPECT_FALSE(ShouldShowAnimatedIdentityOnOpeningWindow(*profile));
}

}  // namespace

}  // namespace signin_ui_util
