// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

using ::TemplateURLPrepopulateData::bing;
using ::TemplateURLPrepopulateData::google;
using ::TemplateURLPrepopulateData::PrepopulatedEngine;
using ::testing::ElementsAreArray;
using TemplateURLVector = ::TemplateURL::TemplateURLVector;

// This test suite simulates that Android codesearch is being split from the
// other codesearch services, to get its own dedicated ID.

const char16_t android_keyword[] = u"cs.android.com";
const char16_t custom_keyword[] = u"cs";
const int generic_id = 824;
const int new_id = 913;

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
    {&chromium_codesearch, &android_codesearch_migrating,
     &android_codesearch_next},
};

// Describes various forms of search providers (`TemplateURL`,
// `TemplateURLData`, `PrepopulatedEngine`) for checking expectations.
struct SearchProviderSummary {
  int id;
  std::u16string keyword;

  // Optional fields are checked for object equality only if both summaries
  // specify it. So always make sure that the "actual" summary populates this
  // field.
  std::optional<bool> is_default;
  std::optional<std::string> search_url;
  std::optional<std::u16string> name;
  std::optional<bool> created_by_regulatory_program;

  bool operator==(const SearchProviderSummary& other) const {
    if (id != other.id || keyword != other.keyword) {
      return false;
    }

    if (is_default.has_value() && other.is_default.has_value() &&
        is_default != other.is_default) {
      return false;
    }

    if (search_url.has_value() && other.search_url.has_value() &&
        *search_url != *other.search_url) {
      return false;
    }

    if (name.has_value() && other.name.has_value() && *name != *other.name) {
      return false;
    }

    if (created_by_regulatory_program.has_value() &&
        other.created_by_regulatory_program.has_value() &&
        created_by_regulatory_program != other.created_by_regulatory_program) {
      return false;
    }

    return true;
  }
};

// For GTEST error messages.
std::ostream& operator<<(std::ostream& os,
                         const SearchProviderSummary& summary) {
  os << "{ id: " << summary.id << ", keyword: " << summary.keyword;
  if (summary.is_default.has_value()) {
    os << ", is_default: " << (*summary.is_default ? "true" : "false");
  }
  if (summary.search_url.has_value()) {
    os << ", search_url: "
       << (summary.search_url == TemplateURLPrepopulateData::google.search_url
               ?
               // Because that URL is super long and spams the error logs!
               "<google search url>"
               : *summary.search_url);
  }
  if (summary.name.has_value()) {
    os << ", name: " << *summary.name;
  }
  if (summary.created_by_regulatory_program.has_value()) {
    os << ", created_by_regulatory_program: "
       << (*summary.created_by_regulatory_program ? "true" : "false");
  }
  os << " }";
  return os;
}

// GTEST does not use `operator<<` on vectors. Override it more explicitly.
[[maybe_unused]] void PrintTo(
    const std::vector<SearchProviderSummary>& summary_list,
    std::ostream* os) {
  (*os) << "{ \n";
  for (const auto& summary : summary_list) {
    (*os) << "  " << summary << ",\n";
  }
  (*os) << "}";
}

