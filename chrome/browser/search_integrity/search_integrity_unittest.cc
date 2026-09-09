// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_integrity/search_integrity.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/search_integrity/search_integrity_allowlist.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_integrity {

class SearchIntegrityTest : public testing::Test {
 public:
  SearchIntegrityTest() = default;
  ~SearchIntegrityTest() override = default;

  void SetUp() override {
    test_util_ = std::make_unique<TemplateURLServiceTestUtil>();
    test_util_->VerifyLoad();
    search_integrity_ = std::make_unique<SearchIntegrity>(
        test_util_->model(), test_util_->profile());
  }

  void TearDown() override {
    SearchEngineAllowlist::GetInstance()->ResetForTesting();
    search_integrity_.reset();
    test_util_.reset();
  }

 protected:
  SearchIntegrityReport CheckSearchEnginesReport() {
    return search_integrity_->CheckSearchEnginesReport();
  }

  SiteSearchIntegrityReport CheckSiteSearchReport() {
    return search_integrity_->CheckSiteSearchReport();
  }

  DuplicateKeywordDetailedReport CheckDuplicateKeywordReport() {
    return search_integrity_->CheckDuplicateKeywordReport();
  }

  void TriggerAllowlistInitialized() {
    search_integrity_->OnAllowlistInitialized({});
  }

  TemplateURL* AddSearchEngineWithKeyword(const std::u16string& keyword,
                                          const std::u16string& short_name,
                                          const std::string& url,
                                          int prepopulate_id = 0,
                                          int starter_pack_id = 0) {
    TemplateURLData data;
    data.SetShortName(short_name);
    data.SetKeyword(keyword);
    data.SetURL(url);
    data.prepopulate_id = prepopulate_id;
    data.starter_pack_id = starter_pack_id;
    return test_util_->model()->Add(std::make_unique<TemplateURL>(data));
  }

  TemplateURL* AddExtensionSearchEngine(
      const std::u16string& keyword,
      const std::string& url,
      const std::string& extension_id,
      TemplateURL::Type type = TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) {
    TemplateURLData data;
    data.SetShortName(keyword);
    data.SetKeyword(keyword);
    data.SetURL(url);
    return test_util_->model()->Add(std::make_unique<TemplateURL>(
        data, type, extension_id, base::Time::Now(),
        /*wants_to_be_default_engine=*/false));
  }

  TemplateURL* AddSearchEngine(const std::u16string& short_name,
                               const std::string& url,
                               bool created_by_policy = false,
                               int prepopulate_id = 0,
                               int starter_pack_id = 0,
                               bool enforced_by_policy = false) {
    TemplateURLData data;
    data.SetShortName(short_name);
    data.SetKeyword(short_name);
    data.SetURL(url);
    if (created_by_policy) {
      data.policy_origin =
          TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    }
    data.enforced_by_policy = created_by_policy || enforced_by_policy;
    data.prepopulate_id = prepopulate_id;
    data.starter_pack_id = starter_pack_id;
    return test_util_->model()->Add(std::make_unique<TemplateURL>(data));
  }

  void ClearAllButDefault() {
    const TemplateURL* def_turl =
        test_util_->model()->GetDefaultSearchProvider();
    for (const auto& turl : test_util_->model()->GetTemplateURLs()) {
      if (turl != def_turl) {
        test_util_->model()->Remove(turl);
      }
    }
  }

  void SetDefaultSearchProvider(TemplateURL* template_url) {
    test_util_->model()->SetUserSelectedDefaultSearchProvider(template_url);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
  std::unique_ptr<SearchIntegrity> search_integrity_;
};

TEST_F(SearchIntegrityTest, CheckCustomSearchEngines_ExtractsReferralParam) {
  TemplateURL* turl =
      AddSearchEngine(u"Referral Engine", "http://custom.com?fr=test_ref");
  SetDefaultSearchProvider(turl);
  TriggerAllowlistInitialized();
  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.has_custom_option);
  EXPECT_EQ(report.referral_param_found, SearchReferralParam::kFr);
}

