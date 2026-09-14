// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using ::testing::_;

namespace enterprise_connectors {

class DeviceTrustConnectorServiceFactoryBaseTest {
 public:
  DeviceTrustConnectorServiceFactoryBaseTest() {
    RedirectTestingFactoryToRealFactory(profile());
  }

  // TestingProfiles create use a TestingFactory for service
  // `DeviceTrustConnectorService`. To test the real factory behavior via a unit
  // test, we redirect the testing factory to the real factory
  // `BuildServiceInstanceFor` method.
  void RedirectTestingFactoryToRealFactory(Profile* profile) {
    DeviceTrustConnectorServiceFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return DeviceTrustConnectorServiceFactory::GetInstance()
              ->BuildServiceInstanceForBrowserContext(context);
        }));
  }

 protected:
  Profile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  base::test::ScopedFeatureList feature_list_;
};

class DeviceTrustConnectorServiceFactoryTest
    : public DeviceTrustConnectorServiceFactoryBaseTest,
      public ::testing::Test {};

TEST_F(DeviceTrustConnectorServiceFactoryTest, CreateForRegularProfile) {
  EXPECT_FALSE(profile()->IsOffTheRecord());
  EXPECT_TRUE(DeviceTrustConnectorServiceFactory::GetForProfile(profile()));
}

TEST_F(DeviceTrustConnectorServiceFactoryTest, CreatedForIsolatedModeProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      enterprise_isolated_mode::kEnableEnterpriseIsolatedMode);
  profile()->GetPrefs()->SetInteger(
      enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
      static_cast<int>(
          enterprise_isolated_mode::IsolatedModeSetting::kEnabled));

  Profile* isolated_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(isolated_profile);
  EXPECT_TRUE(isolated_profile->IsOffTheRecord());
  EXPECT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());

  RedirectTestingFactoryToRealFactory(isolated_profile);

  DeviceTrustConnectorService* isolated_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(isolated_profile);
  EXPECT_TRUE(isolated_service);

  DeviceTrustConnectorService* regular_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile());
  EXPECT_TRUE(regular_service);
  EXPECT_NE(regular_service, isolated_service);
}

TEST_F(DeviceTrustConnectorServiceFactoryTest, NullForIncognitoProfile) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(ash::ProfileHelper::IsSigninProfile(incognito_profile));
#endif  // BUILDFLAG(IS_CHROMEOS)

  ASSERT_TRUE(incognito_profile);

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(device_trust_connector_service);
}

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(DeviceTrustConnectorServiceFactoryTest,
       CreatedForSigninProfileChromeOS) {
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
  std::unique_ptr<TestingProfile> testing_profile = builder.Build();

  Profile* signin_profile =
      testing_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  RedirectTestingFactoryToRealFactory(signin_profile);

  ASSERT_TRUE(signin_profile);
  EXPECT_TRUE(signin_profile->IsOffTheRecord());
  EXPECT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile));

  // Make sure a DeviceTrustConnectorService cannot be created from an incognito
  // Profile.
  DeviceTrustConnectorService* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(signin_profile);
  EXPECT_TRUE(device_trust_connector_service);
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace enterprise_connectors
