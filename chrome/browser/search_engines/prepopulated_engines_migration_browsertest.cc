// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/test/browser_test.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

using ::TemplateURLPrepopulateData::bing;
using ::TemplateURLPrepopulateData::google;
using ::TemplateURLPrepopulateData::PrepopulatedEngine;
using TemplateURLVector = ::TemplateURL::TemplateURLVector;

// This test suite simulates that Android codesearch is being split from the
// other codesearch services, to get its own dedicated ID.

const char16_t android_keyword[] = u"cs.android.com";
const int generic_id = 2424;
const int new_id = 1313;

// Variants of a "codesearch" collection of services

const PrepopulatedEngine android_codesearch = {
    .name = u"Android Code Search",
    .keyword = android_keyword,
    .search_url = "https://cs.android.com/search?q={searchTerms}",
    .id = generic_id,
    .migrate_to_id = 0,
};

const PrepopulatedEngine chrome_codesearch = {
    .name = u"Chrome Code Search",
    .keyword = u"source.chrome.com",
    .search_url = "https://source.chrome.com/search?q={searchTerms}",
    .id = generic_id,
    .migrate_to_id = 0,
};

const PrepopulatedEngine chromium_codesearch = {
    .name = u"Chromium Code Search",
    .keyword = u"source.chromium.org",
    .search_url = "https://source.chromium.org/search?q={searchTerms}",
    .id = generic_id,
    .migrate_to_id = 0,
};

// Below are the new entries added for android codesearch: A replacement of the
// old one that specific that it migrates to a new ID, and the new version that
// uses the new ID.

const PrepopulatedEngine android_codesearch_migrating = {
    .name = android_codesearch.name,
    .keyword = android_codesearch.keyword,
    .search_url = android_codesearch.search_url,
    .id = android_codesearch.id,
    .migrate_to_id = new_id,
};

const PrepopulatedEngine android_codesearch_next = {
    .name = android_codesearch.name,
    .keyword = android_codesearch.keyword,
    .search_url = android_codesearch.search_url,
    .id = new_id,
    .migrate_to_id = 0,
};

using RegionalAndNonRegionalEngines =
    std::pair<std::vector<const PrepopulatedEngine*>,
              std::vector<const PrepopulatedEngine*>>;

// Sample set of prepopulated engines. This version is the basic,
// pre-migration one: android search in the current region, and chrome and
// android codesearch are known variants that are assumed to be used in
// different regions.
RegionalAndNonRegionalEngines android_region_engines = {
    {&google, &bing, &android_codesearch},
    {&chromium_codesearch, &chrome_codesearch},
};

// Sample set of prepopulated engines.
// Android codesearch is still in the current region, but we are indicating that
// we are migrating it to a new standalone entry with its own ID.
RegionalAndNonRegionalEngines android_migrated_region_engines = {
    {&google, &bing, &android_codesearch_migrating},
    {&chromium_codesearch, &android_codesearch_next, &chrome_codesearch},
};

// Sample set of prepopulated engines. This version is one where
// `android_codesearch` is replaced with a variant that is not intended to
// be subject to the migration, since although the ID is the same, the search
// URL is not matching the value from the migrated engine.
// This is the set with Chrome codesearch being the relevant regional variant.
RegionalAndNonRegionalEngines chrome_region_engines = {
    {&google, &bing, &chrome_codesearch},
    {&chromium_codesearch, &android_codesearch},
};

RegionalAndNonRegionalEngines chrome_migrated_region_engines = {
    {&google, &bing, &chrome_codesearch},
    {&android_codesearch, &android_codesearch_migrating,
     &android_codesearch_next},
};

struct SearchProviderExpectation {
  int id;
  std::u16string keyword;
  bool is_default;
  std::optional<std::string> search_url;
};

