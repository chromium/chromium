// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#endif

namespace {

// Extension ID of chrome/test/data/extensions/good.crx
const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";

// Explicit expectations from the caller of GetTrackedPrefHistogramCount(). This
// enables detailed reporting of the culprit on failure.
enum AllowedBuckets {
  // Allow no samples in any buckets.
  ALLOW_NONE = -1,
  // Any integer between BEGIN_ALLOW_SINGLE_BUCKET and END_ALLOW_SINGLE_BUCKET
  // indicates that only this specific bucket is allowed to have a sample.
  BEGIN_ALLOW_SINGLE_BUCKET = 0,
  END_ALLOW_SINGLE_BUCKET = 100,
  // Allow any buckets (no extra verifications performed).
  ALLOW_ANY
};

#if defined(OS_WIN)
base::string16 GetRegistryPathForTestProfile() {
  // Cleanup follow-up to http://crbug.com/721245 for the previous location of
  // this test key which had similar problems (to a lesser extent). It's
  // redundant but harmless to have multiple callers hit this on the same
  // machine. TODO(gab): remove this mid-june 2017.
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, L"SOFTWARE\\Chromium\\PrefHashBrowserTest",
               KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    LONG result = key.DeleteKey(L"");
    EXPECT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
  }

  base::FilePath profile_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &profile_dir));

  // Use a location under the real PreferenceMACs path so that the backup
  // cleanup logic in ChromeTestLauncherDelegate::PreSharding() for interrupted
  // tests covers this test key as well.
  return install_static::GetRegistryPath() +
         L"\\PreferenceMACs\\PrefHashBrowserTest\\" +
         profile_dir.BaseName().value();
}
#endif

// Returns the number of times |histogram_name| was reported so far; adding the
// results of the first 100 buckets (there are only ~19 reporting IDs as of this
// writing; varies depending on the platform). |allowed_buckets| hints at extra
// requirements verified in this method (see AllowedBuckets for details).
int GetTrackedPrefHistogramCount(const char* histogram_name,
                                 const char* histogram_suffix,
                                 int allowed_buckets) {
  std::string full_histogram_name(histogram_name);
  if (*histogram_suffix)
    full_histogram_name.append(".").append(histogram_suffix);
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(full_histogram_name);
  if (!histogram)
    return 0;

  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  int sum = 0;
  for (int i = 0; i < 100; ++i) {
    int count_for_id = samples->GetCount(i);
    EXPECT_GE(count_for_id, 0);
    sum += count_for_id;

    if (allowed_buckets == ALLOW_NONE ||
        (allowed_buckets != ALLOW_ANY && i != allowed_buckets)) {
      EXPECT_EQ(0, count_for_id) << "Unexpected reporting_id: " << i;
    }
  }
  return sum;
}

// Helper function to call GetTrackedPrefHistogramCount with no external
// validation suffix.
int GetTrackedPrefHistogramCount(const char* histogram_name,
                                 int allowed_buckets) {
  return GetTrackedPrefHistogramCount(histogram_name, "", allowed_buckets);
}

std::unique_ptr<base::DictionaryValue> ReadPrefsDictionary(
    const base::FilePath& pref_file) {
  JSONFileValueDeserializer deserializer(pref_file);
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::string error_str;
  std::unique_ptr<base::Value> prefs =
      deserializer.Deserialize(&error_code, &error_str);
  if (!prefs || error_code != JSONFileValueDeserializer::JSON_NO_ERROR) {
    ADD_FAILURE() << "Error #" << error_code << ": " << error_str;
    return std::unique_ptr<base::DictionaryValue>();
  }
  if (!prefs->is_dict()) {
    ADD_FAILURE();
    return std::unique_ptr<base::DictionaryValue>();
  }
  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(prefs.release()));
}

