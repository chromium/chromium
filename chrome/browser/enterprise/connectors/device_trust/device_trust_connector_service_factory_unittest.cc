// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

class DeviceTrustConnectorServiceFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled, true);
  }
  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DeviceTrustConnectorServiceFactoryTest, CreateForRegularProfile) {
  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_TRUE(DeviceTrustConnectorServiceFactory::GetForProfile(profile()));
}

TEST_F(DeviceTrustConnectorServiceFactoryTest,
       CreatedForSigninProfileChromeOS) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
  std::unique_ptr<TestingProfile> testing_profile = builder.Build();

  Profile* signin_profile =
      testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  ASSERT_TRUE(signin_profile);
  EXPECT_TRUE(signin_profile->IsOffTheRecord());
  EXPECT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile));

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(signin_profile);
  EXPECT_TRUE(device_trust_connector_service);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(DeviceTrustConnectorServiceFactoryTest, NullForIncognitoProfile) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(ash::ProfileHelper::IsSigninProfile(incognito_profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ASSERT_TRUE(incognito_profile);

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(device_trust_connector_service);
}

}  // namespace enterprise_connectors
