// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/profile_pref_store_manager.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "services/preferences/public/cpp/tracked/tracked_preference_histogram_names.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_WIN)
std::wstring GetRegistryPathForTestProfile() {
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

  // |DIR_USER_DATA| usually has format %TMP%\12345_6789012345\user_data
  // (unless running with --single-process-tests, where the format is
  // %TMP%\scoped_dir12345_6789012345). Use the parent directory name instead of
  // the leaf directory name "user_data" to avoid conflicts in parallel tests,
  // which would try to modify the same registry key otherwise.
  if (profile_dir.BaseName().value() == L"user_data") {
    profile_dir = profile_dir.DirName();
  }
  // Try to detect regressions when |DIR_USER_DATA| test location changes, which
  // could cause this test to become flaky. See http://crbug/1091409 for more
  // details.
  DCHECK(profile_dir.BaseName().value().find_first_of(L"0123456789") !=
         std::string::npos);

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::optional<base::Value::Dict> ReadPrefsDictionary(
    const base::FilePath& pref_file) {
  JSONFileValueDeserializer deserializer(pref_file);
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::string error_str;
  std::unique_ptr<base::Value> prefs =
      deserializer.Deserialize(&error_code, &error_str);
  if (!prefs || error_code != JSONFileValueDeserializer::JSON_NO_ERROR) {
    ADD_FAILURE() << "Error #" << error_code << ": " << error_str;
    return std::nullopt;
  }
  if (!prefs->is_dict()) {
    ADD_FAILURE();
    return std::nullopt;
  }
  return std::move(*prefs).TakeDict();
}
#endif

// Returns whether external validation is supported on the platform through
// storing MACs in the registry.
bool SupportsRegistryValidation() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

#define PREF_HASH_BROWSER_TEST(fixture, test_name)                             \
  IN_PROC_BROWSER_TEST_F(fixture, PRE_##test_name) { SetupPreferences(); }     \
  IN_PROC_BROWSER_TEST_F(fixture, test_name) { VerifyReactionToPrefAttack(); } \
  static_assert(true, "")

// A base fixture designed such that implementations do two things:
//  1) Override all three pure-virtual methods below to setup, attack, and
//     verify preferences throughout the tests provided by this fixture.
//  2) Instantiate their test via the PREF_HASH_BROWSER_TEST macro above.
// Based on top of ExtensionBrowserTest to allow easy interaction with the
// ExtensionRegistry.
class PrefHashBrowserTestBase : public extensions::ExtensionBrowserTest {
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

  PrefHashBrowserTestBase() : protection_level_(GetProtectionLevel()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  bool SetUpUserDataDirectory() override {
    // Do the normal setup in the PRE test and attack preferences in the main
    // test.
    if (content::IsPreTest())
      return extensions::ExtensionBrowserTest::SetUpUserDataDirectory();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // For some reason, the Preferences file does not exist in the location
    // below on Chrome OS. Since protection is disabled on Chrome OS, it's okay
    // to simply not attack preferences at all (and still assert that no
    // hardening related histogram kicked in in VerifyReactionToPrefAttack()).
    // TODO(gab): Figure out why there is no Preferences file in this location
    // on Chrome OS (and re-enable the section disabled for OS_CHROMEOS further
    // below).
    EXPECT_EQ(PROTECTION_DISABLED_ON_PLATFORM, protection_level_);
    return true;
#else
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

    std::optional<base::Value::Dict> unprotected_preferences(
        ReadPrefsDictionary(unprotected_pref_file));
    if (!unprotected_preferences)
      return false;

    std::optional<base::Value::Dict> protected_preferences;
    if (protection_level_ > PROTECTION_DISABLED_ON_PLATFORM) {
      protected_preferences = ReadPrefsDictionary(protected_pref_file);
      if (!protected_preferences)
        return false;
    }

    // Let the underlying test modify the preferences.
    AttackPreferencesOnDisk(
        &unprotected_preferences.value(),
        protected_preferences ? &protected_preferences.value() : nullptr);

    // Write the modified preferences back to disk.
    JSONFileValueSerializer unprotected_prefs_serializer(unprotected_pref_file);
    EXPECT_TRUE(
        unprotected_prefs_serializer.Serialize(*unprotected_preferences));

    if (protected_preferences) {
      JSONFileValueSerializer protected_prefs_serializer(protected_pref_file);
      EXPECT_TRUE(protected_prefs_serializer.Serialize(*protected_preferences));
    }

    return true;
#endif
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();

    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();

#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_WIN)
    // When done, delete the Registry key to avoid polluting the registry.
    if (!content::IsPreTest()) {
      std::wstring registry_key = GetRegistryPathForTestProfile();
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

        int split_tracked_prefs = GetTrackedPrefHistogramCount(
            user_prefs::tracked::kTrackedPrefHistogramUnchanged,
            user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
            BEGIN_ALLOW_SINGLE_BUCKET + 5);
        EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                  split_tracked_prefs);
      }

      num_tracked_prefs_ += num_split_tracked_prefs;

      std::string num_tracked_prefs_str =
          base::NumberToString(num_tracked_prefs_);
      EXPECT_TRUE(
          base::WriteFile(num_tracked_prefs_file, num_tracked_prefs_str));
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) = 0;

  // Called from the body of the main test. Overrides should use it to verify
  // that the browser had the desired reaction when faced when the attack
  // orchestrated in AttackPreferencesOnDisk().
  virtual void VerifyReactionToPrefAttack() = 0;

  int num_tracked_prefs() const { return num_tracked_prefs_; }

  const SettingsProtectionLevel protection_level_;

 private:
  SettingsProtectionLevel GetProtectionLevel() {
    if (!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking)
      return PROTECTION_DISABLED_ON_PLATFORM;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    // The strongest mode is enforced on Windows and MacOS in the absence of a
    // field trial.
    return PROTECTION_ENABLED_ALL;
#else
    return PROTECTION_DISABLED_FOR_GROUP;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  }

  int num_tracked_prefs_;

#if BUILDFLAG(IS_WIN)
  std::wstring registry_key_for_external_validation_;
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    base::Value::Dict* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_BASIC ? protected_preferences
                                                      : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    EXPECT_TRUE(selected_prefs);
    EXPECT_TRUE(selected_prefs->Remove(prefs::kHomePage));
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
        profile()->GetPrefs(),
        search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
            profile()),
        DefaultSearchManager::ObserverCallback()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        profile()->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    unprotected_preferences->RemoveByDottedPath("protection.macs");
    if (protected_preferences)
      protected_preferences->RemoveByDottedPath("protection.macs");
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
        [[fallthrough]];
      case PROTECTION_ENABLED_DSE:
        ++num_protected_prefs;
        [[fallthrough]];
      case PROTECTION_ENABLED_BASIC:
        num_protected_prefs += num_tracked_prefs() - num_null_values - 2;
        [[fallthrough]];
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
        profile()->GetPrefs(),
        search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
            profile()),
        DefaultSearchManager::ObserverCallback()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        profile()->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
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

    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kURLsToRestoreOnStartup);
    update->Append("http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    base::Value::Dict* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_BASIC ? protected_preferences
                                                      : unprotected_preferences;
    // `selected_prefs` should never be NULL under the protection level picking
    // it.
    ASSERT_TRUE(selected_prefs);
    base::Value::List* startup_urls =
        selected_prefs->FindListByDottedPath(prefs::kURLsToRestoreOnStartup);
    ASSERT_TRUE(startup_urls);
    EXPECT_EQ(1U, startup_urls->size());
    startup_urls->Append("http://example.org");
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly verify the result of reported resets.
    EXPECT_EQ(
        protection_level_ >= PROTECTION_ENABLED_BASIC ? 0U : 2U,
        profile()->GetPrefs()->GetList(prefs::kURLsToRestoreOnStartup).size());
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    base::Value::Dict* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_EXTENSIONS
            ? protected_preferences
            : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    EXPECT_TRUE(selected_prefs);
    base::Value::Dict* extensions_dict = selected_prefs->FindDictByDottedPath(
        extensions::pref_names::kExtensions);
    EXPECT_TRUE(extensions_dict);

    // Tamper with any installed setting for good.crx
    base::Value::Dict* good_crx_dict = extensions_dict->FindDict(kGoodCrxId);
    ASSERT_TRUE(good_crx_dict);
    std::optional<int> good_crx_state = good_crx_dict->FindInt("state");
    ASSERT_TRUE(good_crx_state);
    EXPECT_EQ(extensions::Extension::ENABLED, *good_crx_state);
    good_crx_dict->Set("state", extensions::Extension::DISABLED);

    // Drop a fake extension (for the purpose of this test, dropped settings
    // don't need to be valid extension settings).
    base::Value::Dict fake_extension;
    fake_extension.Set("name", "foo");
    extensions_dict->Set(std::string(32, 'a'), std::move(fake_extension));
  }

  void VerifyReactionToPrefAttack() override {
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 5));

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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    unprotected_preferences->SetByDottedPath(
        prefs::kRestoreOnStartup, static_cast<int>(SessionStartupPref::LAST));
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
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    // Set or change the value in Preferences to the attacker's choice.
    unprotected_preferences->Set(prefs::kHomePage, "http://example.net");
    // Clear the value in Secure Preferences, if any.
    if (protected_preferences)
      protected_preferences->Remove(prefs::kHomePage);
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