// Returns whether external validation is supported on the platform through
// storing MACs in the registry.
bool SupportsRegistryValidation() {
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

#define PREF_HASH_BROWSER_TEST(fixture, test_name)                             \
  IN_PROC_BROWSER_TEST_P(fixture, PRE_##test_name) { SetupPreferences(); }     \
  IN_PROC_BROWSER_TEST_P(fixture, test_name) { VerifyReactionToPrefAttack(); } \
  INSTANTIATE_TEST_SUITE_P(                                                    \
      fixture##Instance, fixture,                                              \
      testing::Values(                                                         \
          chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,     \
          chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,     \
          chrome_prefs::internals::                                            \
              kSettingsEnforcementGroupEnforceAlwaysWithDSE,                   \
          chrome_prefs::internals::                                            \
              kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE))

// A base fixture designed such that implementations do two things:
//  1) Override all three pure-virtual methods below to setup, attack, and
//     verify preferences throughout the tests provided by this fixture.
//  2) Instantiate their test via the PREF_HASH_BROWSER_TEST macro above.
// Based on top of ExtensionBrowserTest to allow easy interaction with the
// ExtensionRegistry.
class PrefHashBrowserTestBase
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<std::string> {
 public:
  // List of potential protection levels for this test in strict increasing
  // order of protection levels.
  enum SettingsProtectionLevel {
    PROTECTION_DISABLED_ON_PLATFORM,
    PROTECTION_DISABLED_FOR_GROUP,
    PROTECTION_ENABLED_BASIC,
    PROTECTION_ENABLED_DSE,
    PROTECTION_ENABLED_EXTENSIONS,
    // Represents the strongest level (i.e. always equivalent to the last one in
    // terms of protection), leave this one last when adding new levels.
    PROTECTION_ENABLED_ALL
  };

  PrefHashBrowserTestBase()
      : protection_level_(GetProtectionLevelFromTrialGroup(GetParam())) {
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    EXPECT_FALSE(command_line->HasSwitch(switches::kForceFieldTrials));
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        std::string(chrome_prefs::internals::kSettingsEnforcementTrialName) +
            "/" + GetParam() + "/");
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  bool SetUpUserDataDirectory() override {
    // Do the normal setup in the PRE test and attack preferences in the main
    // test.
    if (content::IsPreTest())
      return extensions::ExtensionBrowserTest::SetUpUserDataDirectory();

#if defined(OS_CHROMEOS)
    // For some reason, the Preferences file does not exist in the location
    // below on Chrome OS. Since protection is disabled on Chrome OS, it's okay
    // to simply not attack preferences at all (and still assert that no
    // hardening related histogram kicked in in VerifyReactionToPrefAttack()).
    // TODO(gab): Figure out why there is no Preferences file in this location
    // on Chrome OS (and re-enable the section disabled for OS_CHROMEOS further
    // below).
    EXPECT_EQ(PROTECTION_DISABLED_ON_PLATFORM, protection_level_);
    return true;
#endif

    base::FilePath profile_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &profile_dir));
    profile_dir = profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir);

    // Sanity check that old protected pref file is never present in modern
    // Chromes.
    EXPECT_FALSE(base::PathExists(
        profile_dir.Append(FILE_PATH_LITERAL("Protected Preferences"))));

    // Read the preferences from disk.

    const base::FilePath unprotected_pref_file =
        profile_dir.Append(chrome::kPreferencesFilename);
    EXPECT_TRUE(base::PathExists(unprotected_pref_file));

    const base::FilePath protected_pref_file =
        profile_dir.Append(chrome::kSecurePreferencesFilename);
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM,
              base::PathExists(protected_pref_file));

    std::unique_ptr<base::DictionaryValue> unprotected_preferences(
        ReadPrefsDictionary(unprotected_pref_file));
    if (!unprotected_preferences)
      return false;

    std::unique_ptr<base::DictionaryValue> protected_preferences;
    if (protection_level_ > PROTECTION_DISABLED_ON_PLATFORM) {
      protected_preferences = ReadPrefsDictionary(protected_pref_file);
      if (!protected_preferences)
        return false;
    }

    // Let the underlying test modify the preferences.
    AttackPreferencesOnDisk(unprotected_preferences.get(),
                            protected_preferences.get());

    // Write the modified preferences back to disk.

    JSONFileValueSerializer unprotected_prefs_serializer(unprotected_pref_file);
    EXPECT_TRUE(
        unprotected_prefs_serializer.Serialize(*unprotected_preferences));

    if (protected_preferences) {
      JSONFileValueSerializer protected_prefs_serializer(protected_pref_file);
      EXPECT_TRUE(protected_prefs_serializer.Serialize(*protected_preferences));
    }

    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();

