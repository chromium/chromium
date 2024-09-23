// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/search_engines/template_url_service.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_host_to_urls_map.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;
using SiteSearchPolicyConflictType =
    TemplateURLService::SiteSearchPolicyConflictType;
using testing::NotNull;

namespace {

// A prepopulated ID to set for engines we want to show in the default list.
// This must simply be greater than 0.
static constexpr int kPrepopulatedId = 999999;

std::unique_ptr<TemplateURL> CreateKeywordWithDate(
    TemplateURLService* model,
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url,
    const std::string& suggest_url,
    const std::string& alternate_url,
    const std::string& favicon_url,
    bool safe_for_autoreplace,
    int prepopulate_id,
    const std::string& encodings = "UTF-8",
    Time date_created = Time(),
    Time last_modified = Time(),
    Time last_visited = Time(),
    TemplateURL::Type type = TemplateURL::NORMAL) {
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(short_name));
  data.SetKeyword(base::UTF8ToUTF16(keyword));
  data.SetURL(url);
  data.suggestions_url = suggest_url;
  if (!alternate_url.empty())
    data.alternate_urls.push_back(alternate_url);
  data.favicon_url = GURL(favicon_url);
  data.safe_for_autoreplace = safe_for_autoreplace;
  data.prepopulate_id = prepopulate_id;
  data.input_encodings = base::SplitString(
      encodings, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data.date_created = date_created;
  data.last_modified = last_modified;
  data.last_visited = last_visited;
  return std::make_unique<TemplateURL>(data, type);
}

TemplateURL* AddKeywordWithDate(TemplateURLService* model,
                                const std::string& short_name,
                                const std::string& keyword,
                                const std::string& url,
                                const std::string& suggest_url,
                                const std::string& alternate_url,
                                const std::string& favicon_url,
                                bool safe_for_autoreplace,
                                const std::string& encodings,
                                Time date_created,
                                Time last_modified,
                                Time last_visited) {
  TemplateURL* t_url = model->Add(CreateKeywordWithDate(
      model, short_name, keyword, url, suggest_url, alternate_url, favicon_url,
      safe_for_autoreplace, 0, encodings, date_created, last_modified,
      last_visited));
  EXPECT_TRUE(!t_url || (t_url->id() != 0));
  return t_url;
}

// Checks that the two TemplateURLs are similar. It does not check the id or
// any time-related fields. Neither pointer should be NULL.
void ExpectSimilar(const TemplateURL* expected, const TemplateURL* actual) {
  ASSERT_TRUE(expected != nullptr);
  ASSERT_TRUE(actual != nullptr);
  ExpectSimilar(&expected->data(), &actual->data());
}

std::unique_ptr<TemplateURLData> CreateTestSearchEngine() {
  auto result = std::make_unique<TemplateURLData>();
  result->SetShortName(u"test1");
  result->SetKeyword(u"test.com");
  result->SetURL("http://test.com/search?t={searchTerms}");
  result->favicon_url = GURL("http://test.com/icon.jpg");
  result->prepopulate_id = kPrepopulatedId;
  result->input_encodings = {"UTF-16", "UTF-32"};
  result->alternate_urls = {"http://test.com/search#t={searchTerms}"};
  return result;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Creates a `TemplateURLData` corresponding to a site search engine set by
// policy, with some fake data generated from `keyword` and the
// `featured_by_policy` field set according to the corresponding parameter.
std::unique_ptr<TemplateURLData> CreateTestSiteSearchEntry(
    const std::string& keyword,
    bool featured_by_policy) {
  auto data = std::make_unique<TemplateURLData>();
  data->SetShortName(base::UTF8ToUTF16(keyword + "name"));
  data->SetKeyword(base::UTF8ToUTF16(keyword));
  data->SetURL(std::string("https://") + keyword + ".com/q={searchTerms}");
  data->created_by_policy = TemplateURLData::CreatedByPolicy::kSiteSearch;
  data->enforced_by_policy = false;
  data->featured_by_policy = featured_by_policy;
  data->is_active = TemplateURLData::ActiveStatus::kTrue;
  data->favicon_url =
      GURL(std::string("https://") + keyword + ".com/favicon.ico");
  data->safe_for_autoreplace = false;
  data->date_created = base::Time();
  data->last_modified = base::Time();
  return data;
}

// Creates a `TemplateURLData` corresponding to a site search engine set by
// policy, with some fake data generated from `keyword` and
// `featured_by_policy` set as false.
std::unique_ptr<TemplateURLData> CreateTestSiteSearchEntry(
    const std::string& keyword) {
  return CreateTestSiteSearchEntry(keyword, /*featured_by_policy=*/false);
}

// Creates a `TemplateURLData` with some fake data generated from `keyword`
// and with the `safe_for_autoreplace` field set according to the
// corresponding parameter.
TemplateURLData CreateTestSearchEngineWithSafeForAutoreplace(
    const std::string& keyword,
    bool safe_for_autoreplace) {
  TemplateURLData data;
  data.SetKeyword(base::UTF8ToUTF16(keyword));
  data.SetURL(std::string("https://existing-") + keyword +
              ".com/q={searchTerms}");
  data.safe_for_autoreplace = safe_for_autoreplace;
  return data;
}

void VerifySiteSearchPolicyConflictHistograms(
    const base::HistogramTester& histogram_tester,
    const base::flat_map<SiteSearchPolicyConflictType, int>& expected_counts) {
  for (auto [type, count] : expected_counts) {
    histogram_tester.ExpectBucketCount(
        TemplateURLService::kSiteSearchPolicyConflictCountHistogramName, type,
        count);
  }
  histogram_tester.ExpectBucketCount(
      TemplateURLService::kSiteSearchPolicyHasConflictWithFeaturedHistogramName,
      expected_counts.at(SiteSearchPolicyConflictType::kWithFeatured) > 0, 1);
  histogram_tester.ExpectBucketCount(
      TemplateURLService::
          kSiteSearchPolicyHasConflictWithNonFeaturedHistogramName,
      expected_counts.at(SiteSearchPolicyConflictType::kWithNonFeatured) > 0,
      1);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

std::string ParamToTestSuffix(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "SearchEngineChoiceEnabled"
                    : "SearchEngineChoiceDisabled";
}

}  // namespace


// TemplateURLServiceTest -----------------------------------------------------

class TemplateURLServiceTestBase : public testing::Test {
 public:
  explicit TemplateURLServiceTestBase(bool is_search_engine_choice_enabled);

  TemplateURLServiceTestBase(const TemplateURLServiceTestBase&) = delete;
  TemplateURLServiceTestBase& operator=(const TemplateURLServiceTestBase&) =
      delete;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  TemplateURL* AddKeywordWithDate(const std::string& short_name,
                                  const std::string& keyword,
                                  const std::string& url,
                                  const std::string& suggest_url,
                                  const std::string& alternate_url,
                                  const std::string& favicon_url,
                                  bool safe_for_autoreplace,
                                  const std::string& encodings = "UTF-8",
                                  Time date_created = Time(),
                                  Time last_modified = Time(),
                                  Time last_visited = Time());

  // Add extension controlled search engine with |keyword| to model.
  TemplateURL* AddExtensionSearchEngine(const std::string& keyword,
                                        const std::string& extension_name,
                                        bool wants_to_be_default_engine,
                                        const Time& install_time = Time());

  // Verifies the two TemplateURLs are equal.
  void AssertEquals(const TemplateURL& expected, const TemplateURL& actual);
  void AssertEquals(const TemplateURL* expected, const TemplateURL* actual);

  // Verifies the two timestamps are equal, within the expected degree of
  // precision.
  void AssertTimesEqual(const Time& expected, const Time& actual);

  // Create an URL that appears to have been prepopulated, but won't be in the
  // current data.
  std::unique_ptr<TemplateURL> CreatePreloadedTemplateURL(
      bool safe_for_autoreplace,
      int prepopulate_id);

  // Set custom search engine as default fallback through overrides pref.
  void SetOverriddenEngines();

  // Helper methods to make calling TemplateURLServiceTestUtil methods less
  // visually noisy in the test code.
  void VerifyObserverCount(int expected_changed_count);
  void VerifyObserverFired();
  TemplateURLServiceTestUtil* test_util() { return test_util_.get(); }
  TemplateURLService* model() { return test_util_->model(); }
  const SearchTermsData& search_terms_data() {
    return model()->search_terms_data();
  }

 protected:
  bool IsSearchEngineChoiceEnabled() const {
    return is_search_engine_choice_enabled_;
  }

 private:
  const bool is_search_engine_choice_enabled_;

  content::BrowserTaskEnvironment
      task_environment_;  // To set up BrowserThreads.
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;
  base::test::ScopedFeatureList feature_list_;
};

class TemplateURLServiceTest : public TemplateURLServiceTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  TemplateURLServiceTest() : TemplateURLServiceTestBase(GetParam()) {}
};

class TemplateURLServiceWithoutFallbackTest : public TemplateURLServiceTest {
 public:
  TemplateURLServiceWithoutFallbackTest() : TemplateURLServiceTest() {}

  void SetUp() override {
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
    TemplateURLServiceTest::SetUp();
  }

  void TearDown() override {
    TemplateURLServiceTest::TearDown();
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
  }
};

#if BUILDFLAG(IS_ANDROID)
class TemplateURLServicePlayApiTest : public TemplateURLServiceTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  static std::string ParamToTestSuffix(
      const ::testing::TestParamInfo<bool>& info) {
    std::string suffix =
        info.param ? "SearchEngineChoiceEnabled" : "SearchEngineChoiceDisabled";

    return suffix;
  }

  TemplateURLServicePlayApiTest() : TemplateURLServiceTestBase(GetParam()) {
    EXPECT_EQ(
        IsSearchEngineChoiceEnabled(),
        base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};
#endif  // BUILDFLAG(IS_ANDROID)

TemplateURLServiceTestBase::TemplateURLServiceTestBase(
    bool is_search_engine_choice_enabled)
    : is_search_engine_choice_enabled_(is_search_engine_choice_enabled) {
  if (IsSearchEngineChoiceEnabled()) {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoiceTrigger);
  } else {
    feature_list_.InitAndDisableFeature(switches::kSearchEngineChoiceTrigger);
  }
}

void TemplateURLServiceTestBase::SetUp() {
  test_util_ = std::make_unique<TemplateURLServiceTestUtil>(
      TestingProfile::TestingFactories{TestingProfile::TestingFactory{
          HistoryServiceFactory::GetInstance(),
          HistoryServiceFactory::GetDefaultFactory()}});
}

void TemplateURLServiceTestBase::TearDown() {
  test_util_.reset();
}

TemplateURL* TemplateURLServiceTestBase::AddKeywordWithDate(
    const std::string& short_name,
    const std::string& keyword,
    const std::string& url,
    const std::string& suggest_url,
    const std::string& alternate_url,
    const std::string& favicon_url,
    bool safe_for_autoreplace,
    const std::string& encodings,
    Time date_created,
    Time last_modified,
    Time last_visited) {
  return ::AddKeywordWithDate(model(), short_name, keyword, url, suggest_url,
                              alternate_url, favicon_url, safe_for_autoreplace,
                              encodings, date_created, last_modified,
                              last_visited);
}

TemplateURL* TemplateURLServiceTestBase::AddExtensionSearchEngine(
    const std::string& keyword,
    const std::string& extension_name,
    bool wants_to_be_default_engine,
    const Time& install_time) {
  std::unique_ptr<TemplateURLData> turl_data =
      GenerateDummyTemplateURLData(keyword);
  turl_data->safe_for_autoreplace = false;

  auto ext_dse = std::make_unique<TemplateURL>(
      *turl_data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, extension_name,
      install_time, wants_to_be_default_engine);
  return test_util()->AddExtensionControlledTURL(std::move(ext_dse));
}

void TemplateURLServiceTestBase::AssertEquals(const TemplateURL& expected,
                                              const TemplateURL& actual) {
  ASSERT_EQ(expected.short_name(), actual.short_name());
  ASSERT_EQ(expected.keyword(), actual.keyword());
  ASSERT_EQ(expected.url(), actual.url());
  ASSERT_EQ(expected.suggestions_url(), actual.suggestions_url());
  ASSERT_EQ(expected.favicon_url(), actual.favicon_url());
  ASSERT_EQ(expected.alternate_urls(), actual.alternate_urls());
  ASSERT_EQ(expected.prepopulate_id(), actual.prepopulate_id());
  ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
  ASSERT_EQ(expected.input_encodings(), actual.input_encodings());
  ASSERT_EQ(expected.id(), actual.id());
  ASSERT_EQ(expected.date_created(), actual.date_created());
  AssertTimesEqual(expected.last_modified(), actual.last_modified());
  ASSERT_EQ(expected.last_visited(), actual.last_visited());
  ASSERT_EQ(expected.sync_guid(), actual.sync_guid());
}

void TemplateURLServiceTestBase::AssertEquals(const TemplateURL* expected,
                                              const TemplateURL* actual) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);
  if (expected == actual) {
    return;
  }

  AssertEquals(*expected, *actual);
}

void TemplateURLServiceTestBase::AssertTimesEqual(const Time& expected,
                                                  const Time& actual) {
  // Because times are stored with a granularity of one second, there is a loss
  // of precision when serializing and deserializing the timestamps. Hence, only
  // expect timestamps to be equal to within one second of one another.
  ASSERT_LT((expected - actual).magnitude(), base::Seconds(1));
}

std::unique_ptr<TemplateURL>
TemplateURLServiceTestBase::CreatePreloadedTemplateURL(
    bool safe_for_autoreplace,
    int prepopulate_id) {
  TemplateURLData data;
  data.SetShortName(u"unittest");
  data.SetKeyword(u"unittest");
  data.SetURL("http://www.unittest.com/{searchTerms}");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = safe_for_autoreplace;
  data.input_encodings.push_back("UTF-8");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  data.prepopulate_id = prepopulate_id;
  return std::make_unique<TemplateURL>(data);
}

