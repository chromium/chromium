// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/profile_registry/profile_registry_service_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_actuator/local_actuation/local_actuation_names.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/browser_actuator/public/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class ProfileRegistryServiceFactoryTest : public testing::Test {
 protected:
  void SetUp() override {
    chrome::SetUsingDefaultUserDataDirectoryForTesting(true);
  }

  void TearDown() override {
    chrome::SetUsingDefaultUserDataDirectoryForTesting(std::nullopt);
  }

  std::unique_ptr<TestingProfile> BuildProfile(
      TestingProfile::Builder& builder) {
    return builder.Build();
  }

  std::unique_ptr<TestingProfile> BuildProfile() {
    TestingProfile::Builder builder;
    return BuildProfile(builder);
  }

  base::test::ScopedFeatureList feature_list_{kBrowserActuatorProfileRegistry};
  // The fake keychain reports its own access group as the binary's
  // entitlement.
  std::optional<crypto::apple::ScopedFakeKeychainV2> fake_keychain_{
      std::in_place, kLocalActuationKeychainAccessGroup};
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ProfileRegistryServiceFactoryTest, CreatesServiceWithRegularProfile) {
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  // With this gate closed, `GetForProfile()` can only return a service that
  // was created along with the profile.
  chrome::SetUsingDefaultUserDataDirectoryForTesting(false);

  EXPECT_NE(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest, NoServiceWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(kBrowserActuatorProfileRegistry);
  std::unique_ptr<TestingProfile> profile = BuildProfile();

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest, NoServiceForNonDefaultUserDataDir) {
  chrome::SetUsingDefaultUserDataDirectoryForTesting(false);
  std::unique_ptr<TestingProfile> profile = BuildProfile();

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest, NoServiceWithoutEntitlement) {
  fake_keychain_.emplace("TEAMID.com.example.other-group");
  std::unique_ptr<TestingProfile> profile = BuildProfile();

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest, NoServiceForOffTheRecordProfile) {
  std::unique_ptr<TestingProfile> profile = BuildProfile();
  Profile* otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(otr_profile), nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest, NoServiceForEphemeralProfile) {
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  const base::FilePath path =
      profile_manager.profiles_dir().AppendASCII("EphemeralProfile");
  ProfileAttributesInitParams init_params;
  init_params.profile_path = path;
  init_params.profile_name = u"Ephemeral";
  profile_manager.profile_attributes_storage()->AddProfile(
      std::move(init_params));
  ProfileAttributesEntry* entry = profile_manager.profile_attributes_storage()
                                      ->GetProfileAttributesWithPath(path);
  ASSERT_NE(entry, nullptr);
  entry->SetIsEphemeral(true);
  TestingProfile::Builder builder;
  builder.SetPath(path);
  std::unique_ptr<TestingProfile> profile = BuildProfile(builder);

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

TEST_F(ProfileRegistryServiceFactoryTest,
       NoServiceWhenEphemeralProfilesForced) {
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(pref_service->registry());
  pref_service->SetBoolean(prefs::kForceEphemeralProfiles, true);
  TestingProfile::Builder builder;
  builder.SetPrefService(std::move(pref_service));
  std::unique_ptr<TestingProfile> profile = BuildProfile(builder);

  EXPECT_EQ(ProfileRegistryServiceFactory::GetForProfile(profile.get()),
            nullptr);
}

}  // namespace
}  // namespace browser_actuator