#if defined(OS_WIN)
    // Avoid polluting prefs for the user and the bots by writing to a specific
    // testing registry path.
    registry_key_for_external_validation_ = GetRegistryPathForTestProfile();
    ProfilePrefStoreManager::SetPreferenceValidationRegistryPathForTesting(
        &registry_key_for_external_validation_);

    // Keys should be unique, but to avoid flakes in the long run make sure an
    // identical test key wasn't left behind by a previous test.
    if (content::IsPreTest()) {
      base::win::RegKey key;
      if (key.Open(HKEY_CURRENT_USER,
                   registry_key_for_external_validation_.c_str(),
                   KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
        LONG result = key.DeleteKey(L"");
        ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
      }
    }
#endif
  }

  void TearDown() override {
#if defined(OS_WIN)
    // When done, delete the Registry key to avoid polluting the registry.
    if (!content::IsPreTest()) {
      base::string16 registry_key = GetRegistryPathForTestProfile();
      base::win::RegKey key;
      if (key.Open(HKEY_CURRENT_USER, registry_key.c_str(),
                   KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
        LONG result = key.DeleteKey(L"");
        ASSERT_TRUE(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
      }
    }
#endif
    extensions::ExtensionBrowserTest::TearDown();
  }

  // In the PRE_ test, find the number of tracked preferences that were
  // initialized and save it to a file to be read back in the main test and used
  // as the total number of tracked preferences.
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    // File in which the PRE_ test will save the number of tracked preferences
    // on this platform.
    const char kNumTrackedPrefFilename[] = "NumTrackedPrefs";

    base::FilePath num_tracked_prefs_file;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_USER_DATA, &num_tracked_prefs_file));
    num_tracked_prefs_file =
        num_tracked_prefs_file.AppendASCII(kNumTrackedPrefFilename);

    if (content::IsPreTest()) {
      num_tracked_prefs_ = GetTrackedPrefHistogramCount(
          user_prefs::tracked::kTrackedPrefHistogramNullInitialized, ALLOW_ANY);
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM,
                num_tracked_prefs_ > 0);

      // Split tracked prefs are reported as Unchanged not as NullInitialized
      // when an empty dictionary is encountered on first run (this should only
      // hit for pref #5 in the current design).
      int num_split_tracked_prefs = GetTrackedPrefHistogramCount(
          user_prefs::tracked::kTrackedPrefHistogramUnchanged,
          BEGIN_ALLOW_SINGLE_BUCKET + 5);
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                num_split_tracked_prefs);

      if (SupportsRegistryValidation()) {
        // Same checks as above, but for the registry.
        num_tracked_prefs_ = GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
            user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
            ALLOW_ANY);
        EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM,
                  num_tracked_prefs_ > 0);

        int num_split_tracked_prefs = GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged,
            user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
            BEGIN_ALLOW_SINGLE_BUCKET + 5);
        EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                  num_split_tracked_prefs);
      }

      num_tracked_prefs_ += num_split_tracked_prefs;

      std::string num_tracked_prefs_str =
          base::NumberToString(num_tracked_prefs_);
      EXPECT_EQ(static_cast<int>(num_tracked_prefs_str.size()),
                base::WriteFile(num_tracked_prefs_file,
                                num_tracked_prefs_str.c_str(),
                                num_tracked_prefs_str.size()));
    } else {
      std::string num_tracked_prefs_str;
      EXPECT_TRUE(base::ReadFileToString(num_tracked_prefs_file,
                                         &num_tracked_prefs_str));
      EXPECT_TRUE(
          base::StringToInt(num_tracked_prefs_str, &num_tracked_prefs_));
    }
  }

 protected:
  // Called from the PRE_ test's body. Overrides should use it to setup
  // preferences through Chrome.
  virtual void SetupPreferences() = 0;

  // Called prior to the main test launching its browser. Overrides should use
  // it to attack preferences. |(un)protected_preferences| represent the state
  // on disk prior to launching the main test, they can be modified by this
  // method and modifications will be flushed back to disk before launching the
  // main test. |unprotected_preferences| is never NULL, |protected_preferences|
  // may be NULL if in PROTECTION_DISABLED_ON_PLATFORM mode.
  virtual void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) = 0;

  // Called from the body of the main test. Overrides should use it to verify
  // that the browser had the desired reaction when faced when the attack
  // orchestrated in AttackPreferencesOnDisk().
  virtual void VerifyReactionToPrefAttack() = 0;

  int num_tracked_prefs() const { return num_tracked_prefs_; }

  const SettingsProtectionLevel protection_level_;

 private:
  SettingsProtectionLevel GetProtectionLevelFromTrialGroup(
      const std::string& trial_group) {
    if (!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking)
      return PROTECTION_DISABLED_ON_PLATFORM;

// Protection levels can't be adjusted via --force-fieldtrials in official
// builds.
#if defined(OFFICIAL_BUILD)

#if defined(OS_WIN) || defined(OS_MACOSX)
    // The strongest mode is enforced on Windows and MacOS in the absence of a
    // field trial.
    return PROTECTION_ENABLED_ALL;
#else
    return PROTECTION_DISABLED_FOR_GROUP;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#else  // defined(OFFICIAL_BUILD)

    namespace internals = chrome_prefs::internals;
    if (trial_group == internals::kSettingsEnforcementGroupNoEnforcement)
      return PROTECTION_DISABLED_FOR_GROUP;
    if (trial_group == internals::kSettingsEnforcementGroupEnforceAlways)
      return PROTECTION_ENABLED_BASIC;
    if (trial_group == internals::kSettingsEnforcementGroupEnforceAlwaysWithDSE)
      return PROTECTION_ENABLED_DSE;
    if (trial_group ==
        internals::kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE) {
      return PROTECTION_ENABLED_EXTENSIONS;
    }
    ADD_FAILURE();
    return static_cast<SettingsProtectionLevel>(-1);
#endif  // defined(OFFICIAL_BUILD)
  }

  int num_tracked_prefs_;