#if BUILDFLAG(IS_WIN)
class PrefHashBrowserTestRegistryValidationFailure
    : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    profile()->GetPrefs()->SetString(prefs::kHomePage, "http://example.com");
  }

  void AttackPreferencesOnDisk(
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    std::wstring registry_key =
        GetRegistryPathForTestProfile() + L"\\PreferenceMACs\\Default";
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, registry_key.c_str(),
                                      KEY_SET_VALUE | KEY_WOW64_32KEY));
    // An incorrect hash should still have the correct size.
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"homepage", std::wstring(64, 'A').c_str()));
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
        profile()->GetPrefs(),
        search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
            profile()),
        DefaultSearchManager::ObserverCallback()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        profile()->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);

    TemplateURLData user_dse;
    user_dse.SetKeyword(u"userkeyword");
    user_dse.SetShortName(u"username");
    user_dse.SetURL("http://user_default_engine/search?q=good_user_query");
    default_search_manager.SetUserSelectedDefaultSearchEngine(user_dse);

    const TemplateURLData* current_dse =
        default_search_manager.GetDefaultSearchEngine(&dse_source);
    EXPECT_EQ(DefaultSearchManager::FROM_USER, dse_source);
    EXPECT_EQ(current_dse->keyword(), u"userkeyword");
    EXPECT_EQ(current_dse->short_name(), u"username");
    EXPECT_EQ(current_dse->url(),
              "http://user_default_engine/search?q=good_user_query");
  }

  void AttackPreferencesOnDisk(
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
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
    base::Value attack1 = *base::JSONReader::Read(default_search_provider_data);
    base::Value attack2 = *base::JSONReader::Read(search_provider_overrides);
    unprotected_preferences->Merge(attack1.GetDict().Clone());
    unprotected_preferences->Merge(attack2.GetDict().Clone());
    if (protected_preferences) {
      // Override here, too.
      protected_preferences->Merge(attack1.GetDict().Clone());
      protected_preferences->Merge(attack2.GetDict().Clone());
    }
  }

  void VerifyReactionToPrefAttack() override {
    DefaultSearchManager default_search_manager(
        profile()->GetPrefs(),
        search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
            profile()),
        DefaultSearchManager::ObserverCallback()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        profile()->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
    DefaultSearchManager::Source dse_source =
        static_cast<DefaultSearchManager::Source>(-1);

    const TemplateURLData* current_dse =
        default_search_manager.GetDefaultSearchEngine(&dse_source);

    if (protection_level_ < PROTECTION_ENABLED_DSE) {
// This doesn't work on OS_CHROMEOS because we fail to attack Preferences.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      // Attack is successful.
      EXPECT_EQ(DefaultSearchManager::FROM_USER, dse_source);
      EXPECT_EQ(current_dse->keyword(), u"badkeyword");
      EXPECT_EQ(current_dse->short_name(), u"badname");
      EXPECT_EQ(current_dse->url(),
                "http://bad_default_engine/search?q=dirty_user_query");
#endif
    } else {
      // Attack fails.
      EXPECT_EQ(DefaultSearchManager::FROM_FALLBACK, dse_source);
      EXPECT_NE(current_dse->keyword(), u"badkeyword");
      EXPECT_NE(current_dse->short_name(), u"badname");
      EXPECT_NE(current_dse->url(),
                "http://bad_default_engine/search?q=dirty_user_query");
    }
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestDefaultSearch, SearchProtected);

