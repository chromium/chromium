// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/supported_links_infobar_prefs_service.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestAppId1[] = "foo";
const char kTestAppId2[] = "bar";
}  // namespace

namespace apps {

class SupportedLinksInfoBarPrefsServiceTest : public testing::Test {
 public:
  SupportedLinksInfoBarPrefsServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  TestingProfile* profile() { return &profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SupportedLinksInfoBarPrefsServiceTest, IgnoreInfoBar) {
  SupportedLinksInfoBarPrefsService service(profile());

  for (int i = 0; i < 3; i++) {
    ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId1));
    service.MarkInfoBarIgnored(kTestAppId1);
  }

  ASSERT_TRUE(service.ShouldHideInfoBarForApp(kTestAppId1));
  ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId2));
}

TEST_F(SupportedLinksInfoBarPrefsServiceTest, DismissInfoBar) {
  SupportedLinksInfoBarPrefsService service(profile());

  service.MarkInfoBarDismissed(kTestAppId1);
  ASSERT_TRUE(service.ShouldHideInfoBarForApp(kTestAppId1));
  ASSERT_FALSE(service.ShouldHideInfoBarForApp(kTestAppId2));
}

}  // namespace apps