class TemplateURLServiceChangedOnceObserver
    : public TemplateURLServiceObserver {
 public:
  TemplateURLServiceChangedOnceObserver(TemplateURLService* service,
                                        base::OnceClosure closure)
      : closure_(std::move(closure)) {
    CHECK(closure_);
    observation_.Observe(service);
  }

  void OnTemplateURLServiceChanged() override {
    std::move(closure_).Run();
    observation_.Reset();
  }

 private:
  base::OnceClosure closure_;
  base::ScopedObservation<TemplateURLService,
                          TemplateURLServiceChangedOnceObserver>
      observation_{this};
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
    : public PlatformBrowserTest,
      public base::test::WithFeatureOverride {
 protected:
  explicit PrepopulatedEnginesMigrationBrowserTestBase(
      std::vector<RegionalAndNonRegionalEngines> engine_set_load_order)
      : base::test::WithFeatureOverride(
            switches::kPrepopulatedEnginesMigration) {
    // GetTestPreCount() returns 0, 1, 2 respectively for Foo, PRE_Foo,
    // PRE_PRE_Foo. Here we flip the queried index so that we can list the
    // engine sets in the intuitive order (i.e. {first, second, third}) in the
    // constructor.
    size_t set_to_load_index =
        engine_set_load_order.size() - 1 - GetTestPreCount();
    CHECK_GE(set_to_load_index, 0u);
    CHECK_LT(set_to_load_index, engine_set_load_order.size());
    active_engines_config_ = engine_set_load_order[set_to_load_index];

    // Note: Since this gets recorded as a search provider list override,
    // rebuilding the keywords DB is always triggered, without having to change
    // the country or the data version.
    scoped_engines_override_ = std::make_unique<
        regional_capabilities::ScopedPrepopulatedEnginesOverride>(
        regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
            active_engines_config_.first, active_engines_config_.second));
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    WaitForTemplateURLServiceLoad();
  }

  TemplateURLService& template_url_service() {
    return CHECK_DEREF(TemplateURLServiceFactory::GetForProfile(GetProfile()));
  }

  const PrefService& profile_prefs() {
    return CHECK_DEREF(GetProfile()->GetPrefs());
  }

  void WaitForTemplateURLServiceLoad() {
    base::RunLoop run_loop;
    auto subscription =
        template_url_service().RegisterOnLoadedCallback(run_loop.QuitClosure());
    if (subscription) {
      run_loop.Run();
    }
    ASSERT_TRUE(template_url_service().loaded());
  }

#if BUILDFLAG(IS_ANDROID)
  void SetDeviceChoice(std::u16string keyword,
                       std::u16string name,
                       std::string search_url) {
    TemplateURLData data = template_url_service().CreatePlayAPITemplateURLData(
        keyword, name, search_url,
        /*suggest_url=*/"",
        /*favicon_url=*/"",
        /*new_tab_url=*/"",
        /*image_url=*/"",
        /*image_url_post_params=*/"",
        /*image_translate_url=*/"",
        /*image_translate_source_language_param_key=*/"",
        /*image_translate_target_language_param_key=*/"");

    base::RunLoop run_loop;
    TemplateURLServiceChangedOnceObserver changed_once_observer(
        &template_url_service(), run_loop.QuitClosure());

    ASSERT_TRUE(template_url_service().ResetPlayAPISearchEngine(data));

    run_loop.Run();
  }
#endif

  std::vector<SearchProviderSummary> GetServiceSearchProviders() {
    std::vector<SearchProviderSummary> actuals;
    TemplateURL::TemplateURLVector template_urls =
        template_url_service().GetTemplateURLs();
    const TemplateURL* dse = template_url_service().GetDefaultSearchProvider();

    for (const auto& turl : template_urls) {
      if (turl->starter_pack_id() !=
          template_url_starter_pack_data::StarterPackId::kNone) {
        // Ignore starter pack entries (tabs, bookmarks, etc), they just add
        // noise and are irrelevant to the current logic.
        continue;
      }

      actuals.push_back({
          .id = turl->prepopulate_id(),
          .keyword = turl->keyword(),
          .is_default = turl.get() == dse,
          .search_url = turl->url(),
          .name = turl->short_name(),
          .created_by_regulatory_program = turl->CreatedByRegulatoryProgram(),
      });
    }
    return actuals;
  }

  SearchProviderSummary GetSearchProviderFromPrefs(std::string_view pref_name) {
    std::unique_ptr<TemplateURLData> turl_data =
        TemplateURLDataFromDictionary(profile_prefs().GetDict(pref_name));
    return turl_data ? SearchProviderSummary{
        .id = turl_data->prepopulate_id,
        .keyword = turl_data->keyword(),
        .search_url = turl_data->url(),
        .created_by_regulatory_program = turl_data->CreatedByRegulatoryProgram(),
    } : SearchProviderSummary{};
  }

 protected:
  RegionalAndNonRegionalEngines active_engines_config_;

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
 protected:
  PrepopulatedEnginesMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {android_region_engines, android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesMigrationBrowserTest,
                         testing::Bool(),  // Feature states
                         &ParamToTestSuffix);

// -- Non DSE -----------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, PRE_NonDse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, android_keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, NonDse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}

// -- DSE --------------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, PRE_Dse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(android_keyword));

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_keyword, true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{generic_id, android_codesearch.keyword}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{generic_id, android_codesearch.keyword}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, Dse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // When migrating, the prefs are updated to refer to the new ID.
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{IsParamFeatureEnabled() ? new_id : generic_id,
                             android_codesearch.keyword}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{IsParamFeatureEnabled() ? new_id : generic_id,
                             android_codesearch.keyword}));
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest,
                       PRE_AndroidEeaDse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  SetDeviceChoice(android_keyword, android_codesearch.name,
                  android_codesearch.search_url);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {.id = generic_id,
       .keyword = android_keyword,
       .is_default = false,
       .created_by_regulatory_program = false},
      {.id = generic_id,
       .keyword = android_keyword,
       .is_default = true,
       .created_by_regulatory_program = true},
  };
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = android_codesearch.keyword,
                                   .created_by_regulatory_program = true}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{.id = generic_id,
                             .keyword = android_codesearch.keyword,
                             .created_by_regulatory_program = true}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, AndroidEeaDse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {.id = IsParamFeatureEnabled() ? new_id : generic_id,
       .keyword = android_keyword,
       .is_default = true,
       .created_by_regulatory_program = true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // .
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_codesearch.keyword,
                .created_by_regulatory_program = true}))
      << "When migrating, the prefs should be updated to refer to the new ID";
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{
          .id = IsParamFeatureEnabled() ? new_id : generic_id,
          .keyword = android_codesearch.keyword,
          .created_by_regulatory_program = true}));
}
#endif