// Tests related to Prepopulated engine migration.
//
// Assuming we have regional lists like
//   region_1_engines = ([engine1, engine2, engine3], [engine2_alt])
//   region_2_engines = ([engine1, engine2_alt, engine3], [engine2])
//
// This test fixture helps using various PRE_ stages to represent successive
// runs where we might be using one set or the other, to represent changing
// regions or the application of a client update that changes the built-in set.
class PrepopulatedEnginesMigrationBrowserTestBase
    : public InProcessBrowserTest,
      public base::test::WithFeatureOverride {
 public:
  explicit PrepopulatedEnginesMigrationBrowserTestBase(
      std::vector<RegionalAndNonRegionalEngines> engine_set_load_order)
      : base::test::WithFeatureOverride(
            switches::kPrepopulatedEnginesMigration) {
    // GetTestPreCount() returns 0, 1, 2 respectively for Foo, PRE_Foo,
    // PRE_PRE_Foo. Here we flip the queried index so that we can list the
    // engine sets in the intuitive order (i.e. {first, second, third}) in the
    // constructor.
    int set_to_load_index =
        engine_set_load_order.size() - 1 - GetTestPreCount();
    CHECK_GE(set_to_load_index, 0);
    CHECK_LT(set_to_load_index, engine_set_load_order.size());
    auto engines_to_load = engine_set_load_order[set_to_load_index];

    // Note: Since this gets recorded as a search provider list override,
    // rebuilding the keywords DB is always triggered, without having to change
    // the country or the data version.
    scoped_engines_override_ = std::make_unique<
        regional_capabilities::ScopedPrepopulatedEnginesOverride>(
        regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
            engines_to_load.first, engines_to_load.second));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    WaitForTemplateURLServiceLoad();
  }

  TemplateURLService& template_url_service() {
    return CHECK_DEREF(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  PrefService& profile_prefs() {
    return CHECK_DEREF(browser()->profile()->GetPrefs());
  }

  TemplateURLVector GetTemplateURLsWithoutStarterPack() {
    auto urls = template_url_service().GetTemplateURLs();
    urls.erase(
        std::remove_if(
            urls.begin(), urls.end(),
            [](const auto& template_url_unique_ptr) {
              return template_url_unique_ptr->starter_pack_id() !=
                     template_url_starter_pack_data::StarterPackId::kNone;
            }),
        urls.end());
    return urls;
  }

  int CountEngines() { return GetTemplateURLsWithoutStarterPack().size(); }

  void WaitForTemplateURLServiceLoad() {
    base::RunLoop run_loop;
    auto subscription =
        template_url_service().RegisterOnLoadedCallback(run_loop.QuitClosure());
    if (subscription) {
      run_loop.Run();
    }
    ASSERT_TRUE(template_url_service().loaded());
  }

  void CheckSearchProviderExpectations(
      base::span<SearchProviderExpectation> expectations) {
    TemplateURLVector template_urls = GetTemplateURLsWithoutStarterPack();
    auto* dse = template_url_service().GetDefaultSearchProvider();

    ASSERT_EQ(expectations.size(), template_urls.size());
    for (int i = 0; i < expectations.size(); ++i) {
      const SearchProviderExpectation& expectation = expectations[i];
      const TemplateURL* actual = template_urls[i].get();

      EXPECT_EQ(expectation.id, actual->prepopulate_id());
      EXPECT_EQ(expectation.keyword, actual->keyword());
      EXPECT_EQ(expectation.is_default, actual == dse);

      if (expectation.search_url.has_value()) {
        EXPECT_EQ(*expectation.search_url, actual->url());
      }
    }
  }

  void CheckSearchProviderInPrefs(std::string_view pref_name,
                                  SearchProviderExpectation expectations) {
    const base::DictValue& url_dict = profile_prefs().GetDict(pref_name);
    auto turl_data = TemplateURLDataFromDictionary(url_dict);
    EXPECT_EQ(turl_data->keyword(), expectations.keyword);
    EXPECT_EQ(turl_data->prepopulate_id, expectations.id);
  }

 private:
  std::unique_ptr<regional_capabilities::ScopedPrepopulatedEnginesOverride>
      scoped_engines_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

std::string ParamToTestSuffix(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "FeatureEnabled" : "FeatureDisabled";
}

class PrepopulatedEnginesMigrationBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 public:
  PrepopulatedEnginesMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {android_region_engines, android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesMigrationBrowserTest,
                         testing::Bool(),
                         &ParamToTestSuffix);

// -- Non DSE -----------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, PRE_NonDse) {
  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, android_keyword, false}};
  CheckSearchProviderExpectations(expectations);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, NonDse) {
  std::vector<SearchProviderExpectation> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, false}};
  CheckSearchProviderExpectations(expectations);
}

// -- DSE --------------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, PRE_Dse) {
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(android_keyword));

  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_keyword, true}};
  CheckSearchProviderExpectations(expectations);

  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, Dse) {
  std::vector<SearchProviderExpectation> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true}};
  CheckSearchProviderExpectations(expectations);

  // Prefs are not affected by the feature state.
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
}

// -- UserModified ------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest,
                       PRE_UserModified) {
  auto* migrating_t_url =
      template_url_service().GetTemplateURLForKeyword(android_keyword);
  ASSERT_EQ(generic_id, migrating_t_url->prepopulate_id());

  template_url_service().ResetTemplateURL(migrating_t_url, u"Searchy Search",
                                          u"cs", android_codesearch.search_url);

  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, u"cs", false}};
  CheckSearchProviderExpectations(expectations);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, UserModified) {
  std::vector<SearchProviderExpectation> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, u"cs", false}};
  CheckSearchProviderExpectations(expectations);

  auto* modified_url = template_url_service().GetTemplateURLForKeyword(u"cs");
  EXPECT_FALSE(modified_url->safe_for_autoreplace());
}

