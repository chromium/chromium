// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeVariationsServiceClientTest : public ::testing::Test {
 public:
  ChromeVariationsServiceClientTest() {
    // Call the methods that register all the prefs used by this class.
    ProfileAttributesStorage::RegisterPrefs(local_state_.registry());
    metrics::CleanExitBeacon::RegisterPrefs(local_state_.registry());
  }

 protected:
  // Add an entry to ProfileAttributesStorage.
  // Ideally we would mock the ProfileAttributesStorage::GetAllProfilesKeys()
  // call, but it is a static method and the logic is straightforward so we
  // write the pref directly instead.
  void AddProfileAttributesStorageEntry(const std::string& profile_key) {
    ScopedDictPrefUpdate profile_attributes_prefs_update(
        &local_state_, prefs::kProfileAttributes);
    base::DictValue& profile_prefs = profile_attributes_prefs_update.Get();

    base::DictValue profile_info;
    profile_prefs.Set(profile_key, std::move(profile_info));
  }

  ChromeVariationsServiceClient variations_service_client_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(ChromeVariationsServiceClientTest, GetAllProfilesKeys_NoProfiles) {
  EXPECT_THAT(variations_service_client_.GetAllProfilesKeys(&local_state_),
              testing::Optional(testing::IsEmpty()));
}

TEST_F(ChromeVariationsServiceClientTest, GetAllProfilesKeys) {
  AddProfileAttributesStorageEntry("Profile 1");
  AddProfileAttributesStorageEntry("Profile 2");
  AddProfileAttributesStorageEntry("Profile 3");

  EXPECT_THAT(variations_service_client_.GetAllProfilesKeys(&local_state_),
              testing::Optional(testing::UnorderedElementsAre(
                  "Profile 1", "Profile 2", "Profile 3")));
}