void TemplateURLServiceTestBase::SetOverriddenEngines() {
  // Set custom search engine as default fallback through overrides.
  base::Value::Dict entry;
  entry.Set("name", "override_name");
  entry.Set("keyword", "override_keyword");
  entry.Set("search_url", "http://override.com/s?q={searchTerms}");
  entry.Set("favicon_url", "http://override.com/favicon.ico");
  entry.Set("encoding", "UTF-8");
  entry.Set("id", 1001);
  entry.Set("suggest_url", "http://override.com/suggest?q={searchTerms}");

  base::Value::List overrides_list;
  overrides_list.Append(std::move(entry));

  auto* prefs = test_util()->profile()->GetTestingPrefService();
  prefs->SetUserPref(prefs::kSearchProviderOverridesVersion, base::Value(1));
  prefs->SetUserPref(prefs::kSearchProviderOverrides,
                     base::Value(std::move(overrides_list)));
}

void TemplateURLServiceTestBase::VerifyObserverCount(
    int expected_changed_count) {
  EXPECT_EQ(expected_changed_count, test_util_->GetObserverCount());
  test_util_->ResetObserverCount();
}

void TemplateURLServiceTestBase::VerifyObserverFired() {
  EXPECT_LE(1, test_util_->GetObserverCount());
  test_util_->ResetObserverCount();
}

// Actual tests ---------------------------------------------------------------

TEST_P(TemplateURLServiceTest, Load) {
  test_util()->VerifyLoad();
}

TEST_P(TemplateURLServiceTest, AddUpdateRemove) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(u"google");
  data.SetKeyword(u"keyword");
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = true;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  data.sync_guid = "00000000-0000-0000-0000-000000000001";
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"keyword", GURL()));
  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(t_url->keyword()));
  // We need to make a second copy as the model takes ownership of |t_url| and
  // will delete it.  We have to do this after calling Add() since that gives
  // |t_url| its ID.
  std::unique_ptr<TemplateURL> cloned_url =
      std::make_unique<TemplateURL>(t_url->data());

  // Reload the model to verify it was actually saved to the database.
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  TemplateURL* loaded_url = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_TRUE(loaded_url != nullptr);
  AssertEquals(*cloned_url, *loaded_url);
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"keyword", GURL()));

  // We expect the last_modified time to be updated to the present time on an
  // explicit reset.
  Time now = Time::Now();
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(now);
  model()->set_clock(std::move(clock));

  // Mutate an element and verify it succeeded.
  model()->ResetTemplateURL(loaded_url, u"a", u"b", "c");
  ASSERT_EQ(u"a", loaded_url->short_name());
  ASSERT_EQ(u"b", loaded_url->keyword());
  ASSERT_EQ("c", loaded_url->url());
  ASSERT_FALSE(loaded_url->safe_for_autoreplace());
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"keyword", GURL()));
  ASSERT_FALSE(model()->CanAddAutogeneratedKeyword(u"b", GURL()));
  cloned_url = std::make_unique<TemplateURL>(loaded_url->data());
  base::RunLoop().RunUntilIdle();
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(u"b");
  ASSERT_TRUE(loaded_url != nullptr);
  AssertEquals(*cloned_url, *loaded_url);
  // We changed a TemplateURL in the service, so ensure that the time was
  // updated.
  AssertTimesEqual(now, loaded_url->last_modified());

  // Remove an element and verify it succeeded.
  model()->Remove(loaded_url);
  VerifyObserverCount(1);
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"b") == nullptr);
}

TEST_P(TemplateURLServiceTest, AddSameKeyword) {
  test_util()->VerifyLoad();

  AddKeywordWithDate("first", "keyword", "http://test1", std::string(),
                     std::string(), std::string(), true);
  VerifyObserverCount(1);

  // Test what happens when we try to add a TemplateURL with the same keyword as
  // one in the model.
  TemplateURLData data;
  data.SetShortName(u"second");
  data.SetKeyword(u"keyword");
  data.SetURL("http://test2");
  data.safe_for_autoreplace = false;
  data.last_modified = base::Time::FromTimeT(20);
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));

  // Because the old TemplateURL was replaceable and the new one wasn't, the new
  // one should have replaced the old.
  VerifyObserverCount(1);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(u"keyword"));
  EXPECT_EQ(u"second", t_url->short_name());
  EXPECT_EQ(u"keyword", t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a replaceable TemplateURL.  This should just delete the
  // passed-in URL.
  data.SetShortName(u"third");
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  EXPECT_EQ(nullptr, model()->Add(std::make_unique<TemplateURL>(data)));
  VerifyObserverCount(0);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(u"keyword"));
  EXPECT_EQ(u"second", t_url->short_name());
  EXPECT_EQ(u"keyword", t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a non-replaceable TemplateURL again.  This should allow both
  // TemplateURLs to exist under keyword, although the old one should still be
  // better, since it was more recently last_modified.
  data.SetShortName(u"fourth");
  data.SetURL("http://test4");
  // Make sure this one is not as recent as |t_url|.
  data.last_modified = base::Time();
  data.safe_for_autoreplace = false;
  TemplateURL* t_url2 = model()->Add(std::make_unique<TemplateURL>(data));
  VerifyObserverCount(1);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(u"keyword"));
  EXPECT_EQ(u"fourth", t_url2->short_name());
  EXPECT_EQ(u"keyword", t_url2->keyword());
  EXPECT_EQ(u"second", t_url->short_name());
  EXPECT_EQ(u"keyword", t_url->keyword());
}

TEST_P(TemplateURLServiceTest, AddOmniboxExtensionKeyword) {
  test_util()->VerifyLoad();

  AddKeywordWithDate("replaceable", "keyword1", "http://test1", std::string(),
                     std::string(), std::string(), true);
  AddKeywordWithDate("nonreplaceable", "keyword2", "http://test2",
                     std::string(), std::string(), std::string(), false);
  model()->RegisterOmniboxKeyword("test3", "extension", "keyword3",
                                  "http://test3",
                                  Time::FromSecondsSinceUnixEpoch(1));
  TemplateURL* original3 = model()->GetTemplateURLForKeyword(u"keyword3");
  ASSERT_TRUE(original3);

  // Extension keywords should override replaceable keywords.
  model()->RegisterOmniboxKeyword("id1", "test", "keyword1", "http://test4",
                                  Time());
  TemplateURL* extension1 = model()->FindTemplateURLForExtension(
      "id1", TemplateURL::OMNIBOX_API_EXTENSION);
  EXPECT_TRUE(extension1);
  EXPECT_EQ(extension1, model()->GetTemplateURLForKeyword(u"keyword1"));

  // They should also override non-replaceable keywords.
  model()->RegisterOmniboxKeyword("id2", "test", "keyword2", "http://test5",
                                  Time());
  TemplateURL* extension2 = model()->FindTemplateURLForExtension(
      "id2", TemplateURL::OMNIBOX_API_EXTENSION);
  ASSERT_TRUE(extension2);
  EXPECT_EQ(extension2, model()->GetTemplateURLForKeyword(u"keyword2"));

  // They should override extension keywords added earlier.
  model()->RegisterOmniboxKeyword("id3", "test", "keyword3", "http://test6",
                                  Time::FromSecondsSinceUnixEpoch(4));
  TemplateURL* extension3 = model()->FindTemplateURLForExtension(
      "id3", TemplateURL::OMNIBOX_API_EXTENSION);
  ASSERT_TRUE(extension3);
  EXPECT_EQ(extension3, model()->GetTemplateURLForKeyword(u"keyword3"));
}

TEST_P(TemplateURLServiceTest, AddSameKeywordWithOmniboxExtensionPresent) {
  test_util()->VerifyLoad();

  // Similar to the AddSameKeyword test, but with an extension keyword masking a
  // replaceable TemplateURL.  We should still do correct conflict resolution
  // between the non-template URLs.
  model()->RegisterOmniboxKeyword("test2", "extension", "keyword",
                                  "http://test2", Time());
  TemplateURL* extension = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_TRUE(extension);
  // Adding a keyword that matches the extension.
  AddKeywordWithDate("replaceable", "keyword", "http://test1", std::string(),
                     std::string(), std::string(), true);

  // Adding another replaceable keyword should remove the existing one, but
  // leave the extension as is.
  TemplateURLData data;
  data.SetShortName(u"name1");
  data.SetKeyword(u"keyword");
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));
  EXPECT_EQ(extension, model()->GetTemplateURLForKeyword(u"keyword"));
  EXPECT_EQ(t_url, model()->GetTemplateURLForHost("test3"));
  // Check that previous replaceable engine with keyword is removed.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test1"));

  // Adding a nonreplaceable keyword should remove the existing replaceable
  // keyword, yet extension must still be set as the associated URL for this
  // keyword.
  data.SetShortName(u"name2");
  data.SetURL("http://test4");
  data.safe_for_autoreplace = false;
  TemplateURL* nonreplaceable =
      model()->Add(std::make_unique<TemplateURL>(data));
  EXPECT_EQ(extension, model()->GetTemplateURLForKeyword(u"keyword"));
  EXPECT_EQ(nonreplaceable, model()->GetTemplateURLForHost("test4"));
  // Check that previous replaceable engine with keyword is removed.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test3"));
}

TEST_P(TemplateURLServiceTest, NotPersistOmniboxExtensionKeyword) {
  test_util()->VerifyLoad();

  // Register an omnibox keyword.
  model()->RegisterOmniboxKeyword("test", "extension", "keyword",
                                  "chrome-extension://test", Time());
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"keyword"));

  // Reload the data.
  test_util()->ResetModel(true);

  // Ensure the omnibox keyword is not persisted.
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"keyword"));
}

TEST_P(TemplateURLServiceTest, ClearBrowsingData_Keywords) {
  Time now = Time::Now();
  base::TimeDelta one_day = base::Days(1);
  Time month_ago = now - base::Days(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());

  // Create one with a 0 time.
  AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                     std::string(), "http://icon1", true, "UTF-8;UTF-16",
                     Time(), Time(), Time());
  // Create one for now and +/- 1 day.
  AddKeywordWithDate("name2", "key2", "http://foo2", "http://suggest2",
                     std::string(), "http://icon2", true, "UTF-8;UTF-16",
                     now - one_day, Time(), Time());
  AddKeywordWithDate("name3", "key3", "http://foo3", std::string(),
                     std::string(), std::string(), true, std::string(), now,
                     Time(), Time());
  AddKeywordWithDate("name4", "key4", "http://foo4", std::string(),
                     std::string(), std::string(), true, std::string(),
                     now + one_day, Time(), Time());
  // Add a non-replaceable engine, to verify we don't never remove those.
  AddKeywordWithDate("user_engine_name", "user_engine_key", "http://foo5",
                     "http://suggest5", std::string(), "http://icon5", false,
                     "UTF-8;UTF-16", now, Time(), Time());
  // Also add a replaceable engine that's marked as the Default Search Engine.
  // We also need to verify we never remove those. https://crbug.com/1166372
  TemplateURL* replaceable_dse = AddKeywordWithDate(
      "replaceable_dse_name", "replaceable_dse_key", "http://foo6",
      "http://suggest6", std::string(), "http://icon6", true, "UTF-8;UTF-16",
      month_ago, Time(), Time());
  ASSERT_THAT(replaceable_dse, NotNull());
  model()->SetUserSelectedDefaultSearchProvider(replaceable_dse);
  // Prepopulated and starter pack engines should also not be removed.
  std::unique_ptr<TemplateURLData> prepopulate_data =
      GenerateDummyTemplateURLData("prepopulated_key");
  prepopulate_data->prepopulate_id = 1;
  prepopulate_data->date_created = month_ago;
  model()->Add(std::make_unique<TemplateURL>(*prepopulate_data));
  std::unique_ptr<TemplateURLData> starter_pack_data =
      GenerateDummyTemplateURLData("starter_pack_key");
  starter_pack_data->starter_pack_id = 1;
  starter_pack_data->date_created = month_ago;
  model()->Add(std::make_unique<TemplateURL>(*starter_pack_data));

  // We just added a few items, validate them.
  EXPECT_EQ(8U, model()->GetTemplateURLs().size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  model()->RemoveAutoGeneratedBetween(now, base::Time());
  EXPECT_EQ(6U, model()->GetTemplateURLs().size());

  // Try removing from two months ago. This should only delete items that are
  // auto-generated.
  model()->RemoveAutoGeneratedBetween(now - base::Days(60), now);
  EXPECT_EQ(5U, model()->GetTemplateURLs().size());

  // Make sure the right values remain.
  EXPECT_EQ(u"key1", model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(0U,
            model()->GetTemplateURLs()[0]->date_created().ToInternalValue());

  EXPECT_EQ(u"user_engine_key", model()->GetTemplateURLs()[1]->keyword());
  EXPECT_FALSE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());
  EXPECT_EQ(now.ToInternalValue(),
            model()->GetTemplateURLs()[1]->date_created().ToInternalValue());

  EXPECT_EQ(u"replaceable_dse_key", model()->GetTemplateURLs()[2]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[2]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model()->GetTemplateURLs()[2]->date_created().ToInternalValue());

  EXPECT_EQ(u"prepopulated_key", model()->GetTemplateURLs()[3]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[3]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model()->GetTemplateURLs()[3]->date_created().ToInternalValue());

  EXPECT_EQ(u"starter_pack_key", model()->GetTemplateURLs()[4]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[4]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model()->GetTemplateURLs()[4]->date_created().ToInternalValue());

  // Try removing from Time=0 to Time=0. This should delete one more.
  model()->RemoveAutoGeneratedBetween(Time(), Time());
  EXPECT_EQ(4U, model()->GetTemplateURLs().size());
}