#if defined(OS_WIN)
  base::string16 registry_key_for_external_validation_;
#endif
};

}  // namespace

// Verifies that nothing is reset when nothing is tampered with.
// Also sanity checks that the expected preferences files are in place.
class PrefHashBrowserTestUnchangedDefault : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    // Default Chrome setup.
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    // No attack.
  }

  void VerifyReactionToPrefAttack() override {
    // Expect all prefs to be reported as Unchanged with no resets.
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs()
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset, ALLOW_NONE));

    // Nothing else should have triggered.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramChanged, ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramCleared, ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      // Expect all prefs to be reported as Unchanged.
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
                    ? num_tracked_prefs()
                    : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramUnchanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    ALLOW_ANY));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestUnchangedDefault, UnchangedDefault);

// Augments PrefHashBrowserTestUnchangedDefault to confirm that nothing is reset
// when nothing is tampered with, even if Chrome itself wrote custom prefs in
// its last run.
class PrefHashBrowserTestUnchangedCustom
    : public PrefHashBrowserTestUnchangedDefault {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetString(prefs::kHomePage, "http://example.com");

    InstallExtensionWithUIAutoConfirm(
        test_data_dir_.AppendASCII("good.crx"), 1, browser());
  }

  void VerifyReactionToPrefAttack() override {
    // Make sure the settings written in the last run stuck.
    EXPECT_EQ("http://example.com",
              profile()->GetPrefs()->GetString(prefs::kHomePage));

    EXPECT_TRUE(extension_registry()->enabled_extensions().GetByID(kGoodCrxId));

    // Reaction should be identical to unattacked default prefs.
    PrefHashBrowserTestUnchangedDefault::VerifyReactionToPrefAttack();
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestUnchangedCustom, UnchangedCustom);

// Verifies that cleared prefs are reported.
class PrefHashBrowserTestClearedAtomic : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetString(prefs::kHomePage, "http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    base::DictionaryValue* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_BASIC ? protected_preferences
                                                      : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    EXPECT_TRUE(selected_prefs);
    EXPECT_TRUE(selected_prefs->Remove(prefs::kHomePage, NULL));
  }

  void VerifyReactionToPrefAttack() override {
    // The clearance of homepage should have been noticed (as pref #2 being
    // cleared), but shouldn't have triggered a reset (as there is nothing we
    // can do when the pref is already gone).
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramCleared,
                  BEGIN_ALLOW_SINGLE_BUCKET + 2));
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs() - 1
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset, ALLOW_NONE));

    // Nothing else should have triggered.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramChanged, ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      // Expect homepage clearance to have been noticed by registry validation.
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramCleared,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 2));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestClearedAtomic, ClearedAtomic);

