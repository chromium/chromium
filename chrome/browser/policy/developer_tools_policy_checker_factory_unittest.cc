// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class DeveloperToolsPolicyCheckerFactoryTest : public testing::Test {
 public:
  DeveloperToolsPolicyCheckerFactoryTest() = default;
  ~DeveloperToolsPolicyCheckerFactoryTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DeveloperToolsPolicyCheckerFactoryTest, GetForBrowserContext) {
  TestingProfile profile;
  DeveloperToolsPolicyChecker* checker =
      DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(&profile);
  ASSERT_TRUE(checker);
}

TEST_F(DeveloperToolsPolicyCheckerFactoryTest, GetForIncognitoBrowserContext) {
  TestingProfile profile;
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&profile);
  DeveloperToolsPolicyChecker* checker =
      DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
          incognito_profile);
  DeveloperToolsPolicyChecker* checker_base_profile =
      DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(&profile);
  ASSERT_TRUE(checker);
  EXPECT_NE(checker, checker_base_profile);
}

TEST_F(DeveloperToolsPolicyCheckerFactoryTest, GetForGuestBrowserContext) {
  TestingProfile::Builder builder;
  TestingProfile profile;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = builder.Build();

  DeveloperToolsPolicyChecker* checker =
      DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
          guest_profile.get());
  DeveloperToolsPolicyChecker* checker_base_profile =
      DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(&profile);
  ASSERT_TRUE(checker);
  EXPECT_NE(checker, checker_base_profile);
}

}  // namespace policy