TEST_P(TemplateURLServiceTest, ClearBrowsingData_KeywordsForUrls) {
  Time now = Time::Now();
  base::TimeDelta one_day = base::Days(1);
  Time month_ago = now - base::Days(30);

  // Nothing has been added.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());

  // Create one for now and +/- 1 day.
  AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                     std::string(), "http://icon2", true, "UTF-8;UTF-16",
                     now - one_day, Time(), Time());
  AddKeywordWithDate("name2", "key2", "http://foo2", std::string(),
                     std::string(), std::string(), true, std::string(), now,
                     Time(), Time());
  AddKeywordWithDate("name3", "key3", "http://foo3", std::string(),
                     std::string(), std::string(), true, std::string(),
                     now + one_day, Time(), Time());

  // We just added a few items, validate them.
  EXPECT_EQ(3U, model()->GetTemplateURLs().size());

  // Try removing foo2. This should delete foo2, but leave foo1 and 3 untouched.
  GURL url2("http://foo2");
  model()->RemoveAutoGeneratedForUrlsBetween(
      base::BindRepeating(
          static_cast<bool (*)(const GURL&, const GURL&)>(operator==), url2),
      month_ago, now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(u"key1", model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(u"key3", model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());

  // Try removing foo1, but outside the range in which it was modified. It
  // should remain untouched.
  GURL url1("http://foo1");
  model()->RemoveAutoGeneratedForUrlsBetween(
      base::BindRepeating(
          static_cast<bool (*)(const GURL&, const GURL&)>(operator==), url1),
      now, now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(u"key1", model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(u"key3", model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());

  // Try removing foo3. This should delete foo3, but leave foo1 untouched.
  GURL url3("http://foo3");
  model()->RemoveAutoGeneratedForUrlsBetween(
      base::BindRepeating(
          static_cast<bool (*)(const GURL&, const GURL&)>(operator==), url3),
      month_ago, now + one_day + one_day);
  EXPECT_EQ(1U, model()->GetTemplateURLs().size());
  EXPECT_EQ(u"key1", model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
}

TEST_P(TemplateURLServiceTest, Reset) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURLData data;
  data.SetShortName(u"google");
  data.SetKeyword(u"keyword");
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));

  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();

  Time now = Time::Now();
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(now);
  model()->set_clock(std::move(clock));

  // Reset the short name, keyword, url and make sure it takes.
  const std::u16string new_short_name(u"a");
  const std::u16string new_keyword(u"b");
  const std::string new_url("c");
  model()->ResetTemplateURL(t_url, new_short_name, new_keyword, new_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(new_keyword, t_url->keyword());
  ASSERT_EQ(new_url, t_url->url());

  // Make sure the mappings in the model were updated.
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(new_keyword));
  ASSERT_EQ(nullptr, model()->GetTemplateURLForKeyword(u"keyword"));

  std::unique_ptr<TemplateURL> cloned_url(
      std::make_unique<TemplateURL>(t_url->data()));

  // Reload the model from the database and make sure the change took.
  test_util()->ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* read_url = model()->GetTemplateURLForKeyword(new_keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(*cloned_url, *read_url);
  AssertTimesEqual(now, read_url->last_modified());
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(TemplateURLServicePlayApiTest, CreateFromPlayAPI) {
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  const std::u16string short_name = u"google";
  const std::u16string keyword = u"keyword";
  const std::string search_url = "http://www.google.com/foo/bar";
  const std::string suggest_url = "http://www.google.com/suggest";
  const std::string favicon_url = "http://favicon.url";
  const std::string new_tab_url = "https://site.com/newtab";
  const std::string image_url = "https://site.com/img";
  const std::string image_url_post_params = "param";
  const std::string image_translate_url = "https://site.com/transl";
  const std::string image_translate_source_language_param_key = "s";
  const std::string image_translate_target_language_param_key = "t";
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(
      TemplateURLService::CreatePlayAPITemplateURLData(
          keyword, short_name, search_url, suggest_url, favicon_url,
          new_tab_url, image_url, image_url_post_params, image_translate_url,
          image_translate_source_language_param_key,
          image_translate_target_language_param_key)));
  ASSERT_TRUE(t_url);
  ASSERT_EQ(short_name, t_url->short_name());
  ASSERT_EQ(keyword, t_url->keyword());
  ASSERT_EQ(search_url, t_url->url());
  ASSERT_EQ(suggest_url, t_url->suggestions_url());
  ASSERT_EQ(GURL(favicon_url), t_url->favicon_url());
  ASSERT_EQ(new_tab_url, t_url->new_tab_url());
  ASSERT_EQ(image_url, t_url->image_url());
  ASSERT_EQ(image_url_post_params, t_url->image_url_post_params());
  ASSERT_EQ(image_translate_url, t_url->image_translate_url());
  ASSERT_EQ(image_translate_source_language_param_key,
            t_url->image_translate_source_language_param_key());
  ASSERT_EQ(image_translate_target_language_param_key,
            t_url->image_translate_target_language_param_key());

  ASSERT_TRUE(t_url->created_from_play_api());
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(keyword));

  auto cloned_url = std::make_unique<TemplateURL>(t_url->data());

  // Reload the model from the database and make sure the change took.
  test_util()->ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* read_url = model()->GetTemplateURLForKeyword(keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(*cloned_url, *read_url);
}

TEST_P(TemplateURLServicePlayApiTest, UpdateFromPlayAPI) {
  std::u16string keyword = u"keyword";

  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURLData data;
  data.SetShortName(u"google");
  data.SetKeyword(keyword);
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  // Play API only replaces safe_for_autoreplace engines.
  data.safe_for_autoreplace = true;
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));

  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();

  auto clock = std::make_unique<base::SimpleTestClock>();
  clock->SetNow(base::Time::FromTimeT(200));
  model()->set_clock(std::move(clock));

  // Reset the short name and url and make sure it takes.
  const std::u16string new_short_name = u"new_name";
  const std::string new_search_url = "new_url";
  const std::string new_suggest_url = "new_suggest_url";
  const std::string new_favicon_url = "new_favicon_url";
  const std::string new_other_data = "other_data";

  // The update creates a new Play API engine and deletes the old replaceable
  // one.
  t_url = model()->Add(std::make_unique<TemplateURL>(
      TemplateURLService::CreatePlayAPITemplateURLData(
          keyword, new_short_name, new_search_url, new_suggest_url,
          new_favicon_url, new_other_data, new_other_data, new_other_data,
          new_other_data, new_other_data, new_other_data)));
  ASSERT_TRUE(t_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(keyword, t_url->keyword());
  ASSERT_EQ(new_search_url, t_url->url());
  ASSERT_EQ(new_suggest_url, t_url->suggestions_url());
  ASSERT_EQ(GURL(new_favicon_url), t_url->favicon_url());
  ASSERT_EQ(new_other_data, t_url->new_tab_url());
  ASSERT_EQ(new_other_data, t_url->image_url());
  ASSERT_EQ(new_other_data, t_url->image_url_post_params());
  ASSERT_EQ(new_other_data, t_url->image_translate_url());
  ASSERT_EQ(new_other_data, t_url->image_translate_source_language_param_key());
  ASSERT_EQ(new_other_data, t_url->image_translate_target_language_param_key());
  ASSERT_TRUE(t_url->created_from_play_api());

  // Make sure the mappings in the model were updated.
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(keyword));

  auto cloned_url = std::make_unique<TemplateURL>(t_url->data());

  // Reload the model from the database and make sure the change took.
  test_util()->ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* read_url = model()->GetTemplateURLForKeyword(keyword);
  ASSERT_TRUE(read_url);
  AssertEquals(*cloned_url, *read_url);
}

INSTANTIATE_TEST_SUITE_P(,
                         TemplateURLServicePlayApiTest,
                         testing::Values(true, false),
                         &TemplateURLServicePlayApiTest::ParamToTestSuffix);

#endif  // BUILDFLAG(IS_ANDROID)

TEST_P(TemplateURLServiceTest, DefaultSearchProvider) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURL* t_url = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16");
  test_util()->ResetObserverCount();

  model()->SetUserSelectedDefaultSearchProvider(t_url);
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  ASSERT_TRUE(t_url->safe_for_autoreplace());
  ASSERT_TRUE(model()->ShowInDefaultList(t_url));

  // Setting the default search provider should have caused notification.
  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TemplateURL> cloned_url(
      std::make_unique<TemplateURL>(t_url->data()));

  // Make sure when we reload we get a default search provider.
  test_util()->ResetModel(true);
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(*cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_P(TemplateURLServiceTest, CantReplaceWithSameKeyword) {
  test_util()->ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"foo", GURL()));
  TemplateURL* t_url =
      AddKeywordWithDate("name1", "foo", "http://foo1", "http://sugg1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"foo", GURL("http://foo2")));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(
      model()->CanAddAutogeneratedKeyword(u"foo", GURL("http://foo2")));
}

TEST_P(TemplateURLServiceTest, CantReplaceWithSameHosts) {
  test_util()->ChangeModelToLoadState();
  ASSERT_TRUE(
      model()->CanAddAutogeneratedKeyword(u"foo", GURL("http://foo.com")));
  TemplateURL* t_url =
      AddKeywordWithDate("name1", "foo", "http://foo.com", "http://sugg1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(
      model()->CanAddAutogeneratedKeyword(u"bar", GURL("http://foo.com")));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(
      model()->CanAddAutogeneratedKeyword(u"bar", GURL("http://foo.com")));
}

TEST_P(TemplateURLServiceTest, HasDefaultSearchProvider) {
  // We should have a default search provider even if we haven't loaded.
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Now force the model to load and make sure we still have a default.
  test_util()->VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

TEST_P(TemplateURLServiceTest, DefaultSearchProviderLoadedFromPrefs) {
  test_util()->VerifyLoad();

  TemplateURLData data;
  data.SetShortName(u"a");
  data.safe_for_autoreplace = true;
  data.SetURL("http://url/{searchTerms}");
  data.suggestions_url = "http://url2";
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));
  const TemplateURLID id = t_url->id();

  model()->SetUserSelectedDefaultSearchProvider(t_url);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TemplateURL> cloned_url(
      std::make_unique<TemplateURL>(t_url->data()));

  // Reset the model and don't load it. The template url we set as the default
  // should be pulled from prefs now.
  test_util()->ResetModel(false);

  // NOTE: This doesn't use AssertEquals as only a subset of the TemplateURLs
  // value are persisted to prefs.
  const TemplateURL* default_turl = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_turl);
  EXPECT_EQ(u"a", default_turl->short_name());
  EXPECT_EQ("http://url/{searchTerms}", default_turl->url());
  EXPECT_EQ("http://url2", default_turl->suggestions_url());
  EXPECT_EQ(id, default_turl->id());

  // Now do a load and make sure the default search provider really takes.
  test_util()->VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(*cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_P(TemplateURLServiceTest,
       DefaultSearchProviderShouldBeProtectedFromKeywordConflictDuringLoad) {
  // Start with the model unloaded, with the DSE provided purely from prefs.
  ASSERT_FALSE(model()->loaded());
  const TemplateURL* initial_default_search_provider =
      model()->GetDefaultSearchProvider();
  ASSERT_THAT(initial_default_search_provider, NotNull());

  // Now simulate loading from the keyword table, where the DSE is added as a
  // a TemplateURL to the vector.
  TemplateURL* in_vector_dse_engine = model()->Add(
      std::make_unique<TemplateURL>(initial_default_search_provider->data()));
  ASSERT_THAT(in_vector_dse_engine, NotNull());
  ASSERT_EQ(in_vector_dse_engine,
            model()->GetTemplateURLForGUID(
                initial_default_search_provider->sync_guid()));

  // Then simulate loading a conflicting user engine with the same keyword.
  TemplateURL* user_engine = AddKeywordWithDate(
      "user_engine",
      base::UTF16ToUTF8(initial_default_search_provider->keyword()),
      "http://test2", std::string(), std::string(), std::string(), false,
      "UTF-8", base::Time::FromTimeT(20));
  EXPECT_THAT(user_engine, NotNull());

  // Now verify that the in-vector DSE entry was not removed due to the keyword
  // conflict. It should be protected by virtue of matching the initial DSE.
  EXPECT_EQ(in_vector_dse_engine,
            model()->GetTemplateURLForGUID(
                initial_default_search_provider->sync_guid()));
}

TEST_P(TemplateURLServiceTest, RepairPrepopulatedSearchEngines) {
  test_util()->VerifyLoad();

  // Edit Google search engine.
  TemplateURL* google = model()->GetTemplateURLForKeyword(u"google.com");
  ASSERT_TRUE(google);
  model()->ResetTemplateURL(google, u"trash", u"xxx",
                            "http://www.foo.com/s?q={searchTerms}");
  EXPECT_EQ(u"trash", google->short_name());
  EXPECT_EQ(u"xxx", google->keyword());

  // Add third-party default search engine.
  TemplateURL* user_dse = AddKeywordWithDate(
      "malware", "google.com", "http://www.goo.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Remove bing. Despite the extension added below, it will still be restored.
  TemplateURL* bing = model()->GetTemplateURLForKeyword(u"bing.com");
  ASSERT_TRUE(bing);
  model()->Remove(bing);
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"bing.com"));

  // Register an extension with bing keyword.
  model()->RegisterOmniboxKeyword("abcdefg", "extension_name", "bing.com",
                                  "http://abcdefg", Time());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"bing.com"));

  // Remove yahoo. It will be restored later, but for now verify we removed it.
  TemplateURL* yahoo = model()->GetTemplateURLForKeyword(u"yahoo.com");
  ASSERT_TRUE(yahoo);
  model()->Remove(yahoo);
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"yahoo.com"));

  // Now perform the actual repair that should restore Yahoo and Bing.
  model()->RepairPrepopulatedSearchEngines();

  // Google is default.
  ASSERT_EQ(google, model()->GetDefaultSearchProvider());
  // The keyword wasn't reverted.
  EXPECT_EQ(u"trash", google->short_name());
  EXPECT_EQ("www.google.com",
            google->GenerateSearchURL(model()->search_terms_data()).host());

  // Bing was repaired, verify that the NORMAL prepopulated engine is still back
  // even though the bing extension outranks the prepopulated engin.
  bing = nullptr;
  for (TemplateURL* turl : model()->GetTemplateURLs()) {
    if (turl->keyword() == u"bing.com" && turl->type() == TemplateURL::NORMAL &&
        turl->prepopulate_id() > 0) {
      bing = turl;
      break;
    }
  }
  EXPECT_THAT(bing, NotNull());

  // Yahoo was repaired and is now restored.
  yahoo = model()->GetTemplateURLForKeyword(u"yahoo.com");
  EXPECT_TRUE(yahoo);

  // User search engine is preserved.
  EXPECT_EQ(user_dse, model()->GetTemplateURLForHost("www.goo.com"));
  EXPECT_EQ(u"google.com", user_dse->keyword());
}