// Verifies that clearing the MACs results in untrusted Initialized pings for
// non-null protected prefs.
class PrefHashBrowserTestUntrustedInitialized : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    // Explicitly set the DSE (it's otherwise NULL by default, preventing
    // thorough testing of the PROTECTION_ENABLED_DSE level).
    DefaultSearchManager default_search_manager(
        profile()->GetPrefs(), DefaultSearchManager::ObserverCallback());
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);

    const TemplateURLData* default_template_url_data =
        default_search_manager.GetDefaultSearchEngine(&dse_source);
    EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, dse_source);

    default_search_manager.SetUserSelectedDefaultSearchEngine(
        *default_template_url_data);

    default_search_manager.GetDefaultSearchEngine(&dse_source);
    EXPECT_EQ(DefaultSearchManager::FROM_USER, dse_source);

    // Also explicitly set an atomic pref that falls under
    // PROTECTION_ENABLED_BASIC.
    profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup,
                                      SessionStartupPref::URLS);
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    unprotected_preferences->Remove("protection.macs", NULL);
    if (protected_preferences)
      protected_preferences->Remove("protection.macs", NULL);
  }

  void VerifyReactionToPrefAttack() override {
    // Preferences that are NULL by default will be NullInitialized.
    int num_null_values = GetTrackedPrefHistogramCount(
        user_prefs::tracked::kTrackedPrefHistogramNullInitialized, ALLOW_ANY);
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM,
              num_null_values > 0);
    if (num_null_values > 0) {
      // This test requires that at least 3 prefs be non-null (extensions, DSE,
      // and 1 atomic pref explictly set for this test above).
      EXPECT_GE(num_tracked_prefs() - num_null_values, 3);
    }

    // Expect all non-null prefs to be reported as Initialized (with
    // accompanying resets or wanted resets based on the current protection
    // level).
    EXPECT_EQ(
        num_tracked_prefs() - num_null_values,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramInitialized, ALLOW_ANY));

    int num_protected_prefs = 0;
    // A switch statement falling through each protection level in decreasing
    // levels of protection to add expectations for each level which augments
    // the previous one.
    switch (protection_level_) {
      case PROTECTION_ENABLED_ALL:
      case PROTECTION_ENABLED_EXTENSIONS:
        ++num_protected_prefs;
        FALLTHROUGH;
      case PROTECTION_ENABLED_DSE:
        ++num_protected_prefs;
        FALLTHROUGH;
      case PROTECTION_ENABLED_BASIC:
        num_protected_prefs += num_tracked_prefs() - num_null_values - 2;
        FALLTHROUGH;
      case PROTECTION_DISABLED_FOR_GROUP:
      case PROTECTION_DISABLED_ON_PLATFORM:
        // No protection.
        break;
    }

    EXPECT_EQ(
        num_tracked_prefs() - num_null_values - num_protected_prefs,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramWantedReset, ALLOW_ANY));
    EXPECT_EQ(num_protected_prefs,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset, ALLOW_ANY));

    // Explicitly verify the result of reported resets.

    DefaultSearchManager default_search_manager(
        profile()->GetPrefs(), DefaultSearchManager::ObserverCallback());
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);
    default_search_manager.GetDefaultSearchEngine(&dse_source);
    EXPECT_EQ(protection_level_ < PROTECTION_ENABLED_DSE
                  ? DefaultSearchManager::FROM_USER
                  : DefaultSearchManager::FROM_FALLBACK,
              dse_source);

    EXPECT_EQ(protection_level_ < PROTECTION_ENABLED_BASIC,
              profile()->GetPrefs()->GetInteger(prefs::kRestoreOnStartup) ==
                  SessionStartupPref::URLS);

    // Nothing else should have triggered.
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramUnchanged,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramChanged, ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramCleared, ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      // The MACs have been cleared but the preferences have not been tampered.
      // The registry should report all prefs as unchanged.
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
                    ? num_tracked_prefs()
                    : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramUnchanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    ALLOW_ANY));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestUntrustedInitialized,
                       UntrustedInitialized);

