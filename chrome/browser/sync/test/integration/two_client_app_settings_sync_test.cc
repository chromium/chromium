// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"
#include "chrome/browser/sync/test/integration/extension_settings_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace {

using apps_helper::InstallHostedAppForAllProfiles;
using extension_settings_helper::AllExtensionSettingsSameChecker;
using extension_settings_helper::GetExtensionSettings;
using extension_settings_helper::SetExtensionSettings;

class TwoClientAppSettingsSyncTest : public AppsSyncTestBase {
 public:
  TwoClientAppSettingsSyncTest() : AppsSyncTestBase(TWO_CLIENT) {}
  ~TwoClientAppSettingsSyncTest() override = default;

  // APP_SETTINGS is only supported with Sync-the-feature.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
  }

  struct ExpectedSettings {
    base::DictValue extension0;
    base::DictValue extension1;
    base::DictValue extension2;
  };

  // Generic mutations done after the initial setup of all tests. Note that
  // existing configurations of the sync server cannot be tested since the
  // test infrastructure does not support that.
  ExpectedSettings MutateSettingsAcrossProfiles(
      int seed,  // used to modify the mutation values, not keys.
      const std::string& extension0,
      const std::string& extension1,
      const std::string& extension2) {
    ExpectedSettings expected;

    // Write to extension0 from profile 0 but not profile 1.
    expected.extension0.Set("asdf", base::StringPrintf("asdfasdf-%d", seed));
    SetExtensionSettings(GetProfile(0), extension0, expected.extension0);

    // Write the same data to extension1 from both profiles.
    expected.extension1.Set("asdf", base::StringPrintf("asdfasdf-%d", seed));
    expected.extension1.Set("qwer", base::StringPrintf("qwerqwer-%d", seed));
    SetExtensionSettings(GetAllProfiles(), extension1, expected.extension1);

    // Write different data to extension2 from each profile.
    base::DictValue settings0;
    settings0.Set("zxcv", base::StringPrintf("zxcvzxcv-%d", seed));
    SetExtensionSettings(GetProfile(0), extension2, settings0);

    base::DictValue settings1;
    settings1.Set("1324", base::StringPrintf("12341234-%d", seed));
    settings1.Set("5687", base::StringPrintf("56785678-%d", seed));
    SetExtensionSettings(GetProfile(1), extension2, settings1);

    expected.extension2.Merge(settings0.Clone());
    expected.extension2.Merge(settings1.Clone());

    return expected;
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientAppSettingsSyncTest,
                       AppsStartWithSameSettings) {
  ASSERT_TRUE(SetupClients());
  const std::string extension0 = InstallHostedAppForAllProfiles(0);
  const std::string extension1 = InstallHostedAppForAllProfiles(1);
  const std::string extension2 = InstallHostedAppForAllProfiles(2);

  // For three independent extensions:
  // Set up each extension with the same (but not necessarily empty) settings
  // for all profiles, start syncing, add some new settings, sync, mutate those
  // settings, sync.
  // Leave extension0 empty.
  base::DictValue base_settings1;
  base_settings1.Set("foo", "bar");
  SetExtensionSettings(GetAllProfiles(), extension1, base_settings1);
  base::DictValue base_settings2;
  base_settings2.Set("foo", "bar");
  base_settings2.Set("baz", "qux");
  SetExtensionSettings(GetAllProfiles(), extension2, base_settings2);

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
          .Wait());
  EXPECT_EQ(base::DictValue(), GetExtensionSettings(GetProfile(0), extension0));
  EXPECT_EQ(base_settings1, GetExtensionSettings(GetProfile(0), extension1));
  EXPECT_EQ(base_settings2, GetExtensionSettings(GetProfile(0), extension2));

  for (int seed : {0, 1}) {
    ExpectedSettings expected =
        MutateSettingsAcrossProfiles(seed, extension0, extension1, extension2);
    expected.extension1.Merge(base_settings1.Clone());
    expected.extension2.Merge(base_settings2.Clone());
    ASSERT_TRUE(
        AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
            .Wait());
    EXPECT_EQ(expected.extension0,
              GetExtensionSettings(GetProfile(0), extension0))
        << "For seed=" << seed;
    EXPECT_EQ(expected.extension1,
              GetExtensionSettings(GetProfile(0), extension1))
        << "For seed=" << seed;
    EXPECT_EQ(expected.extension2,
              GetExtensionSettings(GetProfile(0), extension2))
        << "For seed=" << seed;
  }
}

IN_PROC_BROWSER_TEST_F(TwoClientAppSettingsSyncTest,
                       AppsStartWithDifferentSettings) {
  ASSERT_TRUE(SetupClients());
  const std::string extension0 = InstallHostedAppForAllProfiles(0);
  const std::string extension1 = InstallHostedAppForAllProfiles(1);
  const std::string extension2 = InstallHostedAppForAllProfiles(2);

  // For three independent extensions:
  // Set up each extension with different settings for each profile, start
  // syncing, add some settings, sync, mutate those settings, sync, have a no-op
  // (non-)change to those settings, sync, mutate again, sync.
  // Leave extension0 empty again for no particular reason other than it's
  // the only remaining unique combination given the other 2 tests have
  // (empty, nonempty) and (nonempty, nonempty) configurations. Testing
  // (nonempty, nonempty) cannot be done because the merging will provide
  // unpredictable results, so (empty, empty) is tested.
  base::DictValue base_settings1;
  base_settings1.Set("foo", "bar");
  SetExtensionSettings(GetProfile(0), extension1, base_settings1);
  base::DictValue base_settings2;
  base_settings2.Set("foo", "bar");
  base_settings2.Set("baz", "qux");
  SetExtensionSettings(GetProfile(1), extension2, base_settings2);

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
          .Wait());
  EXPECT_EQ(base::DictValue(), GetExtensionSettings(GetProfile(0), extension0));
  EXPECT_EQ(base_settings1, GetExtensionSettings(GetProfile(0), extension1));
  EXPECT_EQ(base_settings2, GetExtensionSettings(GetProfile(0), extension2));

  // The second seed=3 iteration tests a round of no-ops once, for sanity.
  // Ideally this would assert that this causes no sync activity, but that
  // is difficult to verify in integration tests.
  for (int seed : {2, 3, 3, 4}) {
    ExpectedSettings expected =
        MutateSettingsAcrossProfiles(seed, extension0, extension1, extension2);
    expected.extension1.Merge(base_settings1.Clone());
    expected.extension2.Merge(base_settings2.Clone());
    ASSERT_TRUE(
        AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
            .Wait());
    EXPECT_EQ(expected.extension0,
              GetExtensionSettings(GetProfile(0), extension0))
        << "For seed=" << seed;
    EXPECT_EQ(expected.extension1,
              GetExtensionSettings(GetProfile(0), extension1))
        << "For seed=" << seed;
    EXPECT_EQ(expected.extension2,
              GetExtensionSettings(GetProfile(0), extension2))
        << "For seed=" << seed;
  }
}

}  // namespace