TEST_P(TemplateURLServiceTest, RepairSearchEnginesWithManagedDefault) {
  // Set a managed preference that establishes a default search provider.
  std::unique_ptr<TemplateURLData> managed = CreateTestSearchEngine();
  SetManagedDefaultSearchPreferences(*managed, true, test_util()->profile());
  test_util()->VerifyLoad();

  // Verify that the default manager we are getting is the managed one.
  auto expected_managed_default = std::make_unique<TemplateURL>(*managed);
  EXPECT_TRUE(model()->is_default_search_managed());
  const TemplateURL* actual_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default.get(), actual_managed_default);

  // The following call has no effect on the managed search engine.
  model()->RepairPrepopulatedSearchEngines();

  EXPECT_TRUE(model()->is_default_search_managed());
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default.get(), actual_managed_default);
}

// Checks that RepairPrepopulatedEngines correctly updates sync guid for default
// search. Repair is considered a user action and new DSE must be synced to
// other devices as well. Otherwise previous user selected engine will arrive on
// next sync attempt.
TEST_P(TemplateURLServiceTest, RepairPrepopulatedEnginesUpdatesSyncGuid) {
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  EXPECT_TRUE(GetDefaultSearchProviderGuidFromPrefs(*prefs).empty());

  const TemplateURL* initial_dse = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(initial_dse);

  // Add user provided default search engine.
  TemplateURL* user_dse = AddKeywordWithDate(
      "user_dse", "user_dse.com", "http://www.user_dse.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  // Check that user dse is different from initial.
  EXPECT_NE(initial_dse, user_dse);

  // Check that user DSE guid is stored in kSyncedDefaultSearchProviderGUID.
  EXPECT_EQ(user_dse->sync_guid(),
            GetDefaultSearchProviderGuidFromPrefs(*prefs));

  model()->RepairPrepopulatedSearchEngines();

  // Check that initial search engine is returned as default after repair.
  ASSERT_EQ(initial_dse, model()->GetDefaultSearchProvider());
  // Check that initial_dse guid is stored in kSyncedDefaultSearchProviderGUID.
  const std::string dse_guid = GetDefaultSearchProviderGuidFromPrefs(*prefs);
  EXPECT_EQ(initial_dse->sync_guid(), dse_guid);
  EXPECT_EQ(initial_dse->keyword(),
            model()->GetTemplateURLForGUID(dse_guid)->keyword());
}

// Checks that RepairPrepopulatedEngines correctly updates sync guid for default
// search when search engines are overridden using pref.
TEST_P(TemplateURLServiceTest,
       RepairPrepopulatedEnginesWithOverridesUpdatesSyncGuid) {
  SetOverriddenEngines();
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  EXPECT_TRUE(GetDefaultSearchProviderGuidFromPrefs(*prefs).empty());

  TemplateURL* overridden_engine =
      model()->GetTemplateURLForKeyword(u"override_keyword");
  ASSERT_TRUE(overridden_engine);

  EXPECT_EQ(overridden_engine, model()->GetDefaultSearchProvider());

  // Add user provided default search engine.
  TemplateURL* user_dse = AddKeywordWithDate(
      "user_dse", "user_dse.com", "http://www.user_dse.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Check that user DSE guid is stored in kSyncedDefaultSearchProviderGUID.
  EXPECT_EQ(user_dse->sync_guid(),
            GetDefaultSearchProviderGuidFromPrefs(*prefs));

  model()->RepairPrepopulatedSearchEngines();

  // Check that overridden engine is returned as default after repair.
  ASSERT_EQ(overridden_engine, model()->GetDefaultSearchProvider());
  // Check that overridden_engine guid is stored in
  // kSyncedDefaultSearchProviderGUID.
  const std::string dse_guid = GetDefaultSearchProviderGuidFromPrefs(*prefs);
  EXPECT_EQ(overridden_engine->sync_guid(), dse_guid);
  EXPECT_EQ(overridden_engine->keyword(),
            model()->GetTemplateURLForGUID(dse_guid)->keyword());
}

// Checks that RepairPrepopulatedEngines correctly updates sync guid for default
// search when search engines is overridden by extension.
TEST_P(TemplateURLServiceTest,
       RepairPrepopulatedEnginesWithExtensionUpdatesSyncGuid) {
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  EXPECT_TRUE(GetDefaultSearchProviderGuidFromPrefs(*prefs).empty());

  // Get initial DSE to check its guid later.
  const TemplateURL* initial_dse = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(initial_dse);

  // Add user provided default search engine.
  TemplateURL* user_dse = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("user_dse")));
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Check that user DSE guid is stored in kSyncedDefaultSearchProviderGUID.
  EXPECT_EQ(user_dse->sync_guid(),
            GetDefaultSearchProviderGuidFromPrefs(*prefs));

  // Add extension controlled default search engine.
  TemplateURL* extension_dse =
      AddExtensionSearchEngine("extension_dse", "extension_id", true);
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  // Check that user DSE guid is still stored in
  // kSyncedDefaultSearchProviderGUID.
  EXPECT_EQ(user_dse->sync_guid(),
            GetDefaultSearchProviderGuidFromPrefs(*prefs));

  model()->RepairPrepopulatedSearchEngines();
  // Check that extension engine is still default but sync guid is updated to
  // initial dse guid.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  EXPECT_EQ(initial_dse->sync_guid(),
            GetDefaultSearchProviderGuidFromPrefs(*prefs));
}

TEST_P(TemplateURLServiceTest, RepairStarterPackEngines) {
  test_util()->VerifyLoad();

  // Edit @bookmarks engine
  TemplateURL* bookmarks = model()->GetTemplateURLForKeyword(u"@bookmarks");
  ASSERT_TRUE(bookmarks);
  model()->ResetTemplateURL(bookmarks, u"trash", u"xxx",
                            "http://www.foo.com/s?q={searchTerms}");
  EXPECT_EQ(u"trash", bookmarks->short_name());
  EXPECT_EQ(u"xxx", bookmarks->keyword());

  // Remove @history. Despite the extension added below, it will still be
  // restored.
  TemplateURL* history = model()->GetTemplateURLForKeyword(u"@history");
  ASSERT_TRUE(history);
  model()->Remove(history);
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"@history"));

  // Register an extension with @history keyword.
  model()->RegisterOmniboxKeyword("abcdefg", "extension_name", "@history",
                                  "http://abcdefg", Time());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"@history"));

  // Now perform the actual repair that should restore @history.
  model()->RepairStarterPackEngines();

  // The keyword for bookmarks wasn't reverted.
  EXPECT_EQ(u"trash", bookmarks->short_name());
  EXPECT_EQ("chrome://bookmarks/?q={searchTerms}", bookmarks->url());

  // @history was repaired, verify that the NORMAL built-in engine is still back
  // even though the @history extension outranks the built-in engine.
  history = nullptr;
  for (TemplateURL* turl : model()->GetTemplateURLs()) {
    if (turl->keyword() == u"@history" && turl->type() == TemplateURL::NORMAL &&
        turl->starter_pack_id() > 0) {
      history = turl;
      break;
    }
  }
  EXPECT_THAT(history, NotNull());
}

TEST_P(TemplateURLServiceTest, SetDefaultSearchProviderPref) {
  std::string pref_value = "sync";
  if (IsSearchEngineChoiceEnabled()) {
    pref_value = "no_sync";
  }

  test_util()->VerifyLoad();
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, pref_value);

  // Test that the correct preference is set when
  // `SetDefaultSearchProviderGuidToPrefs` is called.
  if (IsSearchEngineChoiceEnabled()) {
    EXPECT_EQ(pref_value, prefs->GetString(prefs::kDefaultSearchProviderGUID));
    EXPECT_EQ(pref_value,
              prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
  } else {
    EXPECT_EQ(std::string(),
              prefs->GetString(prefs::kDefaultSearchProviderGUID));
    EXPECT_EQ(pref_value,
              prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
  }
}

TEST_P(TemplateURLServiceTest, GetDefaultSearchProviderPref) {
  test_util()->VerifyLoad();
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);

  EXPECT_EQ(std::string(), prefs->GetString(prefs::kDefaultSearchProviderGUID));
  EXPECT_EQ(std::string(),
            prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));

  const std::string sync_pref_value = "sync";
  const std::string no_sync_pref_value = "no_sync";

  prefs->SetString(prefs::kSyncedDefaultSearchProviderGUID, sync_pref_value);
  prefs->SetString(prefs::kDefaultSearchProviderGUID, no_sync_pref_value);

  // Test that `GetDefaultSearchProviderGuidFromPrefs` will return the value
  // of `kDefaultSearchProviderGUID` when the `kSearchEngineChoiceTrigger`
  // feature is enabled or `kSyncedDefaultSearchProviderGUID` otherwise.
  EXPECT_EQ(
      GetDefaultSearchProviderGuidFromPrefs(*prefs),
      IsSearchEngineChoiceEnabled() ? no_sync_pref_value : sync_pref_value);
}

TEST_P(TemplateURLServiceTest, UpdateKeywordSearchTermsForURL) {
  struct TestData {
    const std::string url;
    const std::u16string term;
  } data[] = {
      {"http://foo/", std::u16string()},
      {"http://foo/foo?q=xx", std::u16string()},
      {"http://x/bar?q=xx", std::u16string()},
      {"http://x/foo?y=xx", std::u16string()},
      {"http://x/foo?q=xx", u"xx"},
      {"http://x/foo?a=b&q=xx", u"xx"},
      {"http://x/foo?q=b&q=xx", std::u16string()},
      {"http://x/foo#query=xx", u"xx"},
      {"http://x/foo?q=b#query=xx", u"xx"},
      {"http://x/foo?q=b#q=xx", u"b"},
      {"http://x/foo?query=b#q=xx", std::u16string()},
  };

  test_util()->ChangeModelToLoadState();
  AddKeywordWithDate("name", "x", "http://x/foo?q={searchTerms}",
                     "http://sugg1", "http://x/foo#query={searchTerms}",
                     "http://icon1", false, "UTF-8;UTF-16");

  for (size_t i = 0; i < std::size(data); ++i) {
    TemplateURLService::URLVisitedDetails details = {
      GURL(data[i].url), false
    };
    model()->UpdateKeywordSearchTermsForURL(details);
    EXPECT_EQ(data[i].term, test_util()->GetAndClearSearchTerm());
  }
}

TEST_P(TemplateURLServiceTest, DontUpdateKeywordSearchForNonReplaceable) {
  struct TestData {
    const std::string url;
  } data[] = {
    { "http://foo/" },
    { "http://x/bar?q=xx" },
    { "http://x/foo?y=xx" },
  };

  test_util()->ChangeModelToLoadState();
  AddKeywordWithDate("name", "x", "http://x/foo", "http://sugg1", std::string(),
                     "http://icon1", false, "UTF-8;UTF-16");

  for (size_t i = 0; i < std::size(data); ++i) {
    TemplateURLService::URLVisitedDetails details = {
      GURL(data[i].url), false
    };
    model()->UpdateKeywordSearchTermsForURL(details);
    ASSERT_EQ(std::u16string(), test_util()->GetAndClearSearchTerm());
  }
}

// Historically, {google:baseURL} keywords would change to different
// country-specific Google URLs dynamically. That logic was removed, but test
// that country-specific Google URLs can still be added manually.
TEST_P(TemplateURLServiceWithoutFallbackTest, ManualCountrySpecificGoogleURL) {
  // NOTE: Do not load the prepopulate data, which also has a {google:baseURL}
  // keyword in it and would confuse this test.
  test_util()->ChangeModelToLoadState();

  const TemplateURL* t_url = AddKeywordWithDate(
      "name", "google.com", "{google:baseURL}?q={searchTerms}", "http://sugg1",
      std::string(), "http://icon1", false, "UTF-8;UTF-16");
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("www.google.com"));
  EXPECT_EQ("www.google.com", t_url->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(u"google.com", t_url->keyword());

  // Now add a manual entry for a country-specific Google URL.
  TemplateURL* manual = AddKeywordWithDate(
      "manual", "google.de", "http://www.google.de/search?q={searchTerms}",
      std::string(), std::string(), std::string(), false);

  // Verify that the entries do not conflict.
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(u"google.com"));
  EXPECT_EQ("www.google.com", t_url->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(u"google.com", t_url->keyword());
  ASSERT_EQ(manual, model()->GetTemplateURLForKeyword(u"google.de"));
  EXPECT_EQ("www.google.de", manual->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(u"google.de", manual->keyword());
}

INSTANTIATE_TEST_SUITE_P(,
                         TemplateURLServiceWithoutFallbackTest,
                         ::testing::Bool(),
                         &ParamToTestSuffix);

// Make sure TemplateURLService generates a KEYWORD_GENERATED visit for
// KEYWORD visits.
TEST_P(TemplateURLServiceTest, GenerateVisitOnKeyword) {
  test_util()->ResetModel(true);

  // Create a keyword.
  TemplateURL* t_url = AddKeywordWithDate(
      "keyword", "keyword", "http://foo.com/foo?query={searchTerms}",
      "http://sugg1", std::string(), "http://icon1", true, "UTF-8;UTF-16",
      Time::Now(), Time::Now(), Time());

  // Add a visit that matches the url of the keyword.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      test_util()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  history->AddPage(
      GURL(t_url->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(u"blah"), search_terms_data())),
      Time::Now(), 0, 0, GURL(), history::RedirectList(),
      ui::PAGE_TRANSITION_KEYWORD, history::SOURCE_BROWSED, false);

  // Wait for history to finish processing the request.
  test_util()->profile()->BlockUntilHistoryProcessesPendingRequests();

  // Query history for the generated url.
  base::CancelableTaskTracker tracker;
  history::QueryURLResult query_url_result;
  history->QueryURL(
      GURL("http://keyword"), true,
      base::BindLambdaForTesting([&](history::QueryURLResult result) {
        query_url_result = std::move(result);
      }),
      &tracker);

  // Wait for the request to be processed.
  test_util()->profile()->BlockUntilHistoryProcessesPendingRequests();

  // And make sure the url and visit were added.
  EXPECT_TRUE(query_url_result.success);
  EXPECT_NE(0, query_url_result.row.id());
  ASSERT_EQ(1U, query_url_result.visits.size());
  EXPECT_TRUE(
      ui::PageTransitionCoreTypeIs(query_url_result.visits[0].transition,
                                   ui::PAGE_TRANSITION_KEYWORD_GENERATED));
}