// -- UserModified ------------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest,
                       PRE_UserModified) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  auto* migrating_t_url =
      template_url_service().GetTemplateURLForKeyword(android_keyword);
  ASSERT_EQ(generic_id, migrating_t_url->prepopulate_id());

  template_url_service().ResetTemplateURL(migrating_t_url, u"Searchy Search",
                                          custom_keyword,
                                          android_codesearch.search_url);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, custom_keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesMigrationBrowserTest, UserModified) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, custom_keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  auto* modified_url =
      template_url_service().GetTemplateURLForKeyword(custom_keyword);
  EXPECT_FALSE(modified_url->safe_for_autoreplace());
}

// -- CrossRegion -------------------------------------------------------------
// Simulates applying the update with the migration and changing region at the
// same time.
// PRE_2: uses android_region_engines
// PRE_1: uses chrome_region_engines, simulating a region switch
// PRE_0: uses android_migrated_region_engines, simulating a switch back to
// the original region + migration
class PrepopulatedEnginesCrossRegionMigrationBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 protected:
  PrepopulatedEnginesCrossRegionMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {android_region_engines, chrome_region_engines,
             android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                         testing::Bool(),  // Feature states
                         &ParamToTestSuffix);

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_PRE_NonDse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_NonDse) {
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       NonDse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, true},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, false}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
}

// -- CrossRegionDSE ----------------------------------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_PRE_Dse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(android_keyword));

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            expectations[2]);
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      expectations[2]);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_Dse) {
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, true}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // TODO(crbug.com/480071119): Prefs are still the ones associated with
  // `android_codesearch`.
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{generic_id, android_codesearch.keyword}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{generic_id, android_codesearch.keyword}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       Dse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{IsParamFeatureEnabled() ? new_id : generic_id,
                             android_codesearch.keyword}))
      << "When migrating, the prefs should be updated to refer to the new ID.";
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{IsParamFeatureEnabled() ? new_id : generic_id,
                             android_codesearch.keyword}));
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_PRE_AndroidEeaDse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  SetDeviceChoice(android_keyword, android_codesearch.name,
                  android_codesearch.search_url);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {.id = generic_id,
       .keyword = android_keyword,
       .is_default = false,
       .created_by_regulatory_program = false},
      {.id = generic_id,
       .keyword = android_keyword,
       .is_default = true,
       .created_by_regulatory_program = true},
  };
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = android_codesearch.keyword,
                                   .created_by_regulatory_program = true}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       PRE_AndroidEeaDse) {
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {.id = generic_id,
       .keyword = android_keyword,
       .is_default = true,
       .created_by_regulatory_program = true},
  };

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = android_codesearch.keyword,
                                   .created_by_regulatory_program = true}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionMigrationBrowserTest,
                       AndroidEeaDse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations{
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {.id = IsParamFeatureEnabled() ? new_id : generic_id,
       .keyword = android_keyword,
       .is_default = true,
       .created_by_regulatory_program = true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // When migrating, the prefs are updated to refer to the new ID.
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_codesearch.keyword,
                .created_by_regulatory_program = true}));
}
#endif