// Verifies that we handle a protected Dict preference being changed to an
// unexpected type (int). See https://crbug.com/1512724.
class PrefHashBrowserTestExtensionDictTypeChanged
    : public PrefHashBrowserTestBase {
 public:
  void SetupPreferences() override {
    InstallExtensionWithUIAutoConfirm(test_data_dir_.AppendASCII("good.crx"), 1,
                                      browser());
  }

  void AttackPreferencesOnDisk(
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    base::Value::Dict* const selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_EXTENSIONS
            ? protected_preferences
            : unprotected_preferences;
    // |selected_prefs| should never be NULL under the protection level picking
    // it.
    ASSERT_TRUE(selected_prefs);
    EXPECT_TRUE(selected_prefs->FindDictByDottedPath(
        extensions::pref_names::kExtensions));
    // Overwrite with an int (wrong type).
    selected_prefs->SetByDottedPath(extensions::pref_names::kExtensions, 13);
    EXPECT_EQ(13, selected_prefs
                      ->FindIntByDottedPath(extensions::pref_names::kExtensions)
                      .value());
  }

  void VerifyReactionToPrefAttack() override {
    // Setting the extensions dict to an invalid type gets noticed regardless
    // of protection level. This implementation just happened to be easier and
    // it doesn't seem important to not protect the kExtensions from being the
    // wrong type at any protection level. PrefService will correct the type
    // either way.
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramCleared,
                  BEGIN_ALLOW_SINGLE_BUCKET + 5));

    // Expect a dictionary for extensions. This shouldn't somehow explode.
    profile()->GetPrefs()->GetDict(extensions::pref_names::kExtensions);
  }
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestExtensionDictTypeChanged,
                       ExtensionDictTypeChanged);