// Make sure that the load routine deletes prepopulated engines that no longer
// exist in the prepopulate data.
TEST_P(TemplateURLServiceTest, LoadDeletesUnusedProvider) {
  // Create a preloaded template url. Add it to a loaded model and wait for the
  // saves to finish.
  test_util()->ChangeModelToLoadState();
  model()->Add(CreatePreloadedTemplateURL(true, kPrepopulatedId));
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"unittest") != nullptr);
  base::RunLoop().RunUntilIdle();

  // Ensure that merging clears this engine.
  test_util()->ResetModel(true);
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"unittest") == nullptr);

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that the database was updated as a result of the
  // merge.
  test_util()->ResetModel(true);
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"unittest") == nullptr);
}

// Make sure that load routine doesn't delete prepopulated engines that no
// longer exist in the prepopulate data if it has been modified by the user.
TEST_P(TemplateURLServiceTest, LoadRetainsModifiedProvider) {
  // Create a preloaded template url and add it to a loaded model.
  test_util()->ChangeModelToLoadState();
  TemplateURL* t_url =
      model()->Add(CreatePreloadedTemplateURL(false, kPrepopulatedId));

  // Do the copy after t_url is added so that the id is set.
  std::unique_ptr<TemplateURL> cloned_url =
      std::make_unique<TemplateURL>(t_url->data());
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(u"unittest"));

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Ensure that merging won't clear it if the user has edited it.
  test_util()->ResetModel(true);
  const TemplateURL* url_for_unittest =
      model()->GetTemplateURLForKeyword(u"unittest");
  ASSERT_TRUE(url_for_unittest != nullptr);
  AssertEquals(*cloned_url, *url_for_unittest);

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that save/reload retains the item.
  test_util()->ResetModel(true);
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"unittest") != nullptr);
}

// Make sure that load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it has been modified by the user.
TEST_P(TemplateURLServiceTest, LoadSavesPrepopulatedDefaultSearchProvider) {
  test_util()->VerifyLoad();
  // Verify that the default search provider is set to something.
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != nullptr);
  std::unique_ptr<TemplateURL> cloned_url(
      new TemplateURL(default_search->data()));

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model and check that the default search provider
  // was properly saved.
  test_util()->ResetModel(true);
  default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != nullptr);
  AssertEquals(*cloned_url, *default_search);
}

// Make sure that the load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it is the default search provider.
TEST_P(TemplateURLServiceTest, LoadRetainsDefaultProvider) {
  // Set the default search provider to a preloaded template url which
  // is not in the current set of preloaded template urls and save
  // the result.
  test_util()->ChangeModelToLoadState();
  TemplateURL* t_url =
      model()->Add(CreatePreloadedTemplateURL(true, kPrepopulatedId));
  model()->SetUserSelectedDefaultSearchProvider(t_url);
  // Do the copy after t_url is added and set as default so that its
  // internal state is correct.
  std::unique_ptr<TemplateURL> cloned_url =
      std::make_unique<TemplateURL>(t_url->data());

  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(u"unittest"));
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  base::RunLoop().RunUntilIdle();

  // Ensure that merging won't clear the prepopulated template url
  // which is no longer present if it's the default engine.
  test_util()->ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(u"unittest");
    ASSERT_TRUE(keyword_url != nullptr);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that the update was saved.
  test_util()->ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(u"unittest");
    ASSERT_TRUE(keyword_url != nullptr);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }
}

// Make sure that the load routine sets a default search provider if it was
// missing and not managed.
TEST_P(TemplateURLServiceTest, LoadEnsuresDefaultSearchProviderExists) {
  // Force the model to load and make sure we have a default search provider.
  test_util()->VerifyLoad();
  EXPECT_TRUE(model()->GetDefaultSearchProvider());

  EXPECT_TRUE(model()->GetDefaultSearchProvider()->SupportsReplacement(
      search_terms_data()));

  // Force the model to load and make sure we have a default search provider.
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(default_search);
  EXPECT_TRUE(default_search->SupportsReplacement(search_terms_data()));

  // Make default search provider unusable (no search terms).  Using
  // GetTemplateURLForKeyword() returns a non-const pointer.
  model()->ResetTemplateURL(
      model()->GetTemplateURLForKeyword(default_search->keyword()), u"test",
      u"test", "http://example.com/");
  base::RunLoop().RunUntilIdle();

  // Reset the model and load it. There should be a usable default search
  // provider.
  test_util()->ResetModel(true);

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->GetDefaultSearchProvider()->SupportsReplacement(
      search_terms_data()));
}

// Make sure that the load routine does not update user modified starter pack
// engines unless the current version is incompatible.
TEST_P(TemplateURLServiceTest,
       LoadUpdatesStarterPackOnlyIfIncompatibleVersion) {
  test_util()->ResetModel(true);

  // Modify a starter pack template URL. Verify load does NOT modify the title
  // if current version is compatible (>= to first compatible version).
  const int first_compatible_version =
      TemplateURLStarterPackData::GetFirstCompatibleDataVersion();
  test_util()->web_data_service()->SetStarterPackKeywordVersion(
      first_compatible_version);

  TemplateURL* t_url = model()->GetTemplateURLForKeyword(u"@history");
  EXPECT_GT(t_url->starter_pack_id(), 0);
  const std::u16string original_title = t_url->short_name();

  model()->ResetTemplateURL(t_url, u"not history", u"@history", t_url->url());
  base::RunLoop().RunUntilIdle();

  // Reset the model and load it.
  test_util()->ResetModel(true);

  t_url = model()->GetTemplateURLForKeyword(u"@history");
  EXPECT_EQ(t_url->short_name(), u"not history");

  // Now test if current version is greater than last compatible version, we
  // should still not modify the user edited data.
  test_util()->web_data_service()->SetStarterPackKeywordVersion(
      first_compatible_version + 1);
  // Reset the model and load it.
  test_util()->ResetModel(true);

  t_url = model()->GetTemplateURLForKeyword(u"@history");
  EXPECT_EQ(t_url->short_name(), u"not history");

  // Now set the starter pack resource version to something less than the last
  // compatible version number, and verify that the title gets overridden back
  // to the default value.
  test_util()->web_data_service()->SetStarterPackKeywordVersion(
      first_compatible_version - 1);

  test_util()->ResetModel(true);
  t_url = model()->GetTemplateURLForKeyword(u"@history");
  EXPECT_EQ(t_url->short_name(), original_title);
}

// Simulates failing to load the webdb and makes sure the default search
// provider is valid.
TEST_P(TemplateURLServiceTest, FailedInit) {
  test_util()->VerifyLoad();

  test_util()->ClearModel();
  test_util()->web_data_service()->ShutdownDatabase();

  test_util()->ResetModel(false);
  model()->Load();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

// Verifies that if the default search URL preference is managed, we report
// the default search as managed.  Also check that we are getting the right
// values.
TEST_P(TemplateURLServiceTest, TestManagedDefaultSearch) {
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  test_util()->ResetObserverCount();

  // Set a regular default search provider.
  TemplateURL* regular_default = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16");
  VerifyObserverCount(1);
  model()->SetUserSelectedDefaultSearchProvider(regular_default);
  // Adding the URL and setting the default search provider should have caused
  // notifications.
  VerifyObserverCount(1);
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Set a managed preference that establishes a default search provider.
  std::unique_ptr<TemplateURLData> managed = CreateTestSearchEngine();
  SetManagedDefaultSearchPreferences(*managed, true, test_util()->profile());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  auto expected_managed_default1 = std::make_unique<TemplateURL>(*managed);
  const TemplateURL* actual_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default1.get(), actual_managed_default);
  EXPECT_TRUE(model()->ShowInDefaultList(actual_managed_default));

  // Update the managed preference and check that the model has changed.
  TemplateURLData managed2;
  managed2.SetShortName(u"test2");
  managed2.SetKeyword(u"other.com");
  managed2.SetURL("http://other.com/search?t={searchTerms}");
  managed2.suggestions_url = "http://other.com/suggest?t={searchTerms}";
  SetManagedDefaultSearchPreferences(managed2, true, test_util()->profile());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are now getting is the correct one.
  auto expected_managed_default2 = std::make_unique<TemplateURL>(managed2);
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default2.get(), actual_managed_default);
  EXPECT_EQ(model()->ShowInDefaultList(actual_managed_default), true);

  // Remove all the managed prefs and check that we are no longer managed.
  RemoveManagedDefaultSearchPreferences(test_util()->profile());
  VerifyObserverFired();
  EXPECT_FALSE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // The default should now be the user preference.
  const TemplateURL* actual_final_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(regular_default, actual_final_managed_default);
  EXPECT_EQ(model()->ShowInDefaultList(actual_final_managed_default), true);

  // Disable the default search provider through policy.
  SetManagedDefaultSearchPreferences(managed2, false, test_util()->profile());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(nullptr == model()->GetDefaultSearchProvider());
  EXPECT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Re-enable it.
  SetManagedDefaultSearchPreferences(*managed, true, test_util()->profile());
  VerifyObserverFired();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_EQ(initial_count + 2, model()->GetTemplateURLs().size());

  // Verify that the default manager we are getting is the managed one.
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default1.get(), actual_managed_default);
  EXPECT_EQ(model()->ShowInDefaultList(actual_managed_default), true);

  // Clear the model and disable the default search provider through policy.
  // Verify that there is no default search provider after loading the model.
  // This checks against regressions of http://crbug.com/67180

  // First, remove the preferences, reset the model, and set a default.
  RemoveManagedDefaultSearchPreferences(test_util()->profile());
  test_util()->ResetModel(true);
  TemplateURL* new_default = model()->GetTemplateURLForKeyword(u"key1");
  ASSERT_FALSE(new_default == nullptr);
  model()->SetUserSelectedDefaultSearchProvider(new_default);
  EXPECT_EQ(new_default, model()->GetDefaultSearchProvider());

  // Now reset the model again but load it after setting the preferences.
  test_util()->ResetModel(false);
  SetManagedDefaultSearchPreferences(*managed, false, test_util()->profile());
  test_util()->VerifyLoad();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(model()->GetDefaultSearchProvider() == nullptr);
}

// Test that if we load a TemplateURL with an empty GUID, the load process
// assigns it a newly generated GUID.
TEST_P(TemplateURLServiceTest, PatchEmptySyncGUID) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(u"google");
  data.SetKeyword(u"keyword");
  data.SetURL("http://www.google.com/foo/bar");
  data.sync_guid.clear();
  model()->Add(std::make_unique<TemplateURL>(data));

  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());

  // Reload the model to verify it was actually saved to the database and
  // assigned a new GUID when brought back.
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* loaded_url = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_FALSE(loaded_url == nullptr);
  ASSERT_FALSE(loaded_url->sync_guid().empty());
}

// Test that if we load a TemplateURL with duplicate input encodings, the load
// process de-dupes them.
TEST_P(TemplateURLServiceTest, DuplicateInputEncodings) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(u"google");
  data.SetKeyword(u"keyword");
  data.SetURL("http://www.google.com/foo/bar");
  std::vector<std::string> encodings;
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("Windows-1252");
  model()->Add(std::make_unique<TemplateURL>(data));

  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  const TemplateURL* loaded_url = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_TRUE(loaded_url != nullptr);
  EXPECT_EQ(8U, loaded_url->input_encodings().size());

  // Reload the model to verify it was actually saved to the database and the
  // duplicate encodings were removed.
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_FALSE(loaded_url == nullptr);
  EXPECT_EQ(4U, loaded_url->input_encodings().size());
}

TEST_P(TemplateURLServiceTest, DefaultExtensionEngine) {
  test_util()->VerifyLoad();
  // Add third-party default search engine.
  TemplateURL* user_dse =
      AddKeywordWithDate("user", "user", "http://www.goo.com/s?q={searchTerms}",
                         std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  TemplateURL* ext_dse_ptr =
      AddExtensionSearchEngine("extension_keyword", "extension_id", true);
  EXPECT_EQ(ext_dse_ptr, model()->GetDefaultSearchProvider());

  test_util()->RemoveExtensionControlledTURL("extension_id");
  ExpectSimilar(user_dse, model()->GetDefaultSearchProvider());
}

TEST_P(TemplateURLServiceTest, SetDefaultExtensionEngineAndRemoveUserDSE) {
  test_util()->VerifyLoad();
  // Add third-party default search engine.
  TemplateURL* user_dse =
      AddKeywordWithDate("user", "user", "http://www.goo.com/s?q={searchTerms}",
                         std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  TemplateURL* ext_dse_ptr =
      AddExtensionSearchEngine("extension_keyword", "extension_id", true);
  EXPECT_EQ(ext_dse_ptr, model()->GetDefaultSearchProvider());
  auto* prefs = test_util()->profile()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  std::string dse_guid = GetDefaultSearchProviderGuidFromPrefs(*prefs);
  EXPECT_EQ(user_dse->sync_guid(), dse_guid);

  model()->Remove(user_dse);
  EXPECT_EQ(ext_dse_ptr, model()->GetDefaultSearchProvider());

  test_util()->RemoveExtensionControlledTURL("extension_id");
  // The DSE is set to the fallback search engine.
  EXPECT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_NE(dse_guid, GetDefaultSearchProviderGuidFromPrefs(*prefs));
}

TEST_P(TemplateURLServiceTest, DefaultExtensionEnginePersist) {
  test_util()->VerifyLoad();
  // Add third-party default search engine.
  TemplateURL* user_dse =
      AddKeywordWithDate("user", "user", "http://www.goo.com/s?q={searchTerms}",
                         std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Create non-default extension search engine.
  AddExtensionSearchEngine("extension1_keyword", "extension1_id", false);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Create default extension search engine.
  TemplateURL* ext_dse_ptr =
      AddExtensionSearchEngine("extension2_keyword", "extension2_id", true);
  EXPECT_EQ(ext_dse_ptr, model()->GetDefaultSearchProvider());
  auto cloned_ext_dse = std::make_unique<TemplateURL>(ext_dse_ptr->data());

  // A default search engine set by an extension must be persisted across
  // browser restarts, until the extension is unloaded/disabled.
  test_util()->ResetModel(false);
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"extension2_keyword"));
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());

  // Non-default extension engines are not persisted across restarts.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"extension1_keyword"));
}