// -- CrossRegion No Migration ------------------------------------------------
// Starts from a definition of the engine that is not targeted from migration,
// so the DSE can be set and affect the prefs from this state:
// PRE_2: uses chrome_region_engines
// PRE_1: uses android_region_engines, simulating a region switch
// PRE_0: uses android_migrated_region_engines, triggering the migration

class PrepopulatedEnginesCrossRegionNoMigrationBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 protected:
  PrepopulatedEnginesCrossRegionNoMigrationBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase(
            {chrome_region_engines, android_region_engines,
             android_migrated_region_engines}) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         PrepopulatedEnginesCrossRegionNoMigrationBrowserTest,
                         testing::Bool(),  // Feature states
                         &ParamToTestSuffix);

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionNoMigrationBrowserTest,
                       PRE_PRE_Dse) {
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(
          chrome_codesearch.keyword));

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, true}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            expectations[2]);
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      expectations[2]);
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionNoMigrationBrowserTest,
                       PRE_Dse) {
  ASSERT_EQ(active_engines_config_, android_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_codesearch.keyword, true}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // TODO(crbug.com/480071119): Prefs are still the ones associated with
  // `chrome_codesearch`.
  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{generic_id, chrome_codesearch.keyword}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{generic_id, chrome_codesearch.keyword}));
}
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesCrossRegionNoMigrationBrowserTest,
                       Dse) {
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations;
  expectations.push_back({google.id, google.keyword, false});
  expectations.push_back({bing.id, bing.keyword, false});
  if (IsParamFeatureEnabled()) {
    // `chrome_codesearch` is identified as not matching with
    // `android_codesearch`, so `android_codesearch_next` is added as
    // independent entry.
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

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  // TODO(crbug.com/480071119): Prefs did not change since the PRE_PRE_
  // run, we migrate from them although the previous state was "non-migrating".
  // Whether the feature is enabled or not, we're not using the URL selected and
  // present in prefs.
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{.id = chrome_codesearch.id,
                             .keyword = chrome_codesearch.keyword,
                             .search_url = chrome_codesearch.search_url}));

  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{.id = chrome_codesearch.id,
                             .keyword = chrome_codesearch.keyword,
                             .search_url = chrome_codesearch.search_url}));
}

// -- Extended Cross-Region Migration -----------------------------------------

class PrepopulatedEnginesExtendedCrossRegionsBrowserTest
    : public PrepopulatedEnginesMigrationBrowserTestBase {
 protected:
  PrepopulatedEnginesExtendedCrossRegionsBrowserTest()
      : PrepopulatedEnginesMigrationBrowserTestBase({
            // See individual test bodies for more info about the state and
            // expectations. High level context here:

            // Change keyword, set old android CS as DSE, change name.
            android_region_engines,

            // Region change.
            chrome_region_engines,

            // Region change + upgrade to post-migration data set.
            android_migrated_region_engines,

            // Restart with same data & region, some issues may self-resolve.
            android_migrated_region_engines,

            // Region change.
            chrome_migrated_region_engines,

            // Region change.
            android_migrated_region_engines,
        }) {}
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
    testing::Bool(),  // Feature state. The "enabled" state is currently not
                      // following the ideal behaviour, added to document the
                      // progression of the various fixes in some sort of TDD
                      // way.
    &ParamToTestSuffix);

