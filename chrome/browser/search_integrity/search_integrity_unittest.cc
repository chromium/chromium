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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
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
        test_util_->model(), test_util_->profile()->GetPath());
  }

  void TearDown() override {
    search_integrity_.reset();
    test_util_.reset();
  }

 protected:
  SearchIntegrityReport CheckSearchEnginesReport() {
    return search_integrity_->CheckSearchEnginesReport();
  }

  void TriggerAllowlistInitialized() {
    search_integrity_->OnAllowlistInitialized("");
  }

  TemplateURL* AddSearchEngine(const std::u16string& short_name,
                               const std::string& url,
                               bool created_by_policy = false,
                               int prepopulate_id = 0,
                               int starter_pack_id = 0) {
    TemplateURLData data;
    data.SetShortName(short_name);
    data.SetKeyword(short_name);
    data.SetURL(url);
    if (created_by_policy) {
      data.policy_origin =
          TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
    }
    data.prepopulate_id = prepopulate_id;
    data.starter_pack_id = starter_pack_id;
    return test_util_->model()->Add(std::make_unique<TemplateURL>(data));
  }

  void SetDefaultSearchProvider(TemplateURL* template_url) {
    test_util_->model()->SetUserSelectedDefaultSearchProvider(template_url);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
  std::unique_ptr<SearchIntegrity> search_integrity_;
};

TEST_F(SearchIntegrityTest, CheckCustomSearchEngines_ExtractsReferralId) {
  TemplateURL* turl =
      AddSearchEngine(u"Referral Engine", "http://custom.com?fr=test_ref");
  SetDefaultSearchProvider(turl);
  TriggerAllowlistInitialized();
  SearchIntegrityReport report = CheckSearchEnginesReport();

  EXPECT_TRUE(report.has_custom_option);
  EXPECT_EQ(report.referral_id, "test_ref");
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
}

}  // namespace search_integrity