TEST_P(TemplateURLServiceTest, DefaultExtensionEnginePersistsBeforeLoad) {
  // Chrome will load the extension system before the TemplateURLService, so
  // extensions controlling the default search engine may be registered before
  // the service has loaded.
  const TemplateURL* ext_dse =
      AddExtensionSearchEngine("extension1_keyword", "extension1_id", true);
  auto cloned_ext_dse = std::make_unique<TemplateURL>(ext_dse->data());

  // Default search engine from extension must be persisted between browser
  // restarts, and should be available before the TemplateURLService is loaded.
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"extension1_keyword"));
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());

  // Check extension DSE is the same after service load.
  test_util()->VerifyLoad();
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());
}

// Checks that correct priority is applied when resolving conflicts between the
// omnibox extension, search engine extension and user search engines with same
// keyword.
TEST_P(TemplateURLServiceTest, KeywordConflictNonReplaceableEngines) {
  test_util()->VerifyLoad();
  // TemplateURLData used for user engines.
  std::unique_ptr<TemplateURLData> turl_data =
      GenerateDummyTemplateURLData("common_keyword");
  turl_data->safe_for_autoreplace = false;
  turl_data->last_modified = base::Time();

  // Add non replaceable user engine.
  const TemplateURL* user1 =
      model()->Add(std::make_unique<TemplateURL>(*turl_data));

  // Add default extension engine with same keyword as user engine.
  const TemplateURL* extension =
      AddExtensionSearchEngine("common_keyword", "extension_id", true,
                               Time::FromSecondsSinceUnixEpoch(2));

  // Add another non replaceable user engine with same keyword as extension.
  // But make it slightly "better" than the other one via last-modified date.
  turl_data->last_modified = base::Time::FromTimeT(20);
  const TemplateURL* user2 =
      model()->Add(std::make_unique<TemplateURL>(*turl_data));
  turl_data->last_modified = base::Time();

  // Check extension DSE is set as default and its keyword is not changed.
  const TemplateURL* current_dse = model()->GetDefaultSearchProvider();
  EXPECT_EQ(extension, current_dse);
  EXPECT_EQ(u"common_keyword", current_dse->keyword());

  // Register omnibox keyword with same keyword as extension.
  // Use |install_time| value less than in AddExtensionSearchEngine call above
  // to check that omnibox api keyword is ranked higher even if installed
  // earlier.
  model()->RegisterOmniboxKeyword("omnibox_api_extension_id", "extension_name",
                                  "common_keyword", "http://test3",
                                  Time::FromSecondsSinceUnixEpoch(1));
  TemplateURL* omnibox_api = model()->FindTemplateURLForExtension(
      "omnibox_api_extension_id", TemplateURL::OMNIBOX_API_EXTENSION);

  // Expect that all four engines kept their keywords.
  EXPECT_EQ(u"common_keyword", user1->keyword());
  EXPECT_EQ(u"common_keyword", user2->keyword());
  EXPECT_EQ(u"common_keyword", extension->keyword());
  EXPECT_EQ(u"common_keyword", omnibox_api->keyword());

  // Omnibox api is accessible by keyword as most relevant.
  EXPECT_EQ(omnibox_api, model()->GetTemplateURLForKeyword(u"common_keyword"));
  // Extension controlled search engine is still set as default and can be found
  // in TemplateURLService.
  EXPECT_EQ(extension, model()->GetDefaultSearchProvider());
  EXPECT_EQ(extension,
            model()->FindTemplateURLForExtension(
                "extension_id", TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION));

  // Test removing engines.
  // Remove omnibox api extension.
  model()->RemoveExtensionControlledTURL("omnibox_api_extension_id",
                                         TemplateURL::OMNIBOX_API_EXTENSION);
  // Expect that keyword is now corresponds to extension search engine.
  EXPECT_EQ(extension, model()->GetTemplateURLForKeyword(u"common_keyword"));
  // Remove extension engine.
  model()->RemoveExtensionControlledTURL(
      "extension_id", TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  EXPECT_NE(extension, model()->GetDefaultSearchProvider());
  // Now latest user engine is returned for keyword.
  EXPECT_EQ(user2, model()->GetTemplateURLForKeyword(u"common_keyword"));
}

// Verifies that we don't have reentrant behavior when resolving default search
// provider keyword conflicts. crbug.com/1031506
TEST_P(TemplateURLServiceTest, DefaultSearchProviderKeywordConflictReentrancy) {
  // Merely loading should increment the count once.
  test_util()->VerifyLoad();
  EXPECT_EQ(1, test_util()->dsp_set_to_google_callback_count());

  // We use a fake {google:baseURL} to take advantage of our existing
  // dsp_change_callback mechanism. The actual behavior we are testing is common
  // to all search providers - this is just for testing convenience.
  //
  // Add two of these with different keywords. Note they should be replaceable,
  // so that we can trigger the reentrant behavior.
  TemplateURL* google_1 =
      AddKeywordWithDate("name1", "key1", "{google:baseURL}/{searchTerms}",
                         std::string(), std::string(), std::string(), true);
  TemplateURL* google_2 =
      AddKeywordWithDate("name2", "key2", "{google:baseURL}/{searchTerms}",
                         std::string(), std::string(), std::string(), true);
  ASSERT_TRUE(google_1);
  ASSERT_TRUE(google_2);
  EXPECT_NE(google_1->data().sync_guid, google_2->data().sync_guid);

  // Set the DSE to google_1, and see that we've changed the DSP twice now.
  model()->SetUserSelectedDefaultSearchProvider(google_1);
  EXPECT_EQ(2, test_util()->dsp_set_to_google_callback_count());

  // Set the DSE to the google_2 (with a different GUID), but with a keyword
  // that conflicts with the google_1. This should remove google_1.
  TemplateURLData google_2_data_copy = google_2->data();
  google_2_data_copy.SetKeyword(u"key1");
  TemplateURL google_2_copy(google_2_data_copy);
  model()->SetUserSelectedDefaultSearchProvider(&google_2_copy);

  // Verify that we only changed the DSP one additional time for a total of 3.
  // If this fails with a larger count, likely the code is doing something
  // reentrant or thrashing the DSP in other ways that can cause undesirable
  // behavior.
  EXPECT_EQ(3, test_util()->dsp_set_to_google_callback_count())
      << "A failure here means you're likey getting undesired reentrant "
         "behavior on ApplyDefaultSearchChangeNoMetrics.";
}

TEST_P(TemplateURLServiceTest, ReplaceableEngineUpdateHandlesKeywordConflicts) {
  test_util()->VerifyLoad();
  // Add 2 replaceable user engine with different keywords.
  TemplateURL* user1 =
      AddKeywordWithDate("user_engine1", "user1", "http://test1", std::string(),
                         std::string(), std::string(), true);
  TemplateURL* user2 =
      AddKeywordWithDate("user_engine2", "user2", "http://test2", std::string(),
                         std::string(), std::string(), true);
  // Update first engine to conflict with second by keyword. This should
  // overwrite the second engine.
  model()->ResetTemplateURL(user1, u"title", u"user2",
                            "http://test_search.com");
  // Check that first engine can now be found by new keyword.
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(u"user2"));
  // Update to return first engine original keyword.
  model()->ResetTemplateURL(user1, u"title", u"user1",
                            "http://test_search.com");
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(u"user1"));
  // Expect that |user2| is now unmasked, since we don't delete replaceable
  // engines during the Update() phase, only on Add().
  EXPECT_EQ(user2, model()->GetTemplateURLForKeyword(u"user2"));
}

// Verifies that we favor prepopulated engines over other safe_for_autoreplace()
// engines, even if they are newer. Also verifies that we never remove the
// prepopulated engine, even if outranked. https://crbug.com/1164024
TEST_P(TemplateURLServiceTest, KeywordConflictFavorsPrepopulatedEngines) {
  test_util()->VerifyLoad();

  // Add prepopulated engine with prepopulate_id == 42, created at time == 10.
  TemplateURL* prepopulated = model()->Add(CreateKeywordWithDate(
      model(), "prepopulated", "common_keyword", "http://test1", std::string(),
      std::string(), std::string(), true, 42, "UTF-8",
      base::Time::FromTimeT(10)));
  ASSERT_THAT(prepopulated, NotNull());
  TemplateURLData prepopulated_data = prepopulated->data();

  // Add a newer (time == 20) autogenerated engine with the same keyword.
  TemplateURL* newer_autogenerated_engine = AddKeywordWithDate(
      "autogenerated", "common_keyword", "http://test2", std::string(),
      std::string(), std::string(), true, "UTF-8", base::Time::FromTimeT(20));

  // Verify that the prepopulated engine was added, and the newer autogenerated
  // engine was discarded. Also check that data has not changed.
  EXPECT_EQ(nullptr, newer_autogenerated_engine);
  EXPECT_EQ(prepopulated, model()->GetTemplateURLForKeyword(u"common_keyword"));
  EXPECT_TRUE(TemplateURL::MatchesData(prepopulated, &prepopulated_data,
                                       model()->search_terms_data()));

  // Now add a non-replaceable (user-added) and newer engine, which should
  // outrank the prepopulated engine.
  std::string prepopulated_guid = prepopulated->sync_guid();
  TemplateURL* newer_user_engine = AddKeywordWithDate(
      "user_engine", "common_keyword", "http://test2", std::string(),
      std::string(), std::string(), false, "UTF-8", base::Time::FromTimeT(20));

  // Verify that the user engine takes over, but that we didn't remove the
  // prepopulated engine during deduplication (it can still be found by guid).
  ASSERT_THAT(newer_user_engine, NotNull());
  ASSERT_EQ(newer_user_engine,
            model()->GetTemplateURLForKeyword(u"common_keyword"));
  EXPECT_EQ(prepopulated, model()->GetTemplateURLForGUID(prepopulated_guid));

  // Verify the prepopulated engine is "unmasked" if we remove the user engine.
  model()->Remove(newer_user_engine);
  EXPECT_EQ(prepopulated, model()->GetTemplateURLForKeyword(u"common_keyword"));

  // Adding a prepopulated engine must never fail, even if it's outranked
  // immediately, because the DSE repair mechanism relies on that. Verify this
  // by adding a worse prepopulated engine than our existing one (time == 0).
  TemplateURL* worse_prepopulated = model()->Add(CreateKeywordWithDate(
      model(), "worse_prepopulated", "common_keyword", "http://test1",
      std::string(), std::string(), std::string(), true, 42, "UTF-8",
      base::Time::FromTimeT(0)));
  ASSERT_THAT(worse_prepopulated, NotNull());
}

TEST_P(TemplateURLServiceTest, CheckNonreplaceableEnginesKeywordsConflicts) {
  test_util()->VerifyLoad();

  const std::u16string kCommonKeyword = u"common_keyword";
  // 1. Add non replaceable user engine.
  const TemplateURL* user1 =
      AddKeywordWithDate("nonreplaceable", "common_keyword", "http://test1",
                         std::string(), std::string(), std::string(), false);

  // Check it is accessible by keyword and host.
  EXPECT_EQ(kCommonKeyword, user1->keyword());
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));

  // 2. Add another non replaceable user engine with same keyword but different
  // search url. Make it a bit "better" with a non-zero date.
  const TemplateURL* user2 = AddKeywordWithDate(
      "nonreplaceable2", "common_keyword", "http://test2", std::string(),
      std::string(), std::string(), false, "UTF-8", base::Time::FromTimeT(20));
  // Both engines must be accessible by host. Prefer user2 because newer.
  EXPECT_EQ(kCommonKeyword, user1->keyword());
  EXPECT_EQ(kCommonKeyword, user2->keyword());
  EXPECT_EQ(user2, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));
  EXPECT_EQ(user2, model()->GetTemplateURLForHost("test2"));

  // Check conflict between search engines with html tags embedded in URL host.
  // URLs with embedded HTML canonicalize to contain uppercase characters in the
  // hostname. Ensure these URLs are still handled correctly for conflict
  // resolution.
  const TemplateURL* embed_better = AddKeywordWithDate(
      "nonreplaceable5", "embedded.%3chtml%3eweb",
      "http://embedded.<html>web/?q={searchTerms}", std::string(),
      std::string(), std::string(), false, "UTF-8", base::Time::FromTimeT(20));
  EXPECT_EQ(u"embedded.%3chtml%3eweb", embed_better->keyword());
  EXPECT_EQ(embed_better,
            model()->GetTemplateURLForKeyword(u"embedded.%3chtml%3eweb"));
  const TemplateURL* embed_worse =
      AddKeywordWithDate("nonreplaceable6", "embedded.%3chtml%3eweb",
                         "http://embedded.<html>web/?q={searchTerms}",
                         std::string(), std::string(), std::string(), false);
  // Expect both to have kept their keyword, but to return the "better" one
  // when requesting the engine for the shared keyword.
  EXPECT_EQ(u"embedded.%3chtml%3eweb", embed_better->keyword());
  EXPECT_EQ(u"embedded.%3chtml%3eweb", embed_worse->keyword());
  EXPECT_EQ(embed_better,
            model()->GetTemplateURLForKeyword(u"embedded.%3chtml%3eweb"));
}