// Verifies that changing an atomic pref results in it being reported (and reset
// if the protection level allows it).
class PrefHashBrowserTestChangedAtomic : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup,
                                      SessionStartupPref::URLS);

    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kURLsToRestoreOnStartup);
    update->AppendString("http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    base::DictionaryValue* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_BASIC ? protected_preferences
                                                      : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    EXPECT_TRUE(selected_prefs);
    base::ListValue* startup_urls;
    EXPECT_TRUE(
        selected_prefs->GetList(prefs::kURLsToRestoreOnStartup, &startup_urls));
    EXPECT_TRUE(startup_urls);
    EXPECT_EQ(1U, startup_urls->GetSize());
    startup_urls->AppendString("http://example.org");
  }

  void VerifyReactionToPrefAttack() override {
    // Expect a single Changed event for tracked pref #4 (startup URLs).
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 4));
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs() - 1
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));

    EXPECT_EQ((protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
               protection_level_ < PROTECTION_ENABLED_BASIC)
                  ? 1
                  : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 4));
    EXPECT_EQ(protection_level_ >= PROTECTION_ENABLED_BASIC ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 4));

// TODO(gab): This doesn't work on OS_CHROMEOS because we fail to attack
// Preferences.
#if !defined(OS_CHROMEOS)
    // Explicitly verify the result of reported resets.
    EXPECT_EQ(protection_level_ >= PROTECTION_ENABLED_BASIC ? 0U : 2U,
              profile()
                  ->GetPrefs()
                  ->GetList(prefs::kURLsToRestoreOnStartup)
                  ->GetSize());
#endif

    // Nothing else should have triggered.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramCleared, ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      // Expect a single Changed event for tracked pref #4 (startup URLs).
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 4));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestChangedAtomic, ChangedAtomic);

// Verifies that changing or adding an entry in a split pref results in both
// items being reported (and remove if the protection level allows it).
class PrefHashBrowserTestChangedSplitPref : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    InstallExtensionWithUIAutoConfirm(
        test_data_dir_.AppendASCII("good.crx"), 1, browser());
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    base::DictionaryValue* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_EXTENSIONS
            ? protected_preferences
            : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    EXPECT_TRUE(selected_prefs);
    base::DictionaryValue* extensions_dict;
    EXPECT_TRUE(selected_prefs->GetDictionary(
        extensions::pref_names::kExtensions, &extensions_dict));
    EXPECT_TRUE(extensions_dict);

    // Tamper with any installed setting for good.crx
    base::DictionaryValue* good_crx_dict;
    EXPECT_TRUE(extensions_dict->GetDictionary(kGoodCrxId, &good_crx_dict));
    int good_crx_state;
    EXPECT_TRUE(good_crx_dict->GetInteger("state", &good_crx_state));
    EXPECT_EQ(extensions::Extension::ENABLED, good_crx_state);
    good_crx_dict->SetInteger("state", extensions::Extension::DISABLED);

    // Drop a fake extension (for the purpose of this test, dropped settings
    // don't need to be valid extension settings).
    auto fake_extension = std::make_unique<base::DictionaryValue>();
    fake_extension->SetString("name", "foo");
    extensions_dict->Set(std::string(32, 'a'), std::move(fake_extension));
  }

  void VerifyReactionToPrefAttack() override {
    // Expect a single split pref changed report with a count of 2 for tracked
    // pref #5 (extensions).
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 5));
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  "Settings.TrackedSplitPreferenceChanged.extensions.settings",
                  BEGIN_ALLOW_SINGLE_BUCKET + 2));

    // Everything else should have remained unchanged.
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs() - 1
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));

    EXPECT_EQ((protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
               protection_level_ < PROTECTION_ENABLED_EXTENSIONS)
                  ? 1
                  : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 5));
    EXPECT_EQ(protection_level_ >= PROTECTION_ENABLED_EXTENSIONS ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 5));

    EXPECT_EQ(
        protection_level_ < PROTECTION_ENABLED_EXTENSIONS,
        extension_registry()->GetExtensionById(
            kGoodCrxId, extensions::ExtensionRegistry::EVERYTHING) != nullptr);

    // Nothing else should have triggered.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramCleared, ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      // Expect that the registry validation caught the invalid MAC in split
      // pref #5 (extensions).
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 5));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestChangedSplitPref, ChangedSplitPref);