TEST_F(SearchIntegrityTest, CheckCustomSearchEngines_ExtractsNoReferralParam) {
  TemplateURL* turl =
      AddSearchEngine(u"No Referral Engine", "http://custom.com");
  SetDefaultSearchProvider(turl);
  TriggerAllowlistInitialized();
  ClearAllButDefault();
  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.has_custom_option);
  EXPECT_FALSE(report.referral_param_found.has_value());
}

TEST_F(SearchIntegrityTest, CheckDefaultSearchEngine_DefaultIsCustom) {
  TemplateURL* turl = AddSearchEngine(u"My Default", "http://default.com");
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
}

TEST_F(SearchIntegrityTest, CheckDefaultSearchEngine_DefaultIsPolicy) {
  TemplateURL* turl = AddSearchEngine(u"Policy Default", "http://policy.com",
                                      /*created_by_policy=*/true);
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_FALSE(report.is_default_custom);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_True) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"Example", "http://custom.example.com?fr=123");
  AddSearchEngine(u"Example", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_TRUE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_False_DifferentName) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"Example", "http://custom.example.com?fr=123");
  AddSearchEngine(u"Other", "http://policy.other.com",
                  /*created_by_policy=*/true, 0, 0);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_False_NotPolicy) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"Example", "http://custom.example.com?fr=123");
  AddSearchEngine(u"Example", "http://other.example.com",
                  /*created_by_policy=*/false);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_False_SameUrl) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"Example", "http://example.com");
  AddSearchEngine(u"Example", "http://example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_TokenMatch) {
  // "Google" and "Google Scholar" share the token "Google", so they should
  // match.
  TemplateURL* custom_engine =
      AddSearchEngine(u"Google Scholar", "http://custom.google.com");
  AddSearchEngine(u"Google", "http://policy.google.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_TRUE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, CheckMatchingPolicyEngine_TokenMismatch) {
  // "E" and "Example" do not share any tokens, so they should not match.
  TemplateURL* custom_engine =
      AddSearchEngine(u"E", "http://custom.example.com");
  AddSearchEngine(u"Example", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, IsNameMatch_StopWordsAreIgnored) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"My Search", "http://custom.example.com");
  AddSearchEngine(u"Your Search", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, IsNameMatch_ShortWordsAreIgnored) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"A B", "http://custom.example.com");
  AddSearchEngine(u"C A", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_FALSE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, IsNameMatch_ValidWordsMatch) {
  TemplateURL* custom_engine =
      AddSearchEngine(u"My Alpha Search", "http://custom.example.com");
  AddSearchEngine(u"Your Alpha Engine", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_TRUE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, IsNameMatch_PunctuationIsIgnored) {
  // "Yahoo!" and "Yahoo" should match because "!" is stripped.
  TemplateURL* custom_engine =
      AddSearchEngine(u"Goog", "http://custom.goog.com");
  AddSearchEngine(u"Goog!", "http://policy.goog.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.is_default_custom);
  EXPECT_TRUE(report.is_default_custom_with_matching_policy_engine);
}

TEST_F(SearchIntegrityTest, Histograms_LoggedCorrectly) {
  base::HistogramTester histogram_tester;

  TemplateURL* custom_engine =
      AddSearchEngine(u"Example", "http://custom.example.com?fr=123");
  AddSearchEngine(u"Example", "http://policy.example.com",
                  /*created_by_policy=*/true);
  SetDefaultSearchProvider(custom_engine);

  TriggerAllowlistInitialized();

  histogram_tester.ExpectUniqueSample("Search.Integrity.HasCustomSearchEngine",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.IsDefaultSearchEngineCustom", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.IsDefaultCustomWithMatchingPolicyEngine", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.Referral.ParameterFound", SearchReferralParam::kFr, 1);
}

TEST_F(SearchIntegrityTest, CheckDefaultSearchEngine_DefaultIsStarterPack) {
  TemplateURL* turl = AddSearchEngine(u"Starter Pack", "http://starter.com",
                                      /*created_by_policy=*/false,
                                      /*prepopulate_id=*/0,
                                      /*starter_pack_id=*/1);
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_FALSE(report.is_default_custom);
}

TEST_F(SearchIntegrityTest, CheckDefaultEnforcedWithoutPolicy_Unmanaged) {
  // Case: enforced_by_policy is true, but DefaultSearchProviderEnabled is not
  // managed.
  TemplateURL* turl = AddSearchEngine(u"Policy Default", "http://policy.com",
                                      /*created_by_policy=*/true, 0, 0,
                                      /*enforced_by_policy=*/true);
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_TRUE(report.is_default_enforced_without_policy);
}

TEST_F(SearchIntegrityTest, CheckDefaultEnforcedWithoutPolicy_Managed) {
  // Case: enforced_by_policy is true, and DefaultSearchProviderEnabled IS
  // managed.
  TemplateURL* turl = AddSearchEngine(u"Policy Default", "http://policy.com",
                                      /*created_by_policy=*/true, 0, 0,
                                      /*enforced_by_policy=*/true);
  SetDefaultSearchProvider(turl);

  test_util_->profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kDefaultSearchProviderEnabled, base::Value(true));

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_FALSE(report.is_default_enforced_without_policy);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywords_None) {
  AddSearchEngine(u"google", "http://google.com");
  AddSearchEngine(u"bing", "http://bing.com");

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_EQ(report.duplicate_keyword_status,
            SearchDuplicateKeyword::kNoDuplicates);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywords_DefaultOnly) {
  TemplateURL* google1 = AddSearchEngine(u"google", "http://google.com");
  AddSearchEngine(u"google", "http://google.ca");
  AddSearchEngine(u"bing", "http://bing.com");
  SetDefaultSearchProvider(google1);

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_EQ(report.duplicate_keyword_status,
            SearchDuplicateKeyword::kDefaultDuplicated);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywords_NonDefaultOnly) {
  TemplateURL* google = AddSearchEngine(u"google", "http://google.com");
  AddSearchEngine(u"bing", "http://bing.com");
  AddSearchEngine(u"bing", "http://bing.ca");
  SetDefaultSearchProvider(google);

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_EQ(report.duplicate_keyword_status,
            SearchDuplicateKeyword::kNonDefaultDuplicated);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywords_Both) {
  TemplateURL* google1 = AddSearchEngine(u"google", "http://google.com");
  AddSearchEngine(u"google", "http://google.ca");
  AddSearchEngine(u"bing", "http://bing.com");
  AddSearchEngine(u"bing", "http://bing.ca");
  SetDefaultSearchProvider(google1);

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_EQ(report.duplicate_keyword_status, SearchDuplicateKeyword::kBoth);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywords_CaseInsensitive) {
  TemplateURL* google1 = AddSearchEngine(u"Google", "http://google.com");
  AddSearchEngine(u"google", "http://google.ca");
  AddSearchEngine(u"Bing", "http://bing.com");
  AddSearchEngine(u"biNg", "http://bing.ca");
  SetDefaultSearchProvider(google1);

  SearchIntegrityReport report = CheckSearchEnginesReport();
  EXPECT_EQ(report.duplicate_keyword_status, SearchDuplicateKeyword::kBoth);
}

TEST_F(SearchIntegrityTest, CheckForSpoofing_NoAlertForNonUrlKeywords) {
  // These 3 should not trigger any spoofing metrics since the keywords are not
  // URLs.
  AddSearchEngine(u"goog", "https://bing.com/search?q={searchTerms}");
  AddSearchEngine(u"@gemini", "https://google.com/search?q={searchTerms}");
  AddSearchEngine(u"altavista", "https://bing.com/search?q={searchTerms}");

  SiteSearchIntegrityReport report = CheckSiteSearchReport();

  EXPECT_FALSE(report.has_cross_domain_search);
  EXPECT_FALSE(report.has_cross_tld_search);
}

TEST_F(SearchIntegrityTest, CheckForSpoofing_AlertForUrlKeywords) {
  // Shortcut "maps.google.com" pointing to Wikipedia should trigger alert.
  AddSearchEngine(u"maps.google.com",
                  "https://wikipedia.org/wiki/{searchTerms}");

  SiteSearchIntegrityReport report = CheckSiteSearchReport();

  EXPECT_TRUE(report.has_cross_domain_search);
  EXPECT_FALSE(report.has_cross_tld_search);
}

TEST_F(SearchIntegrityTest, CheckForSpoofing_AlertForCrossTld) {
  // Shortcut "google.com" pointing to google.ca should trigger CROSS_TLD alert.
  AddSearchEngine(u"google.com", "https://google.ca/search?q={searchTerms}");

  SiteSearchIntegrityReport report = CheckSiteSearchReport();

  EXPECT_TRUE(report.has_cross_tld_search);
}

TEST_F(SearchIntegrityTest, CheckForSpoofing_ExtensionUrlSearch) {
  // Shortcut "google.com" pointing to an extension URL should trigger
  // extension-url-search and NOT cross-domain alert.
  AddSearchEngine(u"google.com",
                  "chrome-extension://extensionid/search?q={searchTerms}");

  SiteSearchIntegrityReport report = CheckSiteSearchReport();

  EXPECT_TRUE(report.has_extension_url_search);
  EXPECT_FALSE(report.has_cross_domain_search);
}

TEST_F(SearchIntegrityTest, CheckForSpoofing_ObfuscatedUrl) {
  // Search URL with hex-encoded "www.google.com" should trigger obfuscated
  // alert. The domain itself should still be correctly identified as
  // google.com.
  AddSearchEngine(u"google.com",
                  "https://%77%77%77%2e%67%6f%6f%67%6c%65%2e%63%6f%6d/"
                  "search?q={searchTerms}");

  SiteSearchIntegrityReport report = CheckSiteSearchReport();

  EXPECT_TRUE(report.has_obfuscated_search_url);
  EXPECT_FALSE(report.has_cross_tld_search);
  EXPECT_FALSE(report.has_cross_domain_search);
}

TEST_F(SearchIntegrityTest, CheckCustomPopulatedDefault_False) {
  TemplateURL* turl = AddSearchEngine(u"Custom Engine", "http://custom.com",
                                      /*created_by_policy=*/false,
                                      /*prepopulate_id=*/0);
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_FALSE(report.custom_populated_default);
}

TEST_F(SearchIntegrityTest, CheckCustomPopulatedDefault_True) {
  TemplateURL* turl = AddSearchEngine(u"Custom with ID", "http://custom-id.com",
                                      /*created_by_policy=*/false,
                                      /*prepopulate_id=*/123);
  SetDefaultSearchProvider(turl);

  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.has_custom_option);
  EXPECT_TRUE(report.custom_populated_default);
}

TEST_F(SearchIntegrityTest, Histograms_CustomPopulatedDefault) {
  base::HistogramTester histogram_tester;

  TemplateURL* turl = AddSearchEngine(u"Custom with ID", "http://custom-id.com",
                                      /*created_by_policy=*/false,
                                      /*prepopulate_id=*/123);
  SetDefaultSearchProvider(turl);

  TriggerAllowlistInitialized();

  histogram_tester.ExpectUniqueSample("Search.Integrity.CustomPopulatedDefault",
                                      true, 1);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_SingleDuplicateCluster) {
  // One keyword duplicated across 2 entries; another keyword is unique.
  AddSearchEngineWithKeyword(u"foo", u"Foo 1",
                             "https://foo1.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"foo", u"Foo 2",
                             "https://foo2.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"bar", u"Bar", "https://bar.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_THAT(report.entries_per_duplicated_keyword, testing::ElementsAre(2));
  EXPECT_FALSE(report.has_trivial_duplicates);
  EXPECT_FALSE(report.has_extension_only_duplicate);
  EXPECT_FALSE(report.has_mixed_extension_duplicate);
  EXPECT_FALSE(report.has_starter_pack_duplicate);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_MultipleClustersDistribution) {
  // Multiple duplicate keywords with varying cluster sizes.
  // Cluster of 2:
  AddSearchEngineWithKeyword(u"two", u"Two A",
                             "https://two-a.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"two", u"Two B",
                             "https://two-b.com?q={searchTerms}");

  // Cluster of 4:
  AddSearchEngineWithKeyword(u"four", u"Four A",
                             "https://four-a.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"four", u"Four B",
                             "https://four-b.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"four", u"Four C",
                             "https://four-c.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"four", u"Four D",
                             "https://four-d.com?q={searchTerms}");

  // Cluster of 3:
  AddSearchEngineWithKeyword(u"three", u"Three A",
                             "https://three-a.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"three", u"Three B",
                             "https://three-b.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"three", u"Three C",
                             "https://three-c.com?q={searchTerms}");

  // Unique engine (should not be counted in duplicate metrics):
  AddSearchEngineWithKeyword(u"single", u"Single",
                             "https://single.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 3);
  EXPECT_THAT(report.entries_per_duplicated_keyword,
              testing::UnorderedElementsAre(2, 3, 4));
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_FullyTrivialDuplicates) {
  // Trivial duplicates have identical URLs across all entries in the cluster
  // (likely indicating sync issues).
  // Keyword "sync1" has 2 identical entries.
  AddSearchEngineWithKeyword(u"sync1", u"Sync 1A",
                             "https://example.com/search?q={searchTerms}");
  AddSearchEngineWithKeyword(u"sync1", u"Sync 1B",
                             "https://example.com/search?q={searchTerms}");

  // Keyword "sync2" has 3 identical entries.
  AddSearchEngineWithKeyword(u"sync2", u"Sync 2A",
                             "https://test.com/search?q={searchTerms}");
  AddSearchEngineWithKeyword(u"sync2", u"Sync 2B",
                             "https://test.com/search?q={searchTerms}");
  AddSearchEngineWithKeyword(u"sync2", u"Sync 2C",
                             "https://test.com/search?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 2);
  EXPECT_THAT(report.entries_per_duplicated_keyword,
              testing::UnorderedElementsAre(2, 3));
  EXPECT_TRUE(report.has_trivial_duplicates);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_PartiallyTrivialDuplicates) {
  // Keyword has some identical URLs and some distinct URLs.
  // It has trivial duplicates, but is NOT fully trivial.
  AddSearchEngineWithKeyword(u"mixed_url", u"Site A1",
                             "https://site.com/search?q={searchTerms}");
  AddSearchEngineWithKeyword(u"mixed_url", u"Site A2",
                             "https://site.com/search?q={searchTerms}");
  AddSearchEngineWithKeyword(u"mixed_url", u"Site B",
                             "https://different.com/search?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_THAT(report.entries_per_duplicated_keyword, testing::ElementsAre(3));
  EXPECT_TRUE(report.has_trivial_duplicates);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_ExtensionOnlyDuplicates) {
  // Keywords where ONLY extensions exist from multiple distinct extensions
  // (count >= 2).
  AddExtensionSearchEngine(
      u"ext_pair", "chrome-extension://ext1/search?q={searchTerms}", "ext1");
  AddExtensionSearchEngine(
      u"ext_pair", "chrome-extension://ext2/search?q={searchTerms}", "ext2");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_TRUE(report.has_extension_only_duplicate);
  EXPECT_FALSE(report.has_mixed_extension_duplicate);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_MixedExtensionDuplicates) {
  // Keywords where non-extension + extension(s) exist.
  AddSearchEngineWithKeyword(u"mixed_single", u"Site",
                             "https://site.com?q={searchTerms}");
  AddExtensionSearchEngine(u"mixed_single",
                           "chrome-extension://ext1/search?q={searchTerms}",
                           "ext1");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_TRUE(report.has_mixed_extension_duplicate);
  EXPECT_FALSE(report.has_extension_only_duplicate);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywordReport_StarterPackDuplicate) {
  // Starter pack / @-keyword collision (b/344666739).
  // @gemini is already present as a built-in starter pack engine.
  // Add a colliding search engine using the same keyword:
  AddSearchEngineWithKeyword(u"@gemini", u"Fake Gemini",
                             "https://imposter.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_TRUE(report.has_starter_pack_duplicate);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_NoStarterPackDuplicate_NormalDuplicates) {
  // Regular duplicate keywords without starter pack engines or @-keywords.
  AddSearchEngineWithKeyword(u"normal", u"Site 1",
                             "https://site1.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"normal", u"Site 2",
                             "https://site2.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_FALSE(report.has_starter_pack_duplicate);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywordReport_EmptyKeywordsIgnored) {
  // Search engines with whitespace-only keywords (which normalize to empty)
  // should NOT form a duplicate cluster.
  AddSearchEngineWithKeyword(u" ", u"Empty 1",
                             "https://empty1.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"  ", u"Empty 2",
                             "https://empty2.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"valid", u"Valid",
                             "https://valid.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 0);
  EXPECT_THAT(report.entries_per_duplicated_keyword, testing::IsEmpty());
  EXPECT_FALSE(report.has_trivial_duplicates);
}

TEST_F(SearchIntegrityTest,
       CheckDuplicateKeywordReport_CustomAtKeywordDuplicate) {
  // Two custom search engines sharing a keyword starting with '@' (without
  // starter_pack_id) should also trigger has_starter_pack_duplicate.
  AddSearchEngineWithKeyword(u"@custom", u"Custom At 1",
                             "https://custom1.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"@custom", u"Custom At 2",
                             "https://custom2.com?q={searchTerms}");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 1);
  EXPECT_TRUE(report.has_starter_pack_duplicate);
}

TEST_F(SearchIntegrityTest, CheckDuplicateKeywordReport_Comprehensive) {
  // Profile with all duplicate types simultaneously:
  // Trivial duplicate: identical URLs
  AddSearchEngineWithKeyword(u"trivial", u"Triv 1",
                             "https://trivial.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"trivial", u"Triv 2",
                             "https://trivial.com?q={searchTerms}");

  // Extension only duplicate
  AddExtensionSearchEngine(
      u"ext_kw", "chrome-extension://ext1/search?q={searchTerms}", "ext1");
  AddExtensionSearchEngine(
      u"ext_kw", "chrome-extension://ext2/search?q={searchTerms}", "ext2");

  // Mixed extension duplicate
  AddSearchEngineWithKeyword(u"mixed_kw", u"Mixed Site",
                             "https://mixed.com?q={searchTerms}");
  AddExtensionSearchEngine(
      u"mixed_kw", "chrome-extension://ext3/search?q={searchTerms}", "ext3");

  // Starter pack duplicate: @gemini is already present as a built-in starter
  // pack engine. Adding a colliding search engine creates a duplicate starter
  // pack cluster of size 2.
  AddSearchEngineWithKeyword(u"@gemini", u"Gemini Alt",
                             "https://gemini-alt.com?q={searchTerms}");

  // Unique non-duplicated engines (normal and extension)
  AddSearchEngineWithKeyword(u"unique_norm", u"Unique",
                             "https://unique.com?q={searchTerms}");
  AddExtensionSearchEngine(
      u"unique_ext", "chrome-extension://ext4/search?q={searchTerms}", "ext4");

  DuplicateKeywordDetailedReport report = CheckDuplicateKeywordReport();

  EXPECT_EQ(report.distinct_duplicated_keywords_count, 4);
  EXPECT_THAT(report.entries_per_duplicated_keyword,
              testing::UnorderedElementsAre(2, 2, 2, 2));
  EXPECT_TRUE(report.has_trivial_duplicates);
  EXPECT_TRUE(report.has_extension_only_duplicate);
  EXPECT_TRUE(report.has_mixed_extension_duplicate);
  EXPECT_TRUE(report.has_starter_pack_duplicate);
}

TEST_F(SearchIntegrityTest, Histograms_DuplicateKeyword) {
  base::HistogramTester histogram_tester;

  // Trivial duplicate: identical URLs
  AddSearchEngineWithKeyword(u"trivial", u"Triv 1",
                             "https://trivial.com?q={searchTerms}");
  AddSearchEngineWithKeyword(u"trivial", u"Triv 2",
                             "https://trivial.com?q={searchTerms}");

  // Extension only duplicate
  AddExtensionSearchEngine(
      u"ext_kw", "chrome-extension://ext1/search?q={searchTerms}", "ext1");
  AddExtensionSearchEngine(
      u"ext_kw", "chrome-extension://ext2/search?q={searchTerms}", "ext2");

  // Mixed extension duplicate
  AddSearchEngineWithKeyword(u"mixed_kw", u"Mixed Site",
                             "https://mixed.com?q={searchTerms}");
  AddExtensionSearchEngine(
      u"mixed_kw", "chrome-extension://ext3/search?q={searchTerms}", "ext3");

  // Starter pack duplicate: @gemini built-in engine collided with custom engine
  AddSearchEngineWithKeyword(u"@gemini", u"Gemini Alt",
                             "https://gemini-alt.com?q={searchTerms}");

  // Unique non-duplicated engine
  AddSearchEngineWithKeyword(u"unique", u"Unique",
                             "https://unique.com?q={searchTerms}");

  TriggerAllowlistInitialized();

  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.DuplicateKeyword.DuplicatedKeywordsCount", 4, 1);
  histogram_tester.ExpectBucketCount(
      "Search.Integrity.DuplicateKeyword.EntriesPerDuplicatedKeyword", 2, 4);
  histogram_tester.ExpectTotalCount(
      "Search.Integrity.DuplicateKeyword.EntriesPerDuplicatedKeyword", 4);

  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.DuplicateKeyword.HasExtensionOnlyDuplicate", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.DuplicateKeyword.HasMixedExtensionDuplicate", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.DuplicateKeyword.HasStarterPackDuplicate", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Search.Integrity.DuplicateKeyword.HasTrivialDuplicates", true, 1);
}

TEST(SearchEngineAllowlistTest, LoadHistoricalUrls) {
  SearchEngineAllowlist* allowlist = SearchEngineAllowlist::GetInstance();
  allowlist->ResetForTesting();

  constexpr char kTestJson[] = R"({
    "elements": {
      "google": {
        "search_url": "https://www.google.com/search?q={searchTerms}",
        "alternate_urls": [
          "https://www.google.com/#q={searchTerms}",
          "https://www.google.com/search#q={searchTerms}"
        ]
      },
      "fake_engine": {
        "search_url": "https://example.com/search?q=%s",
        "alternate_urls": [
          "http://example.com/search?q=%s"
        ]
      },
      "historical_fake_engine": {
        "alternate_urls": [
          "http://search.fakehistorical.com/q=%s"
        ]
      }
    }
  })";

  absl::flat_hash_set<std::string> urls =
      SearchEngineAllowlist::BuildAllowlist(kTestJson);
  allowlist->Initialize(std::move(urls));

  // Verify Google search URL and alternate URLs
  EXPECT_TRUE(allowlist->IsAllowed("https://www.google.com/search?q=%s"));
  EXPECT_TRUE(
      allowlist->IsAllowed("https://www.google.com/search?q={searchTerms}"));
  EXPECT_TRUE(allowlist->IsAllowed("https://www.google.com/#q=%s"));
  EXPECT_TRUE(allowlist->IsAllowed("https://www.google.com/search#q=%s"));

  // Verify fake engine
  EXPECT_TRUE(allowlist->IsAllowed("https://example.com/search?q=%s"));
  EXPECT_TRUE(allowlist->IsAllowed("http://example.com/search?q=%s"));

  // Verify historical fake engine alternate URL
  EXPECT_TRUE(allowlist->IsAllowed("http://search.fakehistorical.com/q=%s"));

  // Verify disallowed URLs
  EXPECT_FALSE(allowlist->IsAllowed("https://disallowed.com/search?q=%s"));
  EXPECT_FALSE(
      allowlist->IsAllowed("http://search.fakehistorical.com/other?q=%s"));
  EXPECT_FALSE(allowlist->IsAllowed("https://www.google.com/other?q=%s"));

  allowlist->ResetForTesting();
  EXPECT_FALSE(allowlist->IsAllowed("https://www.google.com/search?q=%s"));
}

TEST(SearchEngineAllowlistTest, EmptyAndMalformedJson) {
  SearchEngineAllowlist* allowlist = SearchEngineAllowlist::GetInstance();
  allowlist->ResetForTesting();

  auto empty_urls = SearchEngineAllowlist::BuildAllowlist("");
  EXPECT_TRUE(empty_urls.empty());

  auto malformed_urls =
      SearchEngineAllowlist::BuildAllowlist("{ malformed json }");
  EXPECT_TRUE(malformed_urls.empty());
}

}  // namespace search_integrity