TEST_P(TemplateURLServiceTest, CheckReplaceableEnginesKeywordsConflicts) {
  test_util()->VerifyLoad();

  const std::u16string kCommonKeyword = u"common_keyword";
  // 1. Add non replaceable user engine with common keyword.
  const TemplateURL* user1 =
      AddKeywordWithDate("nonreplaceable", "common_keyword", "http://test1",
                         std::string(), std::string(), std::string(), false);
  // Check it is accessible by keyword and host.
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));

  // 2. Try to add replaceable user engine with conflicting keyword. Addition
  // must fail, even if it has a more recent date.
  const TemplateURL* user2 = AddKeywordWithDate(
      "replaceable", "common_keyword", "http://test2", std::string(),
      std::string(), std::string(), true, "UTF-8", base::Time::FromTimeT(20));
  EXPECT_FALSE(user2);
  EXPECT_FALSE(model()->GetTemplateURLForHost("test2"));

  const std::u16string kCommonKeyword2 = u"common_keyword2";
  // 3. Add replaceable user engine with non conflicting keyword.
  const TemplateURL* user3 =
      AddKeywordWithDate("replaceable2", "common_keyword2", "http://test3",
                         std::string(), std::string(), std::string(), true);
  // New engine must exist and be accessible.
  EXPECT_EQ(user3, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user3, model()->GetTemplateURLForHost("test3"));

  // 4. Add a newer replaceable user engine with conflicting keyword.
  const TemplateURL* user4 = AddKeywordWithDate(
      "replaceable3", "common_keyword2", "http://test4", std::string(),
      std::string(), std::string(), true, "UTF-8", base::Time::FromTimeT(20));
  // New engine must exist and be accessible. Old replaceable engine must be
  // evicted from model, because it has a "worse" creation date.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test3"));
  EXPECT_EQ(user4, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user4, model()->GetTemplateURLForHost("test4"));

  // 5. Add non replaceable user engine with common_keyword2. Must evict
  // conflicting replaceable engine, even though it has a better creation date.
  const TemplateURL* user5 =
      AddKeywordWithDate("nonreplaceable5", "common_keyword2", "http://test5",
                         std::string(), std::string(), std::string(), false);
  EXPECT_FALSE(model()->GetTemplateURLForHost("test4"));
  EXPECT_EQ(user5, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user5, model()->GetTemplateURLForHost("test5"));
}

// Check that two extensions with the same engine are handled correctly.
TEST_P(TemplateURLServiceTest, ExtensionsWithSameKeywords) {
  test_util()->VerifyLoad();
  // Add non default extension engine.
  const TemplateURL* extension1 =
      AddExtensionSearchEngine("common_keyword", "extension_id1", false,
                               Time::FromSecondsSinceUnixEpoch(1));

  // Check that GetTemplateURLForKeyword returns last installed extension.
  EXPECT_EQ(extension1, model()->GetTemplateURLForKeyword(u"common_keyword"));

  // Add default extension engine with the same keyword.
  const TemplateURL* extension2 =
      AddExtensionSearchEngine("common_keyword", "extension_id2", true,
                               Time::FromSecondsSinceUnixEpoch(2));
  // Check that GetTemplateURLForKeyword now returns extension2 because it was
  // installed later.
  EXPECT_EQ(extension2, model()->GetTemplateURLForKeyword(u"common_keyword"));

  // Add another non default extension with same keyword. This action must not
  // change any keyword due to conflict.
  const TemplateURL* extension3 =
      AddExtensionSearchEngine("common_keyword", "extension_id3", false,
                               Time::FromSecondsSinceUnixEpoch(3));
  // Check that extension2 is set as default.
  EXPECT_EQ(extension2, model()->GetDefaultSearchProvider());

  // Check that GetTemplateURLForKeyword returns last installed extension.
  EXPECT_EQ(extension3, model()->GetTemplateURLForKeyword(u"common_keyword"));
  // Check that all keywords for extensions are left unchanged.
  EXPECT_EQ(u"common_keyword", extension1->keyword());
  EXPECT_EQ(u"common_keyword", extension2->keyword());
  EXPECT_EQ(u"common_keyword", extension3->keyword());
}

TEST_P(TemplateURLServiceTest, ExtensionEngineVsPolicy) {
  // Set a managed preference that establishes a default search provider.
  std::unique_ptr<TemplateURLData> managed = CreateTestSearchEngine();
  SetManagedDefaultSearchPreferences(*managed, true, test_util()->profile());
  test_util()->VerifyLoad();
  // Verify that the default manager we are getting is the managed one.
  auto expected_managed_default = std::make_unique<TemplateURL>(*managed);
  EXPECT_TRUE(model()->is_default_search_managed());
  const TemplateURL* actual_managed_default =
      model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default.get(), actual_managed_default);

  TemplateURL* ext_dse_ptr = AddExtensionSearchEngine("ext1", "ext1", true);
  EXPECT_EQ(ext_dse_ptr, model()->GetTemplateURLForKeyword(u"ext1"));
  EXPECT_TRUE(model()->is_default_search_managed());
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default.get(), actual_managed_default);
}

TEST_P(TemplateURLServiceTest, LastVisitedTimeUpdate) {
  test_util()->VerifyLoad();
  TemplateURL* original_url =
      AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");
  const Time original_last_visited = original_url->last_visited();
  model()->UpdateTemplateURLVisitTime(original_url);
  TemplateURL* modified_url = model()->GetTemplateURLForKeyword(u"key1");
  const Time modified_last_visited = modified_url->last_visited();
  EXPECT_NE(original_last_visited, modified_last_visited);
  test_util()->ResetModel(true);
  TemplateURL* reloaded_url = model()->GetTemplateURLForKeyword(u"key1");
  AssertTimesEqual(modified_last_visited, reloaded_url->last_visited());
}

TEST_P(TemplateURLServiceTest, LastModifiedTimeUpdate) {
  test_util()->VerifyLoad();
  TemplateURLData data;
  data.SetShortName(u"test_engine");
  data.SetKeyword(u"engine_keyword");
  data.SetURL("http://test_engine");
  data.safe_for_autoreplace = true;
  TemplateURL* original_url = model()->Add(std::make_unique<TemplateURL>(data));
  const Time original_last_modified = original_url->last_modified();
  model()->ResetTemplateURL(original_url, u"test_engine2", u"engine_keyword",
                            "http://test_engine");
  TemplateURL* update_url =
      model()->GetTemplateURLForKeyword(u"engine_keyword");
  const Time update_last_modified = update_url->last_modified();
  model()->SetUserSelectedDefaultSearchProvider(update_url);
  TemplateURL* reloaded_url =
      model()->GetTemplateURLForKeyword(u"engine_keyword");
  const Time reloaded_last_modified = reloaded_url->last_modified();
  EXPECT_NE(original_last_modified, reloaded_last_modified);
  EXPECT_EQ(update_last_modified, reloaded_last_modified);
}

TEST_P(TemplateURLServiceTest, GetDefaultSearchProviderIgnoringExtensions) {
  test_util()->VerifyLoad();

  const TemplateURL* const initial_default =
      model()->GetDefaultSearchProvider();
  ASSERT_TRUE(initial_default);

  EXPECT_EQ(initial_default,
            model()->GetDefaultSearchProviderIgnoringExtensions());

  // Add a new TemplateURL and set it as the default.
  TemplateURL* const new_user_default = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16");
  model()->SetUserSelectedDefaultSearchProvider(new_user_default);

  EXPECT_EQ(new_user_default, model()->GetDefaultSearchProvider());
  EXPECT_EQ(new_user_default,
            model()->GetDefaultSearchProviderIgnoringExtensions());

  // Add an extension-provided search engine. This becomes the new default.
  const TemplateURL* const extension_turl =
      AddExtensionSearchEngine("keyword", "extension id", true);
  EXPECT_EQ(extension_turl, model()->GetDefaultSearchProvider());
  EXPECT_EQ(new_user_default,
            model()->GetDefaultSearchProviderIgnoringExtensions());

  // Add a policy search engine; this takes priority over both the user-selected
  // and extension-provided engines.
  std::unique_ptr<TemplateURLData> managed_data = CreateTestSearchEngine();
  SetManagedDefaultSearchPreferences(*managed_data, true,
                                     test_util()->profile());

  const TemplateURL* const new_default = model()->GetDefaultSearchProvider();
  EXPECT_NE(new_default, extension_turl);
  ExpectSimilar(managed_data.get(), &new_default->data());
  EXPECT_EQ(new_default, model()->GetDefaultSearchProviderIgnoringExtensions());
}

TEST_P(TemplateURLServiceTest,
       EngineReturnedByGetDefaultSearchProviderIgnoringExtensionsTakesOver) {
  test_util()->VerifyLoad();

  // Add a new TemplateURL and set it as the default.
  TemplateURL* const new_user_default = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16");
  model()->SetUserSelectedDefaultSearchProvider(new_user_default);

  // Add an extension-provided search engine. This becomes the new default.
  constexpr char kExtensionId[] = "extension_id";
  const TemplateURL* const extension_turl =
      AddExtensionSearchEngine("keyword", kExtensionId, true);
  EXPECT_EQ(extension_turl, model()->GetDefaultSearchProvider());
  EXPECT_EQ(new_user_default,
            model()->GetDefaultSearchProviderIgnoringExtensions());

  // Remove the extension-provided engine; the |new_user_default| should take
  // over.
  test_util()->RemoveExtensionControlledTURL(kExtensionId);
  EXPECT_EQ(new_user_default, model()->GetDefaultSearchProvider());
  EXPECT_EQ(new_user_default,
            model()->GetDefaultSearchProviderIgnoringExtensions());
}

TEST_P(
    TemplateURLServiceTest,
    GetDefaultSearchProviderIgnoringExtensionsWhenDefaultSearchDisabledByPolicy) {
  test_util()->VerifyLoad();

  // Add a new TemplateURL and set it as the default.
  TemplateURL* const new_user_default = AddKeywordWithDate(
      "name1", "key1", "http://foo1/{searchTerms}", "http://sugg1",
      std::string(), "http://icon1", true, "UTF-8;UTF-16");
  model()->SetUserSelectedDefaultSearchProvider(new_user_default);

  // Disable default search by policy. Even though there's a user-selected
  // search, the default should be null.
  std::unique_ptr<TemplateURLData> managed_search = CreateTestSearchEngine();
  SetManagedDefaultSearchPreferences(*managed_search, false,
                                     test_util()->profile());
  EXPECT_EQ(nullptr, model()->GetDefaultSearchProvider());
  EXPECT_EQ(nullptr, model()->GetDefaultSearchProviderIgnoringExtensions());

  // Add an extension-provided engine; default search should still be null since
  // it's disabled by policy.
  AddExtensionSearchEngine("keyword", "extension id", true);
  EXPECT_EQ(nullptr, model()->GetDefaultSearchProvider());
  EXPECT_EQ(nullptr, model()->GetDefaultSearchProviderIgnoringExtensions());
}

// Tests that a TemplateURL's `is_active` field is correctly set and
// Omnibox.KeywordModeUsageByEngineType histogram is correctly emitted when a
// TemplateURL is activated and/or deactivated.
TEST_P(TemplateURLServiceTest, SetIsActiveTemplateURL) {
  TemplateURL* search_engine = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("keyword")));
  DCHECK(search_engine);

  // Before we activate or modify the search engine, it can be replaced by an
  // autogenerated keyword.
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(u"keyword", GURL()));

  base::HistogramTester histogram_tester;
  model()->SetIsActiveTemplateURL(search_engine, true);
  EXPECT_EQ(search_engine->is_active(), TemplateURLData::ActiveStatus::kTrue);
  histogram_tester.ExpectTotalCount(
      "Omnibox.KeywordModeUsageByEngineType.Activated", 1);
  // Check that we're no longer able to overwrite the keyword once it's been
  // activated.
  ASSERT_FALSE(model()->CanAddAutogeneratedKeyword(u"keyword", GURL()));

  model()->SetIsActiveTemplateURL(search_engine, false);
  EXPECT_EQ(search_engine->is_active(), TemplateURLData::ActiveStatus::kFalse);
  histogram_tester.ExpectTotalCount(
      "Omnibox.KeywordModeUsageByEngineType.Deactivated", 1);

  model()->SetIsActiveTemplateURL(search_engine, true);
  EXPECT_EQ(search_engine->is_active(), TemplateURLData::ActiveStatus::kTrue);
  histogram_tester.ExpectTotalCount(
      "Omnibox.KeywordModeUsageByEngineType.Activated", 2);
}

// Tests that the `Omnibox.KeywordModeUsageByEngineType.ActiveOnStartup` and
// `InactiveOnStartup` are emitted correctly when the model is loaded.
TEST_P(TemplateURLServiceTest, EmitTemplateURLActiveOnStartupHistogram) {
  test_util()->ResetModel(true);

  TemplateURL* search_engine1 = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("keyword1")));
  DCHECK(search_engine1);
  model()->SetIsActiveTemplateURL(search_engine1, true);

  TemplateURL* search_engine2 = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("keyword2")));
  DCHECK(search_engine2);
  model()->SetIsActiveTemplateURL(search_engine2, false);

  base::HistogramTester histogram_tester;
  test_util()->ResetModel(true);

  // All the starter pack entries should be active by default.  We haven't
  // deactivated them, so they should emit to the ActiveOnStartup histogram.
  histogram_tester.ExpectBucketCount(
      "Omnibox.KeywordModeUsageByEngineType.ActiveOnStartup",
      BuiltinEngineType::KEYWORD_MODE_STARTER_PACK_BOOKMARKS, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.KeywordModeUsageByEngineType.ActiveOnStartup",
      BuiltinEngineType::KEYWORD_MODE_STARTER_PACK_HISTORY, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.KeywordModeUsageByEngineType.ActiveOnStartup",
      BuiltinEngineType::KEYWORD_MODE_STARTER_PACK_TABS, 1);

  // We have one active and one inactive "non-builtin" search engine. Check that
  // those histograms are emitted correctly.
  histogram_tester.ExpectBucketCount(
      "Omnibox.KeywordModeUsageByEngineType.ActiveOnStartup",
      BuiltinEngineType::KEYWORD_MODE_NON_BUILT_IN, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.KeywordModeUsageByEngineType.InactiveOnStartup",
      BuiltinEngineType::KEYWORD_MODE_NON_BUILT_IN, 1);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_P(TemplateURLServiceTest, SiteSearchPolicyBeforeLoading) {
  constexpr char kKeyword1[] = "site_search_1";
  constexpr char kKeyword2[] = "site_search_2";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/false);

  // Set a managed preference that establishes site search providers before
  // the keywords table is loaded.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword2));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Ensure managed site search engines can be accessed even before the keywords
  // table loading is completed.
  for (auto& engine : site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }

  // Complete loading the DB.
  test_util()->VerifyLoad();

  // Ensure managed site search engines can still be accessed after the keywords
  // table is loaded.
  for (auto& engine : site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }

  // The following call has no effect on managed search engines.
  model()->RepairPrepopulatedSearchEngines();

  for (auto& engine : site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }
}