// Verifies that adding a value to unprotected preferences for a key which is
// still using the default (i.e. has no value stored in protected preferences)
// doesn't allow that value to slip in with no valid MAC (regression test for
// http://crbug.com/414554)
class PrefHashBrowserTestUntrustedAdditionToPrefs
    : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    // Ensure there is no user-selected value for kRestoreOnStartup.
    EXPECT_FALSE(
        profile()->GetPrefs()->GetUserPrefValue(prefs::kRestoreOnStartup));
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    unprotected_preferences->SetInteger(prefs::kRestoreOnStartup,
                                        SessionStartupPref::LAST);
  }

  void VerifyReactionToPrefAttack() override {
    // Expect a single Changed event for tracked pref #3 (kRestoreOnStartup) if
    // not protecting; if protection is enabled the change should be a no-op.
    int changed_expected =
        protection_level_ == PROTECTION_DISABLED_FOR_GROUP ? 1 : 0;
    EXPECT_EQ((protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
               protection_level_ < PROTECTION_ENABLED_BASIC)
                  ? changed_expected
                  : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 3));
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs() - changed_expected
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));

    EXPECT_EQ((protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
               protection_level_ < PROTECTION_ENABLED_BASIC)
                  ? 1
                  : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 3));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset, ALLOW_NONE));

    // Nothing else should have triggered.
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramCleared, ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      EXPECT_EQ((protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
                 protection_level_ < PROTECTION_ENABLED_BASIC)
                    ? changed_expected
                    : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 3));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestUntrustedAdditionToPrefs,
                       UntrustedAdditionToPrefs);

// Verifies that adding a value to unprotected preferences while wiping a
// user-selected value from protected preferences doesn't allow that value to
// slip in with no valid MAC (regression test for http://crbug.com/414554).
class PrefHashBrowserTestUntrustedAdditionToPrefsAfterWipe
    : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetString(prefs::kHomePage, "http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    // Set or change the value in Preferences to the attacker's choice.
    unprotected_preferences->SetString(prefs::kHomePage, "http://example.net");
    // Clear the value in Secure Preferences, if any.
    if (protected_preferences)
      protected_preferences->Remove(prefs::kHomePage, NULL);
  }

  void VerifyReactionToPrefAttack() override {
    // Expect a single Changed event for tracked pref #2 (kHomePage) if
    // not protecting; if protection is enabled the change should be a Cleared.
    int changed_expected =
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM &&
        protection_level_ < PROTECTION_ENABLED_BASIC
        ? 1 : 0;
    int cleared_expected =
        protection_level_ >= PROTECTION_ENABLED_BASIC
        ? 1 : 0;
    EXPECT_EQ(changed_expected,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 2));
    EXPECT_EQ(cleared_expected,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramCleared,
                  BEGIN_ALLOW_SINGLE_BUCKET + 2));
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs() - changed_expected - cleared_expected
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));

    EXPECT_EQ(changed_expected,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramWantedReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 2));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset, ALLOW_NONE));

    // Nothing else should have triggered.
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
                  ALLOW_NONE));
    EXPECT_EQ(0, GetTrackedPrefHistogramCount(
                     user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
                     ALLOW_NONE));
    EXPECT_EQ(
        0, GetTrackedPrefHistogramCount(
               user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
               ALLOW_NONE));

    if (SupportsRegistryValidation()) {
      EXPECT_EQ(changed_expected,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 2));
      EXPECT_EQ(cleared_expected,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramCleared,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 2));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestUntrustedAdditionToPrefsAfterWipe,
                       UntrustedAdditionToPrefsAfterWipe);

#if defined(OS_WIN)
class PrefHashBrowserTestRegistryValidationFailure
    : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetString(prefs::kHomePage, "http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    base::string16 registry_key =
        GetRegistryPathForTestProfile() + L"\\PreferenceMACs\\Default";
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, registry_key.c_str(),
                                      KEY_SET_VALUE | KEY_WOW64_32KEY));
    // An incorrect hash should still have the correct size.
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"homepage", base::string16(64, 'A').c_str()));
  }

  void VerifyReactionToPrefAttack() override {
    EXPECT_EQ(
        protection_level_ > PROTECTION_DISABLED_ON_PLATFORM
            ? num_tracked_prefs()
            : 0,
        GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged, ALLOW_ANY));

    if (SupportsRegistryValidation()) {
      // Expect that the registry validation caught the invalid MAC for pref #2
      // (homepage).
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 2));
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestRegistryValidationFailure,
                       RegistryValidationFailure);