// -- Extended Cross-Region Migration : Dse -----------------------------------

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_PRE_PRE_Dse) {
  // Sets up the environment, setting `android_codesearch` as DSE.
  ASSERT_EQ(active_engines_config_, android_region_engines);

  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(android_keyword));

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, android_keyword, true, android_codesearch.search_url}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = android_keyword,
                                   .name = android_codesearch.name}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{.id = generic_id,
                             .keyword = android_keyword,
                             .name = android_codesearch.name}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_PRE_Dse) {
  // The region changed from the "android" one to the "chrome" one. The DSE
  // changes to be the "chrome" codesearch variant.
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, chrome_codesearch.keyword, true,
       chrome_codesearch.search_url, chrome_codesearch.name}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = android_keyword,
                                   .name = android_codesearch.name}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_Dse) {
  // The region changed from the "chrome" one to the "android" one, and it also
  // changes to the data set that includes the migration. The DSE changes to be
  // the "android" codesearch variant, and with the new ID when the feature is
  // enabled.
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true,
       android_codesearch.search_url, android_codesearch.name}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_keyword,
                .name = android_codesearch.name}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_Dse) {
  // Same regional state as before, no DSE change from before.
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true,
       android_codesearch.search_url, android_codesearch.name}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_keyword,
                .name = android_codesearch.name}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_Dse) {
  // The region changed from the post-migration "android" one to the "chrome"
  // one. When the feature is enabled, the "android" variant stays DSE, and
  // another entry for the regional "chrome" variant is added.
  ASSERT_EQ(active_engines_config_, chrome_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
  };

  if (IsParamFeatureEnabled()) {
    // DSE reconciled from prefs with `android_codesearch_next`
    expectations.push_back({new_id, android_keyword, true,
                            android_codesearch_next.search_url,
                            android_codesearch_next.name});
    // Added as it's a regional prepopulated entry that's otherwise missing.
    expectations.push_back({generic_id, chrome_codesearch.keyword, false,
                            chrome_codesearch.search_url,
                            chrome_codesearch.name});

  } else {
    expectations.push_back({generic_id, chrome_codesearch.keyword, true,
                            chrome_codesearch.search_url,
                            chrome_codesearch.name});
  }

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_keyword,
                .name = android_codesearch.name}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       Dse) {
  // The region changed from the post-migration "chrome" one to the "android"
  // one. Without the feature, the DSE is switched back to "android", while with
  // the feature the extra "chrome" entry is removed.
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, android_keyword, true,
       android_codesearch_next.search_url, android_codesearch_next.name}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = android_keyword,
                .name = android_codesearch.name}));
}

// -- Extended Cross-Region Migration : DSE with user modifications -----------

// Regression test for https://crbug.com/480071119.
IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_PRE_PRE_UserModifiedDse) {
  // Sets up the environment, customising `android_codesearch` and setting it as
  // DSE.
  ASSERT_EQ(active_engines_config_, android_region_engines);

  template_url_service().ResetTemplateURL(
      template_url_service().GetTemplateURLForKeyword(android_keyword),
      android_codesearch.name, custom_keyword, android_codesearch.search_url);

  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().GetTemplateURLForKeyword(custom_keyword));

  template_url_service().ResetTemplateURL(
      template_url_service().GetTemplateURLForKeyword(custom_keyword),
      u"Searchy Search", custom_keyword, android_codesearch.search_url);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {generic_id, custom_keyword, true, android_codesearch.search_url,
       u"Searchy Search"}};
  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = custom_keyword,
                                   .name = u"Searchy Search"}));
  EXPECT_EQ(
      GetSearchProviderFromPrefs(
          DefaultSearchManager::kMirroredDefaultSearchProviderDataPrefName),
      (SearchProviderSummary{.id = generic_id,
                             .keyword = custom_keyword,
                             .search_url = android_codesearch.search_url,
                             .name = u"Searchy Search"}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_PRE_UserModifiedDse) {
  ASSERT_EQ(active_engines_config_, chrome_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      // Since the keyword was changed, the entry is not `safe_for_autoreplace`
      // anymore. So the variant is switched to the "chrome" one, but keeping
      // the custom name.
      {generic_id, custom_keyword, true, chrome_codesearch.search_url,
       u"Searchy Search"}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{.id = generic_id,
                                   .keyword = custom_keyword,
                                   .search_url = android_codesearch.search_url,
                                   .name = u"Searchy Search"}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_PRE_UserModifiedDse) {
  // The region changed from the "chrome" one to the "android" one, and it also
  // changes to the data set that includes the migration. The DSE changes to be
  // the "android" codesearch variant.When the feature is
  // enabled it gets the new ID.
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, custom_keyword, true,
       android_codesearch.search_url, u"Searchy Search"}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = custom_keyword,
                .search_url = android_codesearch.search_url,
                .name = u"Searchy Search"}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_PRE_UserModifiedDse) {
  // Same regional state as before, the duplication when the feature is enabled
  // resolves itself.
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, custom_keyword, true,
       android_codesearch.search_url, u"Searchy Search"}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = custom_keyword,
                .name = u"Searchy Search"}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       PRE_UserModifiedDse) {
  // The region changed from the post-migration "android" one to the "chrome"
  // one. When the feature is enabled, the "android" variant stays DSE while
  // an entry associated with the regional "chrome" variant is added, instead of
  // replacing the DSE.
  ASSERT_EQ(active_engines_config_, chrome_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
  };

  if (IsParamFeatureEnabled()) {
    // DSE reconciled from prefs with `android_codesearch_next`
    expectations.push_back({new_id, custom_keyword, true,
                            android_codesearch_next.search_url,
                            u"Searchy Search"});

    // New engine from prepopulated entries
    expectations.push_back({generic_id, chrome_codesearch.keyword, false,
                            chrome_codesearch.search_url,
                            chrome_codesearch.name});
  } else {
    expectations.push_back({generic_id, custom_keyword, true,
                            chrome_codesearch.search_url, u"Searchy Search"});
  }

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = custom_keyword,
                .name = u"Searchy Search"}));
}