TEST_P(TemplateURLServiceTest, SiteSearchPolicyAfterLoading) {
  constexpr char kKeyword1[] = "site_search_1";
  constexpr char kKeyword2[] = "site_search_2";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  // Set a managed preference that establishes site search providers after
  // the keywords table loading is completed.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword2));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Ensure managed site search engines can be accessed.
  for (auto& engine : site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }
}

TEST_P(TemplateURLServiceTest, SiteSearchPolicyUpdates) {
  constexpr char kKeyword1[] = "site_search_1";
  constexpr char kKeyword2[] = "site_search_2";
  constexpr char kKeyword3[] = "site_search_3";
  constexpr char kKeyword4[] = "site_search_4";

  constexpr char16_t kKeyword1U16[] = u"site_search_1";
  constexpr char16_t kKeyword2U16[] = u"site_search_2";
  constexpr char16_t kKeyword3U16[] = u"site_search_3";
  constexpr char16_t kKeyword4U16[] = u"site_search_4";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  // Set a managed preference that establishes site search providers.
  // In the first stage, add keywords `kKeyword1`, `kKeyword2`, and `kKeyword3`.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector
      initial_site_search_engines;
  initial_site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  initial_site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword2));
  initial_site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword3));

  SetManagedSiteSearchSettingsPreference(initial_site_search_engines,
                                         test_util()->profile());

  // Ensure managed site search engines can be accessed.
  for (auto& engine : initial_site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }

  // Update the policy including one addition (`kKeyword4`), one deletion
  // (`kKeyword3`), one update (`kKeyword2`).
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector
      updated_site_search_engines;
  updated_site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  std::unique_ptr<TemplateURLData> updated_engine_2 =
      CreateTestSiteSearchEntry(kKeyword2);
  updated_engine_2->SetShortName(u"newname");
  updated_site_search_engines.push_back(std::move(updated_engine_2));
  updated_site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword4));

  SetManagedSiteSearchSettingsPreference(updated_site_search_engines,
                                         test_util()->profile());

  // Ensure the deleted site search engine can no longer be accessed.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(kKeyword3U16));

  // Ensure updated managed site search engines can be accessed.
  for (auto& engine : updated_site_search_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine.get(), &actual_turl->data());
  }

  // Delete all the entries, and ensure they can no longer be accessed.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(kKeyword1U16));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(kKeyword2U16));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(kKeyword3U16));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(kKeyword4U16));
}

TEST_P(TemplateURLServiceTest,
       NonFeaturedSiteSearchPolicyConflictWithExistingEngines) {
  constexpr char kKeyword1[] = "site_search_1";
  constexpr char kKeyword2[] = "site_search_2";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  // Create two pre-existing site search engines.
  TemplateURLService::TemplateURLVector existing_engines{
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeyword1, /*safe_for_autoreplace=*/true))),
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeyword2, /*safe_for_autoreplace=*/false))),
  };

  // Set a managed preference that establishes site search providers conflicting
  // with pre-existing search engines.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword2));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // A search engine set by the `SiteSearchSettings` policy only overrides
  // an existing engine if the latter has not been manually edited by the user
  // (`safe_for_autoreplace` is true).
  std::vector<const TemplateURLData*> expectations_after_policy{
      // Override existing engine because `safe_for_autoreplace` is true.
      site_search_engines[0].get(),
      // Do not override existing engine because `safe_for_autoreplace` is
      // false.
      &existing_engines[1]->data(),
  };
  for (auto* engine : expectations_after_policy) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine, &actual_turl->data());
  }

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 1},
                            {SiteSearchPolicyConflictType::kWithFeatured, 0},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 1},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // Once the policy no longer applies, the user should be able to continue
  // using the site search engines originally defined.
  for (const TemplateURL* user_engine : existing_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(user_engine->keyword());
    ASSERT_TRUE(actual_turl);
    AssertEquals(*user_engine, *actual_turl);
  }
}

TEST_P(TemplateURLServiceTest,
       FeaturedSiteSearchPolicyConflictWithExistingEngines) {
  constexpr char kKeyword1[] = "site_search_1";
  constexpr char kKeywordWithAt1[] = "@site_search_1";
  constexpr char kKeyword2[] = "site_search_2";
  constexpr char kKeywordWithAt2[] = "@site_search_2";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  // Create some pre-existing site search engines with variations of starting/
  // not starting with "@" and `safe_for_autoreplace` .
  TemplateURLService::TemplateURLVector existing_engines{
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeyword1, /*safe_for_autoreplace=*/true))),
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeywordWithAt1, /*safe_for_autoreplace=*/true))),
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeyword2, /*safe_for_autoreplace=*/false))),
      model()->Add(std::make_unique<TemplateURL>(
          CreateTestSearchEngineWithSafeForAutoreplace(
              kKeywordWithAt2, /*safe_for_autoreplace=*/false))),
  };

  // Set a managed preference that establishes site search providers
  // conflicting with pre-existing search engines.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword1));
  site_search_engines.push_back(
      CreateTestSiteSearchEntry(kKeywordWithAt1, /*featured_by_policy=*/true));
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword2));
  site_search_engines.push_back(
      CreateTestSiteSearchEntry(kKeywordWithAt2, /*featured_by_policy=*/true));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  std::vector<const TemplateURLData*> expectations_after_policy{
      // Override existing engine because `safe_for_autoreplace` is true.
      site_search_engines[0].get(),
      // Override existing engine because keyword starts with "@".
      site_search_engines[1].get(),
      // Do not override existing engine because `safe_for_autoreplace` is
      // false.
      &existing_engines[2]->data(),
      // Override existing engine because keyword starts with "@".
      site_search_engines[3].get(),
  };
  for (auto* engine : expectations_after_policy) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(engine->keyword());
    ASSERT_TRUE(actual_turl);
    ExpectSimilar(engine, &actual_turl->data());
  }

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 2},
                            {SiteSearchPolicyConflictType::kWithFeatured, 1},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 1},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // Once the policy no longer applies, the user should be able to continue
  // using the site search engines originally defined.
  for (const TemplateURL* user_engine : existing_engines) {
    const TemplateURL* actual_turl =
        model()->GetTemplateURLForKeyword(user_engine->keyword());
    ASSERT_TRUE(actual_turl);
    AssertEquals(*user_engine, *actual_turl);
  }
}

TEST_P(TemplateURLServiceTest, NonFeaturedSiteSearchPolicyConflictWithDSP) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  const TemplateURL* dse = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(dse);

  AssertEquals(dse, model()->GetTemplateURLForKeyword(dse->keyword()));

  // Set a managed preference that establishes a site search provider
  // conflicting with pre-defined default search engine not customized by the
  // user.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(
      CreateTestSiteSearchEntry(base::UTF16ToUTF8(dse->keyword())));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Expect no change in default search engine.
  EXPECT_EQ(dse, model()->GetDefaultSearchProvider());
  // Override DES for keyword search because `safe_for_autoreplace` is true.
  ExpectSimilar(site_search_engines[0].get(),
                &model()->GetTemplateURLForKeyword(dse->keyword())->data());

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 1},
                            {SiteSearchPolicyConflictType::kWithFeatured, 0},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 0},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // No changes to the DSE once the policy is no longer applied.
  EXPECT_EQ(dse, model()->GetDefaultSearchProvider());
  AssertEquals(dse, model()->GetTemplateURLForKeyword(dse->keyword()));
}

TEST_P(TemplateURLServiceTest,
       NonFeaturedSiteSearchPolicyConflictWithUserDefinedDSP) {
  constexpr char kKeyword[] = "keyword";
  constexpr char16_t kKeywordU16[] = u"keyword";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  TemplateURL* user_dse = AddKeywordWithDate(
      "DSE name", kKeyword, "http://www.goo.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(),
      /*safe_for_autoreplace=*/false);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  AssertEquals(user_dse, model()->GetTemplateURLForKeyword(kKeywordU16));

  // Set a managed preference that establishes a site search provider
  // conflicting with user-defined default search engine.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Expect no change in default search engine.
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  // Do not override DES for keyword search because `safe_for_autoreplace` is
  // false.
  AssertEquals(*user_dse, *model()->GetTemplateURLForKeyword(kKeywordU16));

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 0},
                            {SiteSearchPolicyConflictType::kWithFeatured, 0},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 1},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // No changes to the DSE once the policy is no longer applied.
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  AssertEquals(user_dse, model()->GetTemplateURLForKeyword(kKeywordU16));
}

TEST_P(TemplateURLServiceTest,
       NonFeaturedSiteSearchPolicyConflictWithDSPSetByExtension) {
  constexpr char kKeyword[] = "keyword";
  constexpr char16_t kKeywordU16[] = u"keyword";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  TemplateURL* extension_dse =
      AddExtensionSearchEngine(kKeyword, "extension_id", true);
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  AssertEquals(extension_dse, model()->GetTemplateURLForKeyword(kKeywordU16));

  // Set a managed preference that establishes a site search provider
  // conflicting with default search engine set by extension.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(kKeyword));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Expect no change in default search engine.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  // Do not override DSE for keyword search because `safe_for_autoreplace` is
  // false.
  AssertEquals(extension_dse, model()->GetTemplateURLForKeyword(kKeywordU16));

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 0},
                            {SiteSearchPolicyConflictType::kWithFeatured, 0},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 1},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // No changes to the DSE once the policy is no longer applied.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  AssertEquals(extension_dse, model()->GetTemplateURLForKeyword(kKeywordU16));
}

TEST_P(TemplateURLServiceTest,
       FeaturedSiteSearchPolicyConflictWithUserDefinedDSP) {
  constexpr char kKeyword[] = "@keyword";
  constexpr char16_t kKeywordU16[] = u"@keyword";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  TemplateURL* user_dse = AddKeywordWithDate(
      "DSE name", kKeyword, "http://www.goo.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(),
      /*safe_for_autoreplace=*/false);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  AssertEquals(user_dse, model()->GetTemplateURLForKeyword(kKeywordU16));

  // Set a managed preference that establishes a site search provider
  // conflicting with user-defined default search engine.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(
      CreateTestSiteSearchEntry(kKeyword, /*featured_by_policy=*/true));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Expect no change in default search engine.
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  // Override DES for keyword search because the site search engine is featured.
  ExpectSimilar(site_search_engines[0].get(),
                &model()->GetTemplateURLForKeyword(kKeywordU16)->data());

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 0},
                            {SiteSearchPolicyConflictType::kWithFeatured, 1},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 0},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // No changes to the DSE once the policy is no longer applied.
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
  AssertEquals(user_dse, model()->GetTemplateURLForKeyword(kKeywordU16));
}

TEST_P(TemplateURLServiceTest,
       FeaturedSiteSearchPolicyConflictWithDSPSetByExtension) {
  constexpr char kKeyword[] = "@keyword";
  constexpr char16_t kKeywordU16[] = u"@keyword";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  TemplateURL* extension_dse =
      AddExtensionSearchEngine(kKeyword, "extension_id", true);
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  AssertEquals(extension_dse, model()->GetTemplateURLForKeyword(kKeywordU16));

  // Set a managed preference that establishes a site search provider
  // conflicting with default search engine set by extension.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(
      CreateTestSiteSearchEntry(kKeyword, /*featured_by_policy=*/true));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Expect no change in default search engine.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  // Override DES for keyword search because the site search engine is featured.
  ExpectSimilar(site_search_engines[0].get(),
                &model()->GetTemplateURLForKeyword(kKeywordU16)->data());

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 0},
                            {SiteSearchPolicyConflictType::kWithFeatured, 1},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 0},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // No changes to the DSE once the policy is no longer applied.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  AssertEquals(extension_dse, model()->GetTemplateURLForKeyword(kKeywordU16));
}

TEST_P(TemplateURLServiceTest,
       FeaturedSiteSearchPolicyConflictWithStarterPack) {
  constexpr char kBookmarksKeyword[] = "@bookmarks";
  constexpr char16_t kBookmarksKeywordU16[] = u"@bookmarks";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kSiteSearchSettingsPolicy);
  base::HistogramTester histogram_tester;

  // Reset the model to ensure an `EnterpriseSiteSearchManager` instance is
  // created (it depends on `kSiteSearchSettingsPolicy` being enabled).
  test_util()->ResetModel(/*verify_load=*/true);

  const TemplateURL* bookmarks_entry =
      model()->GetTemplateURLForKeyword(kBookmarksKeywordU16);
  ASSERT_TRUE(bookmarks_entry);

  // Set a managed preference that establishes a site search provider
  // conflicting with pre-defined default search engine not customized by the
  // user.
  EnterpriseSiteSearchManager::OwnedTemplateURLDataVector site_search_engines;
  site_search_engines.push_back(CreateTestSiteSearchEntry(
      kBookmarksKeyword, /*featured_by_policy=*/true));

  SetManagedSiteSearchSettingsPreference(site_search_engines,
                                         test_util()->profile());

  // Override bookmarks for keyword search because the site search engine is
  // featured.
  ExpectSimilar(
      site_search_engines[0].get(),
      &model()->GetTemplateURLForKeyword(kBookmarksKeywordU16)->data());

  VerifySiteSearchPolicyConflictHistograms(
      histogram_tester, {
                            {SiteSearchPolicyConflictType::kNone, 1},
                            {SiteSearchPolicyConflictType::kWithFeatured, 0},
                            {SiteSearchPolicyConflictType::kWithNonFeatured, 0},
                        });

  // Reset the policy.
  SetManagedSiteSearchSettingsPreference(
      EnterpriseSiteSearchManager::OwnedTemplateURLDataVector(),
      test_util()->profile());

  // Go back to the original bookmarks search once the policy is no longer
  // applied.
  AssertEquals(bookmarks_entry,
               model()->GetTemplateURLForKeyword(kBookmarksKeywordU16));
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(,
                         TemplateURLServiceTest,
                         ::testing::Bool(),
                         &ParamToTestSuffix);
