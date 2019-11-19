// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/extension_settings_helper.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace {

using apps_helper::InstallAppForAllProfiles;
using extension_settings_helper::AllExtensionSettingsSameAsVerifier;
using extension_settings_helper::SetExtensionSettings;
using extension_settings_helper::SetExtensionSettingsForAllProfiles;
using extensions_helper::InstallExtensionForAllProfiles;
using sync_datatype_helper::test;

// Generic mutations done after the initial setup of all tests. Note that
// unfortuately we can't test existing configurations of the sync server since
// the tests don't support that.
void MutateSomeSettings(
    int seed,  // used to modify the mutation values, not keys.
    const std::string& extension0,
    const std::string& extension1,
    const std::string& extension2) {
  {
    // Write to extension0 from profile 0 but not profile 1.
    base::DictionaryValue settings;
    settings.SetString("asdf", base::StringPrintf("asdfasdf-%d", seed));
    SetExtensionSettings(test()->verifier(),    extension0, settings);
    SetExtensionSettings(test()->GetProfile(0), extension0, settings);
  }
  {
    // Write the same data to extension1 from both profiles.
    base::DictionaryValue settings;
    settings.SetString("asdf", base::StringPrintf("asdfasdf-%d", seed));
    settings.SetString("qwer", base::StringPrintf("qwerqwer-%d", seed));
    SetExtensionSettingsForAllProfiles(extension1, settings);
  }
  {
    // Write different data to extension2 from each profile.
    base::DictionaryValue settings0;
    settings0.SetString("zxcv", base::StringPrintf("zxcvzxcv-%d", seed));
    SetExtensionSettings(test()->verifier(),    extension2, settings0);
    SetExtensionSettings(test()->GetProfile(0), extension2, settings0);

    base::DictionaryValue settings1;
    settings1.SetString("1324", base::StringPrintf("12341234-%d", seed));
    settings1.SetString("5687", base::StringPrintf("56785678-%d", seed));
    SetExtensionSettings(test()->verifier(),    extension2, settings1);
    SetExtensionSettings(test()->GetProfile(1), extension2, settings1);
  }
}

class TwoClientExtensionSettingsAndAppSettingsSyncTest : public SyncTest {
 public:
  TwoClientExtensionSettingsAndAppSettingsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientExtensionSettingsAndAppSettingsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientExtensionSettingsAndAppSettingsSyncTest);
};

// For three independent extensions:
//
// Set up each extension with the same (but not necessarily empty) settings for
// all profiles, start syncing, add some new settings, sync, mutate those
// settings, sync.
testing::AssertionResult StartWithSameSettingsTest(
    const std::string& extension0,
    const std::string& extension1,
    const std::string& extension2) {
  {
    // Leave extension0 empty.
  }
  {
    base::DictionaryValue settings;
    settings.SetString("foo", "bar");
    SetExtensionSettingsForAllProfiles(extension1, settings);
  }
  {
    base::DictionaryValue settings;
    settings.SetString("foo", "bar");
    settings.SetString("baz", "qux");
    SetExtensionSettingsForAllProfiles(extension2, settings);
  }

  if (!test()->SetupSync())
    return testing::AssertionFailure();
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  MutateSomeSettings(0, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  MutateSomeSettings(1, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

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
    // (empty, nonempty) and (nonempty, nonempty) configurations. We can't test
    // (nonempty, nonempty) because the merging will provide unpredictable
    // results, so test (empty, empty).
  }
  {
    base::DictionaryValue settings;
    settings.SetString("foo", "bar");
    SetExtensionSettings(test()->verifier(), extension1, settings);
    SetExtensionSettings(test()->GetProfile(0), extension1, settings);
  }
  {
    base::DictionaryValue settings;
    settings.SetString("foo", "bar");
    settings.SetString("baz", "qux");
    SetExtensionSettings(test()->verifier(), extension2, settings);
    SetExtensionSettings(test()->GetProfile(1), extension2, settings);
  }

  if (!test()->SetupSync())
    return testing::AssertionFailure();
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  MutateSomeSettings(2, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  MutateSomeSettings(3, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  // Test a round of no-ops once, for sanity. Ideally we'd want to assert that
  // this causes no sync activity, but that sounds tricky.
  MutateSomeSettings(3, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  MutateSomeSettings(4, extension0, extension1, extension2);
  if (!test()->AwaitQuiescence())
    return testing::AssertionFailure();
  if (!AllExtensionSettingsSameAsVerifier())
    return testing::AssertionFailure();

  return testing::AssertionSuccess();
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionSettingsAndAppSettingsSyncTest,
                       ExtensionsStartWithSameSettings) {
  ASSERT_TRUE(SetupClients());
  ASSERT_PRED3(StartWithSameSettingsTest, InstallExtensionForAllProfiles(0),
               InstallExtensionForAllProfiles(1),
               InstallExtensionForAllProfiles(2));
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionSettingsAndAppSettingsSyncTest,
                       AppsStartWithSameSettings) {
  ASSERT_TRUE(SetupClients());
  ASSERT_PRED3(StartWithSameSettingsTest, InstallAppForAllProfiles(0),
               InstallAppForAllProfiles(1), InstallAppForAllProfiles(2));
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionSettingsAndAppSettingsSyncTest,
                       ExtensionsStartWithDifferentSettings) {
  ASSERT_TRUE(SetupClients());
  ASSERT_PRED3(
      StartWithDifferentSettingsTest, InstallExtensionForAllProfiles(0),
      InstallExtensionForAllProfiles(1), InstallExtensionForAllProfiles(2));
}

IN_PROC_BROWSER_TEST_F(TwoClientExtensionSettingsAndAppSettingsSyncTest,
                       AppsStartWithDifferentSettings) {
  ASSERT_TRUE(SetupClients());
  ASSERT_PRED3(StartWithDifferentSettingsTest, InstallAppForAllProfiles(0),
               InstallAppForAllProfiles(1), InstallAppForAllProfiles(2));
}

}  // namespace