IN_PROC_BROWSER_TEST_P(PrepopulatedEnginesExtendedCrossRegionsBrowserTest,
                       UserModifiedDse) {
  // The region changed from the post-migration "chrome" one to the "android"
  // one, and the DSE is switched back to "android".
  ASSERT_EQ(active_engines_config_, android_migrated_region_engines);

  std::vector<SearchProviderSummary> expectations = {
      {google.id, google.keyword, false},
      {bing.id, bing.keyword, false},
      {IsParamFeatureEnabled() ? new_id : generic_id, custom_keyword, true,
       android_codesearch.search_url, u"Searchy Search"}};

  EXPECT_THAT(GetServiceSearchProviders(), ElementsAreArray(expectations));

  EXPECT_EQ(GetSearchProviderFromPrefs(
                DefaultSearchManager::kDefaultSearchProviderDataPrefName),
            (SearchProviderSummary{
                .id = IsParamFeatureEnabled() ? new_id : generic_id,
                .keyword = custom_keyword,
                .name = u"Searchy Search"}));
}

// -- Prepopulated Engines interactions with Search Provider Overrides --------

class PrepopulatedEnginesOverridesBrowserTest : public PlatformBrowserTest {
 protected:
  PrepopulatedEnginesOverridesBrowserTest() {
    if (GetTestPreCount() > 0) {
      // Feature is disabled in PRE_ step.
      feature_list_.InitAndDisableFeature(
          switches::kIgnoreSearchProviderOverrides);
    } else {
      // Feature is enabled in the main step.
      feature_list_.InitAndEnableFeature(
          switches::kIgnoreSearchProviderOverrides);
    }
  }

  TemplateURLService& template_url_service() {
    return CHECK_DEREF(TemplateURLServiceFactory::GetForProfile(GetProfile()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrepopulatedEnginesOverridesBrowserTest,
                       PRE_OverriddenDseIgnored) {
  {
    base::ListValue overrides;
    // Mandatory properties for search provider overrides
    // See `TemplateURLDataFromOverrideDictionary`
    base::DictValue entry;
    entry.Set("name", u"override");
    entry.Set("keyword", u"override");
    entry.Set("search_url", "https://override.com/s?q={searchTerms}");
    entry.Set("favicon_url", "http://override.com/favicon.ico");
    entry.Set("encoding", "UTF-8");
    entry.Set("id", 9999);
    overrides.Append(std::move(entry));

    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetList(prefs::kSearchProviderOverrides, std::move(overrides));
    prefs->SetInteger(prefs::kSearchProviderOverridesVersion, 1);
  }

  // The overridden engine should be available and set as DSE based on the prefs
  // observer.
  TemplateURL* override_turl =
      template_url_service().GetTemplateURLForKeyword(u"override");
  ASSERT_TRUE(override_turl);

  EXPECT_EQ(template_url_service().GetDefaultSearchProvider(), override_turl);
  EXPECT_EQ(template_url_service().GetDefaultSearchProvider()->prepopulate_id(),
            9999);
}

IN_PROC_BROWSER_TEST_F(PrepopulatedEnginesOverridesBrowserTest,
                       OverriddenDseIgnored) {
  // The overridden engine might still be in the keyword map because it's
  // persisted in the WebData database and we don't automatically purge it.
  // However, it should NOT be the default search provider anymore because it
  // should have failed reconciliation and fell back to a default.
  const TemplateURL* dse = template_url_service().GetDefaultSearchProvider();
  ASSERT_TRUE(dse);
  EXPECT_NE(dse->keyword(), u"foo")
      << "DSE should not be the overridden engine. Current DSE: "
      << dse->keyword() << " (ID: " << dse->prepopulate_id() << ")";
  EXPECT_NE(dse->prepopulate_id(), 9999);
}

}  // namespace