#endif

// Verifies that all preferences related to choice of default search engine are
// protected.
class PrefHashBrowserTestDefaultSearch : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    // Set user selected default search engine.
    DefaultSearchManager default_search_manager(
        profile()->GetPrefs(), DefaultSearchManager::ObserverCallback());
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);

    TemplateURLData user_dse;
    user_dse.SetKeyword(base::UTF8ToUTF16("userkeyword"));
    user_dse.SetShortName(base::UTF8ToUTF16("username"));
    user_dse.SetURL("http://user_default_engine/search?q=good_user_query");
    default_search_manager.SetUserSelectedDefaultSearchEngine(user_dse);

    const TemplateURLData* current_dse =
        default_search_manager.GetDefaultSearchEngine(&dse_source);
    EXPECT_EQ(DefaultSearchManager::FROM_USER, dse_source);
    EXPECT_EQ(current_dse->keyword(), base::UTF8ToUTF16("userkeyword"));
    EXPECT_EQ(current_dse->short_name(), base::UTF8ToUTF16("username"));
    EXPECT_EQ(current_dse->url(),
              "http://user_default_engine/search?q=good_user_query");
  }

  void AttackPreferencesOnDisk(
      base::DictionaryValue* unprotected_preferences,
      base::DictionaryValue* protected_preferences) override {
    static constexpr char default_search_provider_data[] = R"(
    {
      "default_search_provider_data" : {
        "template_url_data" : {
          "keyword" : "badkeyword",
          "short_name" : "badname",
          "url" : "http://bad_default_engine/search?q=dirty_user_query"
        }
      }
    })";
    static constexpr char search_provider_overrides[] = R"(
    {
      "search_provider_overrides" : [
        {
          "keyword" : "badkeyword",
          "name" : "badname",
          "search_url" : "http://bad_default_engine/search?q=dirty_user_query",
          "encoding" : "utf-8",
          "id" : 1
        }, {
          "keyword" : "badkeyword2",
          "name" : "badname2",
          "search_url" : "http://bad_default_engine2/search?q=dirty_user_query",
          "encoding" : "utf-8",
          "id" : 2
        }
      ]
    })";

    // Try to override default search in all three of available preferences.
    auto attack1 = base::DictionaryValue::From(
        base::JSONReader::ReadDeprecated(default_search_provider_data));
    auto attack2 = base::DictionaryValue::From(
        base::JSONReader::ReadDeprecated(search_provider_overrides));
    unprotected_preferences->MergeDictionary(attack1.get());
    unprotected_preferences->MergeDictionary(attack2.get());
    if (protected_preferences) {
      // Override here, too.
      protected_preferences->MergeDictionary(attack1.get());
      protected_preferences->MergeDictionary(attack2.get());
    }
  }

  void VerifyReactionToPrefAttack() override {
    DefaultSearchManager default_search_manager(
        profile()->GetPrefs(), DefaultSearchManager::ObserverCallback());
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);

    const TemplateURLData* current_dse =
        default_search_manager.GetDefaultSearchEngine(&dse_source);

    if (protection_level_ < PROTECTION_ENABLED_DSE) {
// This doesn't work on OS_CHROMEOS because we fail to attack Preferences.
#if !defined(OS_CHROMEOS)
      // Attack is successful.
      EXPECT_EQ(DefaultSearchManager::FROM_USER, dse_source);
      EXPECT_EQ(current_dse->keyword(), base::UTF8ToUTF16("badkeyword"));
      EXPECT_EQ(current_dse->short_name(), base::UTF8ToUTF16("badname"));
      EXPECT_EQ(current_dse->url(),
                "http://bad_default_engine/search?q=dirty_user_query");
#endif
    } else {
      // Attack fails.
      EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, dse_source);
      EXPECT_NE(current_dse->keyword(), base::UTF8ToUTF16("badkeyword"));
      EXPECT_NE(current_dse->short_name(), base::UTF8ToUTF16("badname"));
      EXPECT_NE(current_dse->url(),
                "http://bad_default_engine/search?q=dirty_user_query");
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestDefaultSearch, SearchProtected);
