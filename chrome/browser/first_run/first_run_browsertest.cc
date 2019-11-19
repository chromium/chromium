// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef InProcessBrowserTest FirstRunBrowserTest;

namespace first_run {

IN_PROC_BROWSER_TEST_F(FirstRunBrowserTest, SetShouldShowWelcomePage) {
  EXPECT_FALSE(ShouldShowWelcomePage());
  SetShouldShowWelcomePage();
  EXPECT_TRUE(ShouldShowWelcomePage());
  EXPECT_FALSE(ShouldShowWelcomePage());
}

#if !defined(OS_CHROMEOS)
namespace {

// A generic test class to be subclassed by test classes testing specific
// master_preferences. All subclasses must call SetMasterPreferencesForTest()
// from their SetUp() method before deferring the remainder of Setup() to this
// class.
class FirstRunMasterPrefsBrowserTestBase : public InProcessBrowserTest {
 public:
  FirstRunMasterPrefsBrowserTestBase() {}

 protected:
  void SetUp() override {
    // All users of this test class need to call SetMasterPreferencesForTest()
    // before this class' SetUp() is invoked.
    ASSERT_TRUE(text_.get());

    ASSERT_TRUE(base::CreateTemporaryFile(&prefs_file_));
    EXPECT_EQ(static_cast<int>(text_->size()),
              base::WriteFile(prefs_file_, text_->c_str(), text_->size()));
    SetMasterPrefsPathForTesting(prefs_file_);

    // This invokes BrowserMain, and does the import, so must be done last.
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    EXPECT_TRUE(base::DeleteFile(prefs_file_, false));
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
    EXPECT_EQ(AUTO_IMPORT_NONE, auto_import_state());

    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

#if defined(OS_MACOSX) || defined(OS_LINUX)
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Suppress first run dialog since it blocks test progress.
    internal::ForceFirstRunDialogShownForTesting(false);
  }
#endif

  void SetMasterPreferencesForTest(const char text[]) {
    text_.reset(new std::string(text));
  }

 private:
  base::FilePath prefs_file_;
  std::unique_ptr<std::string> text_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsBrowserTestBase);
};

template<const char Text[]>
class FirstRunMasterPrefsBrowserTestT
    : public FirstRunMasterPrefsBrowserTestBase {
 public:
  FirstRunMasterPrefsBrowserTestT() {}

 protected:
  void SetUp() override {
    SetMasterPreferencesForTest(Text);
    FirstRunMasterPrefsBrowserTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsBrowserTestT);
};

// Returns the true expected import state, derived from the original
// |expected_import_state|, for the current test machine's configuration. Some
// bot configurations do not have another profile (browser) to import from and
// thus the import must not be expected to have occurred.
int MaskExpectedImportState(int expected_import_state) {
  std::unique_ptr<ImporterList> importer_list(new ImporterList());
  base::RunLoop run_loop;
  importer_list->DetectSourceProfiles(
      g_browser_process->GetApplicationLocale(),
      false,  // include_interactive_profiles?
      run_loop.QuitClosure());
  run_loop.Run();
  int source_profile_count = importer_list->count();
#if defined(OS_WIN)
  // On Windows, the importer's DetectIEProfiles() will always add to the count.
  // Internet Explorer always exists and always has something to import.
  EXPECT_GT(source_profile_count, 0);
#endif
  if (source_profile_count == 0)
    return expected_import_state & ~AUTO_IMPORT_PROFILE_IMPORTED;
  return expected_import_state;
}

}  // namespace

const char kImportDefault[] =
    "{\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportDefault>
    FirstRunMasterPrefsImportDefault;
// No items are imported by default.
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportDefault, ImportDefault) {
  EXPECT_EQ(MaskExpectedImportState(AUTO_IMPORT_CALLED), auto_import_state());
}

const char kImportAll[] =
    "{\n"
    "  \"distribution\": {\n"
    "    \"import_bookmarks\": true,\n"
    "    \"import_history\": true,\n"
    "    \"import_home_page\": true,\n"
    "    \"import_search_engine\": true\n"
    "  }\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportAll>
    FirstRunMasterPrefsImportAll;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportAll, ImportAll) {
  EXPECT_EQ(MaskExpectedImportState(AUTO_IMPORT_CALLED |
                                    AUTO_IMPORT_PROFILE_IMPORTED),
            auto_import_state());
}

// The bookmarks file doesn't actually need to exist for this integration test
// to trigger the interaction being tested.
const char kImportBookmarksFile[] =
    "{\n"
    "  \"distribution\": {\n"
    "     \"import_bookmarks_from_file\": \"/foo/doesntexists.wtv\"\n"
    "  }\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportBookmarksFile>
    FirstRunMasterPrefsImportBookmarksFile;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportBookmarksFile,
                       ImportBookmarksFile) {
  EXPECT_EQ(MaskExpectedImportState(AUTO_IMPORT_CALLED |
                                    AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED),
            auto_import_state());
}

