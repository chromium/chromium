// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeVariationsServiceClientTest : public ::testing::Test {
 public:
  ChromeVariationsServiceClientTest() {
    // Call the methods that register all the prefs used by this class.
    ProfileAttributesStorage::RegisterPrefs(local_state_.registry());
    metrics::CleanExitBeacon::RegisterPrefs(local_state_.registry());
    variations::VariationsService::RegisterPrefs(local_state_.registry());
  }

 protected:
  // Adds an entry to the variations Google Groups preference.
  void AddVariationsEntry(const std::string& profile_key) {
    ScopedDictPrefUpdate variations_prefs_update(
        &local_state_, variations::prefs::kVariationsGoogleGroups);
    base::Value::Dict& variations_prefs = variations_prefs_update.Get();

    // Just set an empty list of groups as the code under test is agnostic to
    // the value of the entry.
    base::Value::List groups;
    variations_prefs.Set(profile_key, std::move(groups));
  }

  // Add an entry to ProfileAttributesStorage.
  // Ideally we would mock the ProfileAttributesStorage::GetAllProfilesKeys()
  // call, but it is a static method and the logic is straightforward so we
  // write the pref directly instead.
  void AddProfileAttributesStorageEntry(const std::string& profile_key) {
    ScopedDictPrefUpdate profile_attributes_prefs_update(
        &local_state_, prefs::kProfileAttributes);
    base::Value::Dict& profile_prefs = profile_attributes_prefs_update.Get();

    base::Value::Dict profile_info;
    profile_prefs.Set(profile_key, std::move(profile_info));
  }

  ChromeVariationsServiceClient variations_service_client_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(ChromeVariationsServiceClientTest,
       RemoveGoogleGroupsFromPrefsForDeletedProfiles_NoPrefNoProfile) {
  variations_service_client_.RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      &local_state_);

  const base::Value::Dict& cached_profiles =
      local_state_.GetDict(variations::prefs::kVariationsGoogleGroups);
  ASSERT_EQ(cached_profiles.size(), 0UL);
}

TEST_F(ChromeVariationsServiceClientTest,
       RemoveGoogleGroupsFromPrefsForDeletedProfiles_PrefNoProfile) {
  AddVariationsEntry("Profile 1");

  variations_service_client_.RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      &local_state_);

  const base::Value::Dict& cached_profiles =
      local_state_.GetDict(variations::prefs::kVariationsGoogleGroups);
  ASSERT_EQ(cached_profiles.size(), 0UL);
}

TEST_F(ChromeVariationsServiceClientTest,
       RemoveGoogleGroupsFromPrefsForDeletedProfiles_NoPrefProfile) {
  AddProfileAttributesStorageEntry("Profile 1");

  variations_service_client_.RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      &local_state_);

  const base::Value::Dict& cached_profiles =
      local_state_.GetDict(variations::prefs::kVariationsGoogleGroups);
  ASSERT_EQ(cached_profiles.size(), 0UL);
}

TEST_F(ChromeVariationsServiceClientTest,
       RemoveGoogleGroupsFromPrefsForDeletedProfiles_PrefAndDifferentProfile) {
  AddVariationsEntry("Profile 1");
  AddProfileAttributesStorageEntry("Profile 2");

  variations_service_client_.RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      &local_state_);

  const base::Value::Dict& cached_profiles =
      local_state_.GetDict(variations::prefs::kVariationsGoogleGroups);
  ASSERT_EQ(cached_profiles.size(), 0UL);
}

TEST_F(ChromeVariationsServiceClientTest,
       RemoveGoogleGroupsFromPrefsForDeletedProfiles_PrefAndSameProfile) {
  std::string profile_key = "Profile 1";
  AddVariationsEntry(profile_key);
  AddProfileAttributesStorageEntry(profile_key);

  variations_service_client_.RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      &local_state_);

  const base::Value::Dict& cached_profiles =
      local_state_.GetDict(variations::prefs::kVariationsGoogleGroups);
  ASSERT_EQ(cached_profiles.size(), 1UL);
  ASSERT_TRUE(cached_profiles.Find(profile_key));
}
