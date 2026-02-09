// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"
#include "chrome/browser/sync/test/integration/extension_settings_helper.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace {

using apps_helper::InstallHostedAppForAllProfiles;
using extension_settings_helper::AllExtensionSettingsSameChecker;
using extension_settings_helper::SetExtensionSettings;

class TwoClientAppSettingsSyncTest : public AppsSyncTestBase {
 public:
  TwoClientAppSettingsSyncTest() : AppsSyncTestBase(TWO_CLIENT) {}
  ~TwoClientAppSettingsSyncTest() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/40724949): rewrite tests to not use verifier.
    return true;
  }

  // APP_SETTINGS is only supported with Sync-the-feature.
  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTheFeature;
  }

  // Generic mutations done after the initial setup of all tests. Note that
  // unfortunately we can't test existing configurations of the sync server
  // since the tests don't support that.
  void MutateSomeSettings(
      int seed,  // used to modify the mutation values, not keys.
      const std::string& extension0,
      const std::string& extension1,
      const std::string& extension2) {
    {
      // Write to extension0 from profile 0 but not profile 1.
      base::DictValue settings;
      settings.Set("asdf", base::StringPrintf("asdfasdf-%d", seed));
      SetExtensionSettings(verifier(), extension0, settings);
      SetExtensionSettings(GetProfile(0), extension0, settings);
    }
    {
      // Write the same data to extension1 from both profiles.
      base::DictValue settings;
      settings.Set("asdf", base::StringPrintf("asdfasdf-%d", seed));
      settings.Set("qwer", base::StringPrintf("qwerqwer-%d", seed));
      SetExtensionSettings(GetAllProfiles(), extension1, settings);
    }
    {
      // Write different data to extension2 from each profile.
      base::DictValue settings0;
      settings0.Set("zxcv", base::StringPrintf("zxcvzxcv-%d", seed));
      SetExtensionSettings(verifier(), extension2, settings0);
      SetExtensionSettings(GetProfile(0), extension2, settings0);

      base::DictValue settings1;
      settings1.Set("1324", base::StringPrintf("12341234-%d", seed));
      settings1.Set("5687", base::StringPrintf("56785678-%d", seed));
      SetExtensionSettings(verifier(), extension2, settings1);
      SetExtensionSettings(GetProfile(1), extension2, settings1);
    }
  }

  // For three independent extensions:
  //
  // Set up each extension with the same (but not necessarily empty) settings
  // for all profiles, start syncing, add some new settings, sync, mutate those
  // settings, sync.
  testing::AssertionResult StartWithSameSettingsTest(
      const std::string& extension0,
      const std::string& extension1,
      const std::string& extension2) {
    {
      // Leave extension0 empty.
    }
    {
      base::DictValue settings;
      settings.Set("foo", "bar");
      SetExtensionSettings(GetAllProfiles(), extension1, settings);
    }
    {
      base::DictValue settings;
      settings.Set("foo", "bar");
      settings.Set("baz", "qux");
      SetExtensionSettings(GetAllProfiles(), extension2, settings);
    }

    if (!SetupSync()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    MutateSomeSettings(0, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    MutateSomeSettings(1, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    return testing::AssertionSuccess();
  }

  // For three independent extensions:
  //
  // Set up each extension with different settings for each profile, start
  // syncing, add some settings, sync, mutate those settings, sync, have a no-op
  // (non-)change to those settings, sync, mutate again, sync.
  testing::AssertionResult StartWithDifferentSettingsTest(
      const std::string& extension0,
      const std::string& extension1,
      const std::string& extension2) {
    {
      // Leave extension0 empty again for no particular reason other than it's
      // the only remaining unique combination given the other 2 tests have
      // (empty, nonempty) and (nonempty, nonempty) configurations. We can't
      // test (nonempty, nonempty) because the merging will provide
      // unpredictable results, so test (empty, empty).
    }
    {
      base::DictValue settings;
      settings.Set("foo", "bar");
      SetExtensionSettings(verifier(), extension1, settings);
      SetExtensionSettings(GetProfile(0), extension1, settings);
    }
    {
      base::DictValue settings;
      settings.Set("foo", "bar");
      settings.Set("baz", "qux");
      SetExtensionSettings(verifier(), extension2, settings);
      SetExtensionSettings(GetProfile(1), extension2, settings);
    }

    if (!SetupSync()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    MutateSomeSettings(2, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    MutateSomeSettings(3, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    // Test a round of no-ops once, for sanity. Ideally we'd want to assert that
    // this causes no sync activity, but that sounds tricky.
    MutateSomeSettings(3, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    MutateSomeSettings(4, extension0, extension1, extension2);
    if (!AwaitQuiescence()) {
      return testing::AssertionFailure();
    }
    if (!AllExtensionSettingsSameChecker(GetSyncServices(), GetAllProfiles())
             .Wait()) {
      return testing::AssertionFailure();
    }

    return testing::AssertionSuccess();
  }
};

IN_PROC_BROWSER_TEST_F(TwoClientAppSettingsSyncTest,
                       AppsStartWithSameSettings) {
  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(StartWithSameSettingsTest(InstallHostedAppForAllProfiles(0),
                                        InstallHostedAppForAllProfiles(1),
                                        InstallHostedAppForAllProfiles(2)));
}

IN_PROC_BROWSER_TEST_F(TwoClientAppSettingsSyncTest,
                       AppsStartWithDifferentSettings) {
  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(StartWithDifferentSettingsTest(
      InstallHostedAppForAllProfiles(0), InstallHostedAppForAllProfiles(1),
      InstallHostedAppForAllProfiles(2)));
}

}  // namespace