// Test an import with all import options disabled. This is a regression test
// for http://crbug.com/169984 where this would cause the import process to
// stay running, and the NTP to be loaded with no apps.
const char kImportNothing[] =
    "{\n"
    "  \"distribution\": {\n"
    "    \"import_bookmarks\": false,\n"
    "    \"import_history\": false,\n"
    "    \"import_home_page\": false,\n"
    "    \"import_search_engine\": false\n"
    "  }\n"
    "}\n";
typedef FirstRunMasterPrefsBrowserTestT<kImportNothing>
    FirstRunMasterPrefsImportNothing;
IN_PROC_BROWSER_TEST_F(FirstRunMasterPrefsImportNothing,
                       ImportNothingAndShowNewTabPage) {
  EXPECT_EQ(AUTO_IMPORT_CALLED, auto_import_state());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_TRUE(WaitForLoadStop(tab));
}

// Test first run with some tracked preferences.
const char kWithTrackedPrefs[] =
    "{\n"
    "  \"homepage\": \"example.com\",\n"
    "  \"homepage_is_newtabpage\": false\n"
    "}\n";
// A test fixture that will run in a first run scenario with master_preferences
// set to kWithTrackedPrefs. Parameterizable on the SettingsEnforcement
// experiment to be forced.
class FirstRunMasterPrefsWithTrackedPreferences
    : public FirstRunMasterPrefsBrowserTestT<kWithTrackedPrefs>,
      public testing::WithParamInterface<std::string> {
 public:
  FirstRunMasterPrefsWithTrackedPreferences() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunMasterPrefsBrowserTestT::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        std::string(chrome_prefs::internals::kSettingsEnforcementTrialName) +
            "/" + GetParam() + "/");
  }

  void SetUpInProcessBrowserTestFixture() override {
    FirstRunMasterPrefsBrowserTestT::SetUpInProcessBrowserTestFixture();

    // Bots are on a domain, turn off the domain check for settings hardening in
    // order to be able to test all SettingsEnforcement groups.
    chrome_prefs::DisableDomainCheckForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsWithTrackedPreferences);
};

IN_PROC_BROWSER_TEST_P(FirstRunMasterPrefsWithTrackedPreferences,
                       TrackedPreferencesSurviveFirstRun) {
  const PrefService* user_prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ("example.com", user_prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kHomePageIsNewTabPage));

  // The test for kHomePageIsNewTabPage above relies on the fact that true is
  // the default (hence false must be the user's pref); ensure this fact remains
  // true.
  const base::Value* default_homepage_is_ntp_value =
      user_prefs->GetDefaultPrefValue(prefs::kHomePageIsNewTabPage);
  bool default_homepage_is_ntp = false;
  EXPECT_TRUE(
      default_homepage_is_ntp_value->GetAsBoolean(&default_homepage_is_ntp));
  EXPECT_TRUE(default_homepage_is_ntp);
}

INSTANTIATE_TEST_SUITE_P(
    FirstRunMasterPrefsWithTrackedPreferencesInstance,
    FirstRunMasterPrefsWithTrackedPreferences,
    testing::Values(
        chrome_prefs::internals::kSettingsEnforcementGroupNoEnforcement,
        chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlways,
        chrome_prefs::internals::kSettingsEnforcementGroupEnforceAlwaysWithDSE,
        chrome_prefs::internals::
            kSettingsEnforcementGroupEnforceAlwaysWithExtensionsAndDSE));

#define COMPRESSED_SEED_TEST_VALUE                                             \
  "H4sIAAAAAAAA/+LSME4xsTAySjYzSDQ1S01KSk1KMUg1SjI1Tk4yMjI2NDMzTzEySjRPMxA6xs" \
  "glH+rrqBual5mWX5SbWVKpG1KUmZija2igG5BalJyaV2LB6MXDxZFelF9aEG9gKIDMM0LhGaPw" \
  "TFB4pig8MxSeOQrPAoVnKcDoxc3FnpKalliaUyLAGCSiwaDBqMGkwazBosGqwZbR8LppA38CIy" \
  "AAAP//KpzsDPMAAAA="
#define SEED_SIGNATURE_TEST_VALUE                                              \
  "MEUCIQCo4D8Ad0pMlFoLT4mMrv7/ZK7PqyEmJlW5jciua6mluAIgQQKcj352r/sjq8b98W+jRk" \
  "dwyDZn3ocgV01juWKx3u0="

constexpr char kCompressedSeedTestValue[] = COMPRESSED_SEED_TEST_VALUE;
constexpr char kSignatureValue[] = SEED_SIGNATURE_TEST_VALUE;

extern const char kWithVariationsPrefs[] =
    "{\n"
    "  \"variations_compressed_seed\": \"" COMPRESSED_SEED_TEST_VALUE
    "\",\n"
    "  \"variations_seed_signature\": \"" SEED_SIGNATURE_TEST_VALUE
    "\"\n"
    "}\n";

