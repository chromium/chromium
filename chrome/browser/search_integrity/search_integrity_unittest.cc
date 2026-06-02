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
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
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

  void TriggerAllowlistInitialized() {
    search_integrity_->OnAllowlistInitialized("");
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
    data.enforced_by_policy = enforced_by_policy;
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

}  // namespace search_integrity
