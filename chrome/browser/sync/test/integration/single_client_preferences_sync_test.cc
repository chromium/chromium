// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

using preferences_helper::BooleanPrefMatches;
using preferences_helper::BuildPrefStoreFromPrefsFile;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::GetRegistry;
using user_prefs::PrefRegistrySyncable;
using testing::Eq;
using testing::NotNull;

namespace {

class SingleClientPreferencesSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientPreferencesSyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSPreferences),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPreferencesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
  ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(BooleanPrefMatches(prefs::kHomePageIsNewTabPage));
}

// This test simply verifies that preferences registered after sync started
// get properly synced.
IN_PROC_BROWSER_TEST_P(SingleClientPreferencesSyncTest, LateRegistration) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PrefRegistrySyncable* registry = GetRegistry(GetProfile(0));
  const std::string pref_name = "testing.my-test-preference";
  registry->WhitelistLateRegistrationPrefForSync(pref_name);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  registry->RegisterBooleanPref(
      pref_name, true, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Verify the default is properly used.
  EXPECT_TRUE(GetProfile(0)->GetPrefs()->GetBoolean(pref_name));
  // Now make a change and verify it gets uploaded.
  GetProfile(0)->GetPrefs()->SetBoolean(pref_name, false);
  ASSERT_FALSE(GetProfile(0)->GetPrefs()->GetBoolean(pref_name));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  GetRegistry(verifier())
      ->RegisterBooleanPref(pref_name, true,
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  EXPECT_FALSE(BooleanPrefMatches(pref_name.c_str()));
}

IN_PROC_BROWSER_TEST_P(SingleClientPreferencesSyncTest,
                       ShouldRemoveBadDataWhenRegistering) {
  // Populate the data store with data of type boolean but register as string.
  SetPreexistingPreferencesFileContents(
      0, "{\"testing\":{\"my-test-preference\":true}}");
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PrefRegistrySyncable* registry = GetRegistry(GetProfile(0));
  registry->RegisterStringPref("testing.my-test-preference", "default-value",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  const PrefService::Preference* preference =
      GetProfile(0)->GetPrefs()->FindPreference("testing.my-test-preference");
  ASSERT_THAT(preference, NotNull());
  EXPECT_THAT(preference->GetType(), Eq(base::Value::Type::STRING));
  EXPECT_THAT(preference->GetValue()->GetString(), Eq("default-value"));
  // This might actually expose a bug: IsDefaultValue() is looking for the
  // the store with highest priority which has a value for the preference's
  // name. For this, no type checks are done, and hence this value is not
  // recognized as a default value. --> file a bug!
  EXPECT_TRUE(preference->IsDefaultValue());

  // To verify the bad data has been removed, we read the JSON file from disk.
  scoped_refptr<PrefStore> pref_store =
      BuildPrefStoreFromPrefsFile(GetProfile(0));
  const base::Value* result;
  EXPECT_FALSE(pref_store->GetValue("testing.my-test-preference", &result));
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientPreferencesSyncTest,
                        ::testing::Values(false, true));

}  // namespace