#undef COMPRESSED_SEED_TEST_VALUE
#undef SEED_SIGNATURE_TEST_VALUE

// Note: This test is parametrized on metrics consent state, since that affects
// field trial randomization.
class FirstRunMasterPrefsVariationsSeedTest
    : public FirstRunMasterPrefsBrowserTestT<kWithVariationsPrefs>,
      public testing::WithParamInterface<bool> {
 public:
  FirstRunMasterPrefsVariationsSeedTest() : metrics_consent_(GetParam()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        variations::switches::kDisableFieldTrialTestingConfig);
  }
  ~FirstRunMasterPrefsVariationsSeedTest() override = default;

  void SetUp() override {
    // Make metrics reporting work same as in Chrome branded builds, for test
    // consistency between Chromium and Chrome builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    // Based on GetParam(), either enable or disable metrics reporting.
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    FirstRunMasterPrefsBrowserTestT::SetUp();
  }

  // Writes the trial group to the temporary file.
  void WriteTrialGroupToTestFile(const std::string& trial_group) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int bytes_to_write = base::checked_cast<int>(trial_group.length());
    int bytes_written =
        base::WriteFile(GetTestFilePath(), trial_group.c_str(), bytes_to_write);
    EXPECT_EQ(bytes_to_write, bytes_written);
  }

  // Reads the trial group from the temporary file.
  std::string ReadTrialGroupFromTestFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    char data[256];
    int bytes_read = base::ReadFile(GetTestFilePath(), data, sizeof(data));
    EXPECT_NE(-1, bytes_read);
    return std::string(data, bytes_read);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  const bool metrics_consent_;

  // Returns a file path for persisting a field trial's state. The path is
  // under the user data dir, so should only persist between pairs of PRE_Foo
  // and Foo tests.
  base::FilePath GetTestFilePath() {
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    return user_data_dir.AppendASCII("FirstRunMasterPrefsVariationsSeedTest");
  }

  DISALLOW_COPY_AND_ASSIGN(FirstRunMasterPrefsVariationsSeedTest);
};

IN_PROC_BROWSER_TEST_P(FirstRunMasterPrefsVariationsSeedTest, Test) {
  // Tests variation migration from master_preferences to local_state.
  EXPECT_EQ(kCompressedSeedTestValue,
            g_browser_process->local_state()->GetString(
                variations::prefs::kVariationsCompressedSeed));
  EXPECT_EQ(kSignatureValue, g_browser_process->local_state()->GetString(
                                 variations::prefs::kVariationsSeedSignature));

  // Verify variations loaded in VariationsService by metrics.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       variations::StoreSeedResult::SUCCESS, 1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.LoadSeedSignature",
      variations::VerifySignatureResult::VALID_SIGNATURE, 1);
}

// The following tests are only enabled on Windows, since it is the only
// platform where master prefs is used to deliver first run variations. The
// tests do not pass on other platforms due to the provisional client id logic
// in metrics_state_manager.cc. See the comment there for details.

#if defined(OS_WIN)

// The trial and groups encoded in the above seed.
constexpr char kTrialName[] = "UMA-Uniformity-Trial-10-Percent";
const char* kTrialGroups[] = {"default",  "group_01", "group_02", "group_03",
                              "group_04", "group_05", "group_06", "group_07",
                              "group_08", "group_09"};

IN_PROC_BROWSER_TEST_P(FirstRunMasterPrefsVariationsSeedTest, PRE_SecondRun) {
  // Check that the trial from the seed exists and is in one of the expected
  // states. Persist the state so that we can verify its randomization persists
  // in FirstRunMasterPrefsVariationsSeedTest.SecondRun.
  const std::string group_name = base::FieldTrialList::FindFullName(kTrialName);
  ASSERT_TRUE(base::Contains(kTrialGroups, group_name)) << group_name;
  // Ensure trial is active (not disabled).
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
  WriteTrialGroupToTestFile(group_name);
}

IN_PROC_BROWSER_TEST_P(FirstRunMasterPrefsVariationsSeedTest, SecondRun) {
  // This test runs after PRE_SecondRun and verifies that the trial state on
  // the second run matches what was seen in the PRE_ test.
  const std::string group_name = base::FieldTrialList::FindFullName(kTrialName);
  ASSERT_TRUE(base::Contains(kTrialGroups, group_name)) << group_name;
  // Ensure trial is active (not disabled).
  ASSERT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
  // Read the trial group name that was saved by PRE_ForceTrials from the
  // corresponding test file.
  EXPECT_EQ(group_name, ReadTrialGroupFromTestFile());
}
#endif  // defined(OS_WIN)

INSTANTIATE_TEST_SUITE_P(FirstRunMasterPrefsVariationsSeedTests,
                         FirstRunMasterPrefsVariationsSeedTest,
                         testing::Bool());

#endif  // !defined(OS_CHROMEOS)

}  // namespace first_run