// -- CrossRegion -------------------------------------------------------------
// Simulates applying the update with the migration and changing region at the
// same time.
// PRE_2: uses android_region_engines
// PRE_1: uses chrome_region_engines, simulating a region switch
// PRE_0: uses android_migrated_region_engines, simulating a switch back to
// the original region + migration
class PrepopulatedEnginesXRegionMigrationBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 public:
  PrepopulatedEnginesXRegionMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {android_region_engines, chrome_region_engines,
             android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesXRegionMigrationBrowserTest,
                         testing::Bool(),
                         &ParamToTestSuffix);

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest,
                       PRE_PRE_NonDse) {
  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, false}};
  CheckSearchProviderExpectations(expectations);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest,
                       PRE_NonDse) {
  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, false}};
  CheckSearchProviderExpectations(expectations);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest, NonDse) {
  std::vector<SearchProviderExpectation> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, false}};
  CheckSearchProviderExpectations(expectations);
}

// -- CrossRegionDSE ----------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest,
                       PRE_PRE_Dse) {
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(android_keyword));

  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, true}};
  CheckSearchProviderExpectations(expectations);
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      expectations[2]);
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      expectations[2]);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest,
                       PRE_Dse) {
  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, true}};

  CheckSearchProviderExpectations(expectations);

  // TODO(crbug.com/480071119): Prefs are still the ones associated with
  // `android_codesearch`.
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionMigrationBrowserTest, Dse) {
  std::vector<SearchProviderExpectation> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true}};
  CheckSearchProviderExpectations(expectations);

  // TODO(crbug.com/480071119): Prefs did not change since the PRE_PRE_
  // run, we migrate from them although the previous state was "non-migrating".
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {generic_id, android_codesearch.keyword});
}

// -- CrossRegion No Migration ------------------------------------------------
// Starts from a definition of the engine that is not targeted from migration,
// so the DSE can be set and affect the prefs from this state:
// PRE_2: uses chrome_region_engines
// PRE_1: uses android_region_engines, simulating a region switch
// PRE_0: uses android_migrated_region_engines, triggering the migration

class PrepopulatedEnginesXRegionNoMigrationBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 public:
  PrepopulatedEnginesXRegionNoMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {chrome_region_engines, android_region_engines,
             android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesXRegionNoMigrationBrowserTest,
                         testing::Bool(),
                         &ParamToTestSuffix);

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionNoMigrationBrowserTest,
                       PRE_PRE_Dse) {
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(
          chrome_codesearch.keyword));

  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, true}};
  CheckSearchProviderExpectations(expectations);
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      expectations[2]);
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      expectations[2]);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionNoMigrationBrowserTest,
                       PRE_Dse) {
  std::vector<SearchProviderExpectation> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, true}};

  CheckSearchProviderExpectations(expectations);

  // TODO(crbug.com/480071119): Prefs are still the ones associated with
  // `chrome_codesearch`.
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {generic_id, chrome_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {generic_id, chrome_codesearch.keyword});
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesXRegionNoMigrationBrowserTest, Dse) {
  std::vector<SearchProviderExpectation> expectations;
  expectations.push_back({google.id, google.keyword, false});
  expectations.push_back({bing.id, bing.keyword, false});
  if (IsParamFeatureEnabled()) {
    // `chrome_codesearch` is identified as not matching with
    // `android_codesearch`, so `android_codesearch` is added as independent
    // entry.
    expectations.push_back(
        {new_id, android_keyword, false, android_codesearch.search_url});
    // Based on the ID, `chromium_codesearch` was identified as the associated
    // engine during the reconciliation phase, as it's the first
    // entry in the full list matching `generic_id`.
    expectations.push_back({generic_id, chromium_codesearch.keyword, true,
                            chromium_codesearch.search_url});
  } else {
    // Based on the ID, the `android_codesearch_migrating` from the regional
    // engines is an appropriate replacement match for `generic_id`.
    expectations.push_back(
        {generic_id, android_keyword, true, android_codesearch.search_url});
  }

  CheckSearchProviderExpectations(expectations);

  // TODO(crbug.com/480071119): Prefs did not change since the PRE_PRE_
  // run, we migrate from them although the previous state was "non-migrating".
  // Whether the feature is enabled or not, we're not using the URL selected and
  // present in prefs.
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      {chrome_codesearch.id, chrome_codesearch.keyword});
  CheckSearchProviderInPrefs(
      DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName,
      {chrome_codesearch.id, chrome_codesearch.keyword});
}

}  // namespace
