// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_ai/private_ai_service_factory.h"

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/private_ai/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

class PrivateAiServiceFactoryTest : public testing::Test {
 protected:
  explicit PrivateAiServiceFactoryTest(bool feature_enabled = true) {
    if (feature_enabled) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(kPrivateAi);
    }
  }

  TestingProfile* profile() {
    if (!profile_) {
      profile_ = TestingProfile::Builder().Build();
    }
    return profile_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(PrivateAiServiceFactoryTest, ServiceCreationSucceedsWhenFlagEnabled) {
  PrivateAiService* service = PrivateAiServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(service);
}

TEST_F(PrivateAiServiceFactoryTest, ServiceCreationFailsForIncognito) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);
  // Service should be `nullptr` for OTR profiles.
  EXPECT_FALSE(PrivateAiServiceFactory::GetForProfile(otr_profile));
}

class PrivateAiServiceFactoryFeatureDisabledTest
    : public PrivateAiServiceFactoryTest {
 public:
  PrivateAiServiceFactoryFeatureDisabledTest()
      : PrivateAiServiceFactoryTest(/*feature_enabled=*/false) {}
};

TEST_F(PrivateAiServiceFactoryFeatureDisabledTest,
       ServiceCreationFailsWhenFlagDisabled) {
  PrivateAiService* service = PrivateAiServiceFactory::GetForProfile(profile());
  EXPECT_FALSE(service);
}

}  // namespace private_ai
