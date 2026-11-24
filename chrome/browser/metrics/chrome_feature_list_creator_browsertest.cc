// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_feature_list_creator.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/variations_ids_provider.h"
#include "components/webui/flags/feature_entry.h"
#include "components/webui/flags/feature_entry_macros.h"
#include "components/webui/flags/flags_ui_switches.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS)
// For chromeos::switches::kFeatureFlags.
#include "third_party/cros_system_api/switches/chrome_switches.h"
#endif

namespace {

constexpr char kTestFlagName[] = "chrome-feature-list-creator-test-flag";
// The options of a feature entry with variations are Default, Enabled, one
// option per variation, then Disabled. Option 2 selects the first variation.
constexpr char kTestFlagVariationOption[] =
    "chrome-feature-list-creator-test-flag@2";
constexpr char kTestTrialName[] = "ChromeFeatureListCreatorTestTrial";
constexpr char kTestParamName[] = "test-param";
constexpr char kTestParamValue[] = "test-value";
constexpr variations::VariationID kTestVariationId = 3300200;
constexpr char kTestVariationIdString[] = "3300200";

BASE_FEATURE(kChromeFeatureListCreatorTestFeature,
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr flags_ui::FeatureEntry::FeatureParam kTestParams[] = {
    {kTestParamName, kTestParamValue}};
constexpr flags_ui::FeatureEntry::FeatureVariation kTestVariations[] = {
    {"with test param", kTestParams, kTestVariationIdString}};

std::vector<flags_ui::FeatureEntry> GetTestFeatureEntries() {
  return {{kTestFlagName, "name", "description",
           static_cast<unsigned short>(-1),
           FEATURE_WITH_PARAMS_VALUE_TYPE(kChromeFeatureListCreatorTestFeature,
                                          kTestVariations, kTestTrialName)}};
}

}  // namespace

class ChromeFeatureListCreatorTestBase : public InProcessBrowserTest {
 public:
  ChromeFeatureListCreatorTestBase()
      : scoped_feature_entries_(GetTestFeatureEntries()) {}

 protected:
  // Selects `kTestFlagVariationOption` in Local State for the next startup.
  void SelectTestFlagVariationInLocalState() {
    flags_ui::PrefServiceFlagsStorage flags_storage(
        g_browser_process->local_state());
    about_flags::SetFeatureEntryEnabled(&flags_storage,
                                        kTestFlagVariationOption,
                                        /*enable=*/true);
  }

 private:
  about_flags::testing::ScopedFeatureEntries scoped_feature_entries_;
};

// Starts the browser with the about:flags variation `kTestFlagVariationOption`
// selected. ChromeOS reads about:flags from the --feature-flags switch that
// session_manager sets. Other platforms read about:flags from Local State, so
// each test there has a PRE_ test that selects the variation.
class ChromeFeatureListCreatorBrowserTest
    : public ChromeFeatureListCreatorTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeFeatureListCreatorTestBase::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitchASCII(
        chromeos::switches::kFeatureFlags,
        std::string("[\"") + kTestFlagVariationOption + "\"]");
#endif
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorBrowserTest,
                       PRE_RegistersVariationParams) {
  SelectTestFlagVariationInLocalState();
}
#endif

// Verifies that startup applies the selected about:flags variation and creates
// its field trial. On ChromeOS, the field trial exists only if parameter
// registration reads the same flags as switch conversion.
IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorBrowserTest,
                       RegistersVariationParams) {
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(kChromeFeatureListCreatorTestFeature));
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestTrialName));
  EXPECT_EQ(kTestParamValue,
            base::GetFieldTrialParamValueByFeature(
                kChromeFeatureListCreatorTestFeature, kTestParamName));
  EXPECT_THAT(variations::VariationsIdsProvider::GetInstance()
                  ->GetVariationsVectorForWebPropertiesKeys(),
              testing::Contains(kTestVariationId));
}

#if BUILDFLAG(IS_CHROMEOS)
// Starts ChromeOS without --feature-flags so startup reads no command-line
// flags even when a variation is stored in Local State.
using ChromeFeatureListCreatorLocalStateOnlyBrowserTest =
    ChromeFeatureListCreatorTestBase;

IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorLocalStateOnlyBrowserTest,
                       PRE_IgnoresLocalStateVariation) {
  SelectTestFlagVariationInLocalState();
}

// Verifies that ChromeOS ignores an about:flags variation stored only in Local
// State when --feature-flags does not pass it.
IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorLocalStateOnlyBrowserTest,
                       IgnoresLocalStateVariation) {
  flags_ui::PrefServiceFlagsStorage flags_storage(
      g_browser_process->local_state());
  EXPECT_TRUE(flags_storage.GetFlags().contains(kTestFlagVariationOption));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(kChromeFeatureListCreatorTestFeature));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestTrialName));
  EXPECT_EQ("", base::GetFieldTrialParamValueByFeature(
                    kChromeFeatureListCreatorTestFeature, kTestParamName));
  EXPECT_THAT(variations::VariationsIdsProvider::GetInstance()
                  ->GetVariationsVectorForWebPropertiesKeys(),
              testing::Not(testing::Contains(kTestVariationId)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Starts the browser with --no-experiments and the about:flags variation
// `kTestFlagVariationOption` selected.
class ChromeFeatureListCreatorNoExperimentsBrowserTest
    : public ChromeFeatureListCreatorBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeFeatureListCreatorBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoExperiments);
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorNoExperimentsBrowserTest,
                       PRE_SkipsVariationParams) {
  SelectTestFlagVariationInLocalState();
}
#endif

// Verifies that --no-experiments skips both about:flags startup steps: switch
// conversion and parameter registration.
IN_PROC_BROWSER_TEST_F(ChromeFeatureListCreatorNoExperimentsBrowserTest,
                       SkipsVariationParams) {
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(kChromeFeatureListCreatorTestFeature));
  EXPECT_FALSE(base::FieldTrialList::TrialExists(kTestTrialName));
  EXPECT_EQ("", base::GetFieldTrialParamValueByFeature(
                    kChromeFeatureListCreatorTestFeature, kTestParamName));
}