// Verifies that changing the account value of a tracked pref results in it
// being reported under the same id as the local value (and reset if the
// protection level allows it).
class PrefHashBrowserTestAccountValueUntrustedAddition
    : public PrefHashBrowserTestBase {
 public:
  PrefHashBrowserTestAccountValueUntrustedAddition()
      : feature_list_(syncer::kEnablePreferencesAccountStorage) {}

  void SetupPreferences() override {
    EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  }

  void AttackPreferencesOnDisk(
      base::Value::Dict* unprotected_preferences,
      base::Value::Dict* protected_preferences) override {
    base::Value::Dict* selected_prefs =
        protection_level_ >= PROTECTION_ENABLED_BASIC ? protected_preferences
                                                      : unprotected_preferences;
    // `selected_prefs` should never be NULL under the protection level picking
    // it.
    ASSERT_TRUE(selected_prefs);
    selected_prefs->SetByDottedPath(
        base::StringPrintf("%s.%s", chrome_prefs::kAccountPreferencesPrefix,
                           prefs::kShowHomeButton),
        true);
  }

  void VerifyReactionToPrefAttack() override {
    // Expect a single Changed event for tracked pref #0 (show home button).
    EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramChanged,
                  BEGIN_ALLOW_SINGLE_BUCKET + 0));
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
                  BEGIN_ALLOW_SINGLE_BUCKET + 0));
    EXPECT_EQ(protection_level_ >= PROTECTION_ENABLED_BASIC ? 1 : 0,
              GetTrackedPrefHistogramCount(
                  user_prefs::tracked::kTrackedPrefHistogramReset,
                  BEGIN_ALLOW_SINGLE_BUCKET + 0));

// TODO(gab): This doesn't work on OS_CHROMEOS because we fail to attack
// Preferences.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly verify the result of reported resets.
    EXPECT_EQ(protection_level_ < PROTECTION_ENABLED_BASIC,
              profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
      // Expect a single Changed event for tracked pref #0 (show home button).
      EXPECT_EQ(protection_level_ > PROTECTION_DISABLED_ON_PLATFORM ? 1 : 0,
                GetTrackedPrefHistogramCount(
                    user_prefs::tracked::kTrackedPrefHistogramChanged,
                    user_prefs::tracked::kTrackedPrefRegistryValidationSuffix,
                    BEGIN_ALLOW_SINGLE_BUCKET + 0));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

PREF_HASH_BROWSER_TEST(PrefHashBrowserTestAccountValueUntrustedAddition,
                       AccountValueUntrustedAddition);
