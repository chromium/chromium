// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_host_to_urls_map.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;

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
  ASSERT_TRUE(expected != NULL);
  ASSERT_TRUE(actual != NULL);
  ExpectSimilar(&expected->data(), &actual->data());
}

std::unique_ptr<TemplateURLData> CreateTestSearchEngine() {
  auto result = std::make_unique<TemplateURLData>();
  result->SetShortName(ASCIIToUTF16("test1"));
  result->SetKeyword(ASCIIToUTF16("test.com"));
  result->SetURL("http://test.com/search?t={searchTerms}");
  result->favicon_url = GURL("http://test.com/icon.jpg");
  result->prepopulate_id = kPrepopulatedId;
  result->input_encodings = {"UTF-16", "UTF-32"};
  result->alternate_urls = {"http://test.com/search#t={searchTerms}"};
  return result;
}

}  // namespace


// TemplateURLServiceTest -----------------------------------------------------

class TemplateURLServiceTest : public testing::Test {
 public:
  TemplateURLServiceTest();

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

 private:
  content::BrowserTaskEnvironment
      task_environment_;  // To set up BrowserThreads.
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceTest);
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

TemplateURLServiceTest::TemplateURLServiceTest() {
}

void TemplateURLServiceTest::SetUp() {
  test_util_.reset(new TemplateURLServiceTestUtil);
}

void TemplateURLServiceTest::TearDown() {
  test_util_.reset();
}

TemplateURL* TemplateURLServiceTest::AddKeywordWithDate(
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

TemplateURL* TemplateURLServiceTest::AddExtensionSearchEngine(
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

void TemplateURLServiceTest::AssertEquals(const TemplateURL& expected,
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

void TemplateURLServiceTest::AssertTimesEqual(const Time& expected,
                                              const Time& actual) {
  // Because times are stored with a granularity of one second, there is a loss
  // of precision when serializing and deserializing the timestamps. Hence, only
  // expect timestamps to be equal to within one second of one another.
  ASSERT_LT((expected - actual).magnitude(), TimeDelta::FromSeconds(1));
}

std::unique_ptr<TemplateURL> TemplateURLServiceTest::CreatePreloadedTemplateURL(
    bool safe_for_autoreplace,
    int prepopulate_id) {
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("unittest"));
  data.SetKeyword(ASCIIToUTF16("unittest"));
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

void TemplateURLServiceTest::SetOverriddenEngines() {
  // Set custom search engine as default fallback through overrides.
  auto entry = std::make_unique<base::DictionaryValue>();
  entry->SetString("name", "override_name");
  entry->SetString("keyword", "override_keyword");
  entry->SetString("search_url", "http://override.com/s?q={searchTerms}");
  entry->SetString("favicon_url", "http://override.com/favicon.ico");
  entry->SetString("encoding", "UTF-8");
  entry->SetInteger("id", 1001);
  entry->SetString("suggest_url",
                   "http://override.com/suggest?q={searchTerms}");

  auto overrides_list = std::make_unique<base::ListValue>();
  overrides_list->Append(std::move(entry));

  auto* prefs = test_util()->profile()->GetTestingPrefService();
  prefs->SetUserPref(prefs::kSearchProviderOverridesVersion,
                     std::make_unique<base::Value>(1));
  prefs->SetUserPref(prefs::kSearchProviderOverrides,
                     std::move(overrides_list));
}

void TemplateURLServiceTest::VerifyObserverCount(int expected_changed_count) {
  EXPECT_EQ(expected_changed_count, test_util_->GetObserverCount());
  test_util_->ResetObserverCount();
}

void TemplateURLServiceTest::VerifyObserverFired() {
  EXPECT_LE(1, test_util_->GetObserverCount());
  test_util_->ResetObserverCount();
}


// Actual tests ---------------------------------------------------------------

TEST_F(TemplateURLServiceTest, Load) {
  test_util()->VerifyLoad();
}

TEST_F(TemplateURLServiceTest, AddUpdateRemove) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("google"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = true;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  data.sync_guid = "00000000-0000-0000-0000-000000000001";
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("keyword"),
                                                  GURL(), NULL));
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
  TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(*cloned_url, *loaded_url);
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("keyword"),
                                                  GURL(), NULL));

  // We expect the last_modified time to be updated to the present time on an
  // explicit reset.
  Time now = Time::Now();
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  clock->SetNow(now);
  model()->set_clock(std::move(clock));

  // Mutate an element and verify it succeeded.
  model()->ResetTemplateURL(loaded_url, ASCIIToUTF16("a"), ASCIIToUTF16("b"),
                            "c");
  ASSERT_EQ(ASCIIToUTF16("a"), loaded_url->short_name());
  ASSERT_EQ(ASCIIToUTF16("b"), loaded_url->keyword());
  ASSERT_EQ("c", loaded_url->url());
  ASSERT_FALSE(loaded_url->safe_for_autoreplace());
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("keyword"),
                                                  GURL(), NULL));
  ASSERT_FALSE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("b"), GURL(),
                                                   NULL));
  cloned_url.reset(new TemplateURL(loaded_url->data()));
  base::RunLoop().RunUntilIdle();
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("b"));
  ASSERT_TRUE(loaded_url != NULL);
  AssertEquals(*cloned_url, *loaded_url);
  // We changed a TemplateURL in the service, so ensure that the time was
  // updated.
  AssertTimesEqual(now, loaded_url->last_modified());

  // Remove an element and verify it succeeded.
  model()->Remove(loaded_url);
  VerifyObserverCount(1);
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("b")) == NULL);
}

TEST_F(TemplateURLServiceTest, AddSameKeyword) {
  test_util()->VerifyLoad();

  AddKeywordWithDate("first", "keyword", "http://test1", std::string(),
                     std::string(), std::string(), true);
  VerifyObserverCount(1);

  // Test what happens when we try to add a TemplateURL with the same keyword as
  // one in the model.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("second"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://test2");
  data.safe_for_autoreplace = false;
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));

  // Because the old TemplateURL was replaceable and the new one wasn't, the new
  // one should have replaced the old.
  VerifyObserverCount(1);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a replaceable TemplateURL.  This should just delete the
  // passed-in URL.
  data.SetShortName(ASCIIToUTF16("third"));
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  model()->Add(std::make_unique<TemplateURL>(data));
  VerifyObserverCount(0);
  EXPECT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url->keyword());
  EXPECT_FALSE(t_url->safe_for_autoreplace());

  // Now try adding a non-replaceable TemplateURL again.  This should uniquify
  // the existing entry's keyword.
  data.SetShortName(ASCIIToUTF16("fourth"));
  data.SetURL("http://test4");
  data.safe_for_autoreplace = false;
  TemplateURL* t_url2 = model()->Add(std::make_unique<TemplateURL>(data));
  VerifyObserverCount(1);
  EXPECT_EQ(t_url2, model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(ASCIIToUTF16("fourth"), t_url2->short_name());
  EXPECT_EQ(ASCIIToUTF16("keyword"), t_url2->keyword());
  EXPECT_EQ(ASCIIToUTF16("second"), t_url->short_name());
  EXPECT_EQ(ASCIIToUTF16("test2"), t_url->keyword());
}

TEST_F(TemplateURLServiceTest, AddOmniboxExtensionKeyword) {
  test_util()->VerifyLoad();

  AddKeywordWithDate("replaceable", "keyword1", "http://test1", std::string(),
                     std::string(), std::string(), true);
  AddKeywordWithDate("nonreplaceable", "keyword2", "http://test2",
                     std::string(), std::string(), std::string(), false);
  model()->RegisterOmniboxKeyword("test3", "extension", "keyword3",
                                  "http://test3", Time::FromDoubleT(1));
  TemplateURL* original3 =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword3"));
  ASSERT_TRUE(original3);

  // Extension keywords should override replaceable keywords.
  model()->RegisterOmniboxKeyword("id1", "test", "keyword1", "http://test4",
                                  Time());
  TemplateURL* extension1 = model()->FindTemplateURLForExtension(
      "id1", TemplateURL::OMNIBOX_API_EXTENSION);
  EXPECT_TRUE(extension1);
  EXPECT_EQ(extension1,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword1")));

  // They should also override non-replaceable keywords.
  model()->RegisterOmniboxKeyword("id2", "test", "keyword2", "http://test5",
                                  Time());
  TemplateURL* extension2 = model()->FindTemplateURLForExtension(
      "id2", TemplateURL::OMNIBOX_API_EXTENSION);
  ASSERT_TRUE(extension2);
  EXPECT_EQ(extension2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword2")));

  // They should override extension keywords added earlier.
  model()->RegisterOmniboxKeyword("id3", "test", "keyword3", "http://test6",
                                  Time::FromDoubleT(4));
  TemplateURL* extension3 = model()->FindTemplateURLForExtension(
      "id3", TemplateURL::OMNIBOX_API_EXTENSION);
  ASSERT_TRUE(extension3);
  EXPECT_EQ(extension3,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword3")));
}

TEST_F(TemplateURLServiceTest, AddSameKeywordWithOmniboxExtensionPresent) {
  test_util()->VerifyLoad();

  // Similar to the AddSameKeyword test, but with an extension keyword masking a
  // replaceable TemplateURL.  We should still do correct conflict resolution
  // between the non-template URLs.
  model()->RegisterOmniboxKeyword("test2", "extension", "keyword",
                                  "http://test2", Time());
  TemplateURL* extension =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(extension);
  // Adding a keyword that matches the extension.
  AddKeywordWithDate("replaceable", "keyword", "http://test1", std::string(),
                     std::string(), std::string(), true);

  // Adding another replaceable keyword should remove the existing one, but
  // leave the extension as is.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("name1"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
  data.SetURL("http://test3");
  data.safe_for_autoreplace = true;
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));
  EXPECT_EQ(extension,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(t_url, model()->GetTemplateURLForHost("test3"));
  // Check that previous replaceable engine with keyword is removed.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test1"));

  // Adding a nonreplaceable keyword should remove the existing replaceable
  // keyword, yet extension must still be set as the associated URL for this
  // keyword.
  data.SetShortName(ASCIIToUTF16("name2"));
  data.SetURL("http://test4");
  data.safe_for_autoreplace = false;
  TemplateURL* nonreplaceable =
      model()->Add(std::make_unique<TemplateURL>(data));
  EXPECT_EQ(extension,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
  EXPECT_EQ(nonreplaceable, model()->GetTemplateURLForHost("test4"));
  // Check that previous replaceable engine with keyword is removed.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test3"));
}

TEST_F(TemplateURLServiceTest, NotPersistOmniboxExtensionKeyword) {
  test_util()->VerifyLoad();

  // Register an omnibox keyword.
  model()->RegisterOmniboxKeyword("test", "extension", "keyword",
                                  "chrome-extension://test", Time());
  ASSERT_TRUE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));

  // Reload the data.
  test_util()->ResetModel(true);

  // Ensure the omnibox keyword is not persisted.
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));
}

TEST_F(TemplateURLServiceTest, ClearBrowsingData_Keywords) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

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
  // Try the other three states.
  AddKeywordWithDate("name5", "key5", "http://foo5", "http://suggest5",
                     std::string(), "http://icon5", false, "UTF-8;UTF-16", now,
                     Time(), Time());
  AddKeywordWithDate("name6", "key6", "http://foo6", "http://suggest6",
                     std::string(), "http://icon6", false, "UTF-8;UTF-16",
                     month_ago, Time(), Time());

  // We just added a few items, validate them.
  EXPECT_EQ(6U, model()->GetTemplateURLs().size());

  // Try removing from current timestamp. This should delete the one in the
  // future and one very recent one.
  model()->RemoveAutoGeneratedSince(now);
  EXPECT_EQ(4U, model()->GetTemplateURLs().size());

  // Try removing from two months ago. This should only delete items that are
  // auto-generated.
  model()->RemoveAutoGeneratedBetween(now - TimeDelta::FromDays(60), now);
  EXPECT_EQ(3U, model()->GetTemplateURLs().size());

  // Make sure the right values remain.
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(0U,
            model()->GetTemplateURLs()[0]->date_created().ToInternalValue());

  EXPECT_EQ(ASCIIToUTF16("key5"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_FALSE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());
  EXPECT_EQ(now.ToInternalValue(),
            model()->GetTemplateURLs()[1]->date_created().ToInternalValue());

  EXPECT_EQ(ASCIIToUTF16("key6"), model()->GetTemplateURLs()[2]->keyword());
  EXPECT_FALSE(model()->GetTemplateURLs()[2]->safe_for_autoreplace());
  EXPECT_EQ(month_ago.ToInternalValue(),
            model()->GetTemplateURLs()[2]->date_created().ToInternalValue());

  // Try removing from Time=0. This should delete one more.
  model()->RemoveAutoGeneratedSince(Time());
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
}

TEST_F(TemplateURLServiceTest, ClearBrowsingData_KeywordsForUrls) {
  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);
  Time month_ago = now - TimeDelta::FromDays(30);

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
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 url2),
      month_ago, now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(ASCIIToUTF16("key3"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());

  // Try removing foo1, but outside the range in which it was modified. It
  // should remain untouched.
  GURL url1("http://foo1");
  model()->RemoveAutoGeneratedForUrlsBetween(
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 url1),
      now, now + one_day);
  EXPECT_EQ(2U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
  EXPECT_EQ(ASCIIToUTF16("key3"), model()->GetTemplateURLs()[1]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[1]->safe_for_autoreplace());

  // Try removing foo3. This should delete foo3, but leave foo1 untouched.
  GURL url3("http://foo3");
  model()->RemoveAutoGeneratedForUrlsBetween(
      base::Bind(static_cast<bool (*)(const GURL&, const GURL&)>(operator==),
                 url3),
      month_ago, now + one_day + one_day);
  EXPECT_EQ(1U, model()->GetTemplateURLs().size());
  EXPECT_EQ(ASCIIToUTF16("key1"), model()->GetTemplateURLs()[0]->keyword());
  EXPECT_TRUE(model()->GetTemplateURLs()[0]->safe_for_autoreplace());
}

TEST_F(TemplateURLServiceTest, Reset) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("google"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
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
  const base::string16 new_short_name(ASCIIToUTF16("a"));
  const base::string16 new_keyword(ASCIIToUTF16("b"));
  const std::string new_url("c");
  model()->ResetTemplateURL(t_url, new_short_name, new_keyword, new_url);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(new_keyword, t_url->keyword());
  ASSERT_EQ(new_url, t_url->url());

  // Make sure the mappings in the model were updated.
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(new_keyword));
  ASSERT_EQ(nullptr,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword")));

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

TEST_F(TemplateURLServiceTest, CreateFromPlayAPI) {
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  const base::string16 short_name = ASCIIToUTF16("google");
  const base::string16 keyword = ASCIIToUTF16("keyword");
  const std::string search_url = "http://www.google.com/foo/bar";
  const std::string suggest_url = "http://www.google.com/suggest";
  const std::string favicon_url = "http://favicon.url";
  TemplateURL* t_url = model()->CreateOrUpdateTemplateURLFromPlayAPIData(
      short_name, keyword, search_url, suggest_url, favicon_url);
  ASSERT_EQ(short_name, t_url->short_name());
  ASSERT_EQ(keyword, t_url->keyword());
  ASSERT_EQ(search_url, t_url->url());
  ASSERT_EQ(suggest_url, t_url->suggestions_url());
  ASSERT_EQ(GURL(favicon_url), t_url->favicon_url());
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

TEST_F(TemplateURLServiceTest, UpdateFromPlayAPI) {
  base::string16 keyword = ASCIIToUTF16("keyword");

  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("google"));
  data.SetKeyword(keyword);
  data.SetURL("http://www.google.com/foo/bar");
  data.favicon_url = GURL("http://favicon.url");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.last_visited = Time::FromTimeT(100);
  TemplateURL* t_url = model()->Add(std::make_unique<TemplateURL>(data));

  VerifyObserverCount(1);
  base::RunLoop().RunUntilIdle();

  Time now = Time::Now();
  auto clock = std::make_unique<base::SimpleTestClock>();
  clock->SetNow(now);
  model()->set_clock(std::move(clock));

  // Reset the short name and url and make sure it takes.
  const base::string16 new_short_name = ASCIIToUTF16("new_name");
  const std::string new_search_url = "new_url";
  const std::string new_suggest_url = "new_suggest_url";
  const std::string new_favicon_url = "new_favicon_url";
  TemplateURL* updated_turl = model()->CreateOrUpdateTemplateURLFromPlayAPIData(
      new_short_name, keyword, new_search_url, new_suggest_url,
      new_favicon_url);
  ASSERT_EQ(t_url, updated_turl);
  ASSERT_EQ(new_short_name, t_url->short_name());
  ASSERT_EQ(keyword, t_url->keyword());
  ASSERT_EQ(new_search_url, t_url->url());
  ASSERT_EQ(new_suggest_url, t_url->suggestions_url());
  ASSERT_EQ(GURL(new_favicon_url), t_url->favicon_url());
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

TEST_F(TemplateURLServiceTest, DefaultSearchProvider) {
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

TEST_F(TemplateURLServiceTest, CantReplaceWithSameKeyword) {
  test_util()->ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("foo"), GURL(),
                                                  NULL));
  TemplateURL* t_url =
      AddKeywordWithDate("name1", "foo", "http://foo1", "http://sugg1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo2"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("foo"),
                                          GURL("http://foo2"), NULL));
}

TEST_F(TemplateURLServiceTest, CantReplaceWithSameHosts) {
  test_util()->ChangeModelToLoadState();
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("foo"),
                                         GURL("http://foo.com"), NULL));
  TemplateURL* t_url =
      AddKeywordWithDate("name1", "foo", "http://foo.com", "http://sugg1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");

  // Can still replace, newly added template url is marked safe to replace.
  ASSERT_TRUE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("bar"),
                                         GURL("http://foo.com"), NULL));

  // ResetTemplateURL marks the TemplateURL as unsafe to replace, so it should
  // no longer be replaceable.
  model()->ResetTemplateURL(t_url, t_url->short_name(), t_url->keyword(),
                            t_url->url());

  ASSERT_FALSE(model()->CanAddAutogeneratedKeyword(ASCIIToUTF16("bar"),
                                          GURL("http://foo.com"), NULL));
}

TEST_F(TemplateURLServiceTest, HasDefaultSearchProvider) {
  // We should have a default search provider even if we haven't loaded.
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Now force the model to load and make sure we still have a default.
  test_util()->VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceTest, DefaultSearchProviderLoadedFromPrefs) {
  test_util()->VerifyLoad();

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("a"));
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
  EXPECT_EQ(ASCIIToUTF16("a"), default_turl->short_name());
  EXPECT_EQ("http://url/{searchTerms}", default_turl->url());
  EXPECT_EQ("http://url2", default_turl->suggestions_url());
  EXPECT_EQ(id, default_turl->id());

  // Now do a load and make sure the default search provider really takes.
  test_util()->VerifyLoad();

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  AssertEquals(*cloned_url, *model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceTest, RepairPrepopulatedSearchEngines) {
  test_util()->VerifyLoad();

  // Edit Google search engine.
  TemplateURL* google = model()->GetTemplateURLForKeyword(
      ASCIIToUTF16("google.com"));
  ASSERT_TRUE(google);
  model()->ResetTemplateURL(google, ASCIIToUTF16("trash"), ASCIIToUTF16("xxx"),
                            "http://www.foo.com/s?q={searchTerms}");
  EXPECT_EQ(ASCIIToUTF16("trash"), google->short_name());
  EXPECT_EQ(ASCIIToUTF16("xxx"), google->keyword());

  // Add third-party default search engine.
  TemplateURL* user_dse = AddKeywordWithDate(
      "malware", "google.com", "http://www.goo.com/s?q={searchTerms}",
      std::string(), std::string(), std::string(), true);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Remove bing.
  TemplateURL* bing = model()->GetTemplateURLForKeyword(
      ASCIIToUTF16("bing.com"));
  ASSERT_TRUE(bing);
  model()->Remove(bing);
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("bing.com")));

  // Register an extension with bing keyword.
  model()->RegisterOmniboxKeyword("abcdefg", "extension_name", "bing.com",
                                  "http://abcdefg", Time());
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("bing.com")));

  model()->RepairPrepopulatedSearchEngines();

  // Google is default.
  ASSERT_EQ(google, model()->GetDefaultSearchProvider());
  // The keyword wasn't reverted.
  EXPECT_EQ(ASCIIToUTF16("trash"), google->short_name());
  EXPECT_EQ("www.google.com",
            google->GenerateSearchURL(model()->search_terms_data()).host());

  // Bing was repaired.
  bing =
      model()->FindNonExtensionTemplateURLForKeyword(ASCIIToUTF16("bing.com"));
  ASSERT_TRUE(bing);
  EXPECT_EQ(TemplateURL::NORMAL, bing->type());

  // User search engine is preserved.
  EXPECT_EQ(user_dse, model()->GetTemplateURLForHost("www.goo.com"));
  EXPECT_EQ(ASCIIToUTF16("google.com"), user_dse->keyword());
}

TEST_F(TemplateURLServiceTest, RepairSearchEnginesWithManagedDefault) {
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
TEST_F(TemplateURLServiceTest, RepairPrepopulatedEnginesUpdatesSyncGuid) {
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  EXPECT_TRUE(test_util()
                  ->profile()
                  ->GetTestingPrefService()
                  ->GetString(prefs::kSyncedDefaultSearchProviderGUID)
                  .empty());

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
            test_util()->profile()->GetTestingPrefService()->GetString(
                prefs::kSyncedDefaultSearchProviderGUID));

  model()->RepairPrepopulatedSearchEngines();

  // Check that initial search engine is returned as default after repair.
  ASSERT_EQ(initial_dse, model()->GetDefaultSearchProvider());
  // Check that initial_dse guid is stored in kSyncedDefaultSearchProviderGUID.
  const std::string dse_guid =
      test_util()->profile()->GetTestingPrefService()->GetString(
          prefs::kSyncedDefaultSearchProviderGUID);
  EXPECT_EQ(initial_dse->sync_guid(), dse_guid);
  EXPECT_EQ(initial_dse->keyword(),
            model()->GetTemplateURLForGUID(dse_guid)->keyword());
}

// Checks that RepairPrepopulatedEngines correctly updates sync guid for default
// search when search engines are overridden using pref.
TEST_F(TemplateURLServiceTest,
       RepairPrepopulatedEnginesWithOverridesUpdatesSyncGuid) {
  SetOverriddenEngines();
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  EXPECT_TRUE(test_util()
                  ->profile()
                  ->GetTestingPrefService()
                  ->GetString(prefs::kSyncedDefaultSearchProviderGUID)
                  .empty());

  TemplateURL* overridden_engine =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("override_keyword"));
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
            test_util()->profile()->GetTestingPrefService()->GetString(
                prefs::kSyncedDefaultSearchProviderGUID));

  model()->RepairPrepopulatedSearchEngines();

  // Check that overridden engine is returned as default after repair.
  ASSERT_EQ(overridden_engine, model()->GetDefaultSearchProvider());
  // Check that overridden_engine guid is stored in
  // kSyncedDefaultSearchProviderGUID.
  const std::string dse_guid =
      test_util()->profile()->GetTestingPrefService()->GetString(
          prefs::kSyncedDefaultSearchProviderGUID);
  EXPECT_EQ(overridden_engine->sync_guid(), dse_guid);
  EXPECT_EQ(overridden_engine->keyword(),
            model()->GetTemplateURLForGUID(dse_guid)->keyword());
}

// Checks that RepairPrepopulatedEngines correctly updates sync guid for default
// search when search engines is overridden by extension.
TEST_F(TemplateURLServiceTest,
       RepairPrepopulatedEnginesWithExtensionUpdatesSyncGuid) {
  test_util()->VerifyLoad();

  // The synced DSE GUID should be empty until the user selects something or
  // there is sync activity.
  EXPECT_TRUE(test_util()
                  ->profile()
                  ->GetTestingPrefService()
                  ->GetString(prefs::kSyncedDefaultSearchProviderGUID)
                  .empty());

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
            test_util()->profile()->GetTestingPrefService()->GetString(
                prefs::kSyncedDefaultSearchProviderGUID));

  // Add extension controlled default search engine.
  TemplateURL* extension_dse =
      AddExtensionSearchEngine("extension_dse", "extension_id", true);
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  // Check that user DSE guid is still stored in
  // kSyncedDefaultSearchProviderGUID.
  EXPECT_EQ(user_dse->sync_guid(),
            test_util()->profile()->GetTestingPrefService()->GetString(
                prefs::kSyncedDefaultSearchProviderGUID));

  model()->RepairPrepopulatedSearchEngines();
  // Check that extension engine is still default but sync guid is updated to
  // initial dse guid.
  EXPECT_EQ(extension_dse, model()->GetDefaultSearchProvider());
  EXPECT_EQ(initial_dse->sync_guid(),
            test_util()->profile()->GetTestingPrefService()->GetString(
                prefs::kSyncedDefaultSearchProviderGUID));
}

TEST_F(TemplateURLServiceTest, UpdateKeywordSearchTermsForURL) {
  struct TestData {
    const std::string url;
    const base::string16 term;
  } data[] = {
    { "http://foo/", base::string16() },
    { "http://foo/foo?q=xx", base::string16() },
    { "http://x/bar?q=xx", base::string16() },
    { "http://x/foo?y=xx", base::string16() },
    { "http://x/foo?q=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?a=b&q=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b&q=xx", base::string16() },
    { "http://x/foo#query=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b#query=xx", ASCIIToUTF16("xx") },
    { "http://x/foo?q=b#q=xx", ASCIIToUTF16("b") },
    { "http://x/foo?query=b#q=xx", base::string16() },
  };

  test_util()->ChangeModelToLoadState();
  AddKeywordWithDate("name", "x", "http://x/foo?q={searchTerms}",
                     "http://sugg1", "http://x/foo#query={searchTerms}",
                     "http://icon1", false, "UTF-8;UTF-16");

  for (size_t i = 0; i < base::size(data); ++i) {
    TemplateURLService::URLVisitedDetails details = {
      GURL(data[i].url), false
    };
    model()->UpdateKeywordSearchTermsForURL(details);
    EXPECT_EQ(data[i].term, test_util()->GetAndClearSearchTerm());
  }
}

TEST_F(TemplateURLServiceTest, DontUpdateKeywordSearchForNonReplaceable) {
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

  for (size_t i = 0; i < base::size(data); ++i) {
    TemplateURLService::URLVisitedDetails details = {
      GURL(data[i].url), false
    };
    model()->UpdateKeywordSearchTermsForURL(details);
    ASSERT_EQ(base::string16(), test_util()->GetAndClearSearchTerm());
  }
}

// Historically, {google:baseURL} keywords would change to different
// country-specific Google URLs dynamically. That logic was removed, but test
// that country-specific Google URLs can still be added manually.
TEST_F(TemplateURLServiceWithoutFallbackTest, ManualCountrySpecificGoogleURL) {
  // NOTE: Do not load the prepopulate data, which also has a {google:baseURL}
  // keyword in it and would confuse this test.
  test_util()->ChangeModelToLoadState();

  const TemplateURL* t_url = AddKeywordWithDate(
      "name", "google.com", "{google:baseURL}?q={searchTerms}", "http://sugg1",
      std::string(), "http://icon1", false, "UTF-8;UTF-16");
  ASSERT_EQ(t_url, model()->GetTemplateURLForHost("www.google.com"));
  EXPECT_EQ("www.google.com", t_url->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("google.com"), t_url->keyword());

  // Now add a manual entry for a country-specific Google URL.
  TemplateURL* manual = AddKeywordWithDate(
      "manual", "google.de", "http://www.google.de/search?q={searchTerms}",
      std::string(), std::string(), std::string(), false);

  // Verify that the entries do not conflict.
  ASSERT_EQ(t_url,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("google.com")));
  EXPECT_EQ("www.google.com", t_url->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("google.com"), t_url->keyword());
  ASSERT_EQ(manual,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("google.de")));
  EXPECT_EQ("www.google.de", manual->url_ref().GetHost(search_terms_data()));
  EXPECT_EQ(ASCIIToUTF16("google.de"), manual->keyword());
}

// Make sure TemplateURLService generates a KEYWORD_GENERATED visit for
// KEYWORD visits.
TEST_F(TemplateURLServiceTest, GenerateVisitOnKeyword) {
  test_util()->profile()->CreateBookmarkModel(false);
  ASSERT_TRUE(test_util()->profile()->CreateHistoryService(true, false));
  test_util()->ResetModel(true);

  // Create a keyword.
  TemplateURL* t_url = AddKeywordWithDate(
      "keyword", "keyword", "http://foo.com/foo?query={searchTerms}",
      "http://sugg1", std::string(), "http://icon1", true, "UTF-8;UTF-16",
      Time::Now(), Time::Now(), Time());

  // Add a visit that matches the url of the keyword.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      test_util()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  history->AddPage(GURL(t_url->url_ref().ReplaceSearchTerms(
                       TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("blah")),
                       search_terms_data())),
                   Time::Now(), NULL, 0, GURL(), history::RedirectList(),
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
TEST_F(TemplateURLServiceTest, LoadDeletesUnusedProvider) {
  // Create a preloaded template url. Add it to a loaded model and wait for the
  // saves to finish.
  test_util()->ChangeModelToLoadState();
  model()->Add(CreatePreloadedTemplateURL(true, kPrepopulatedId));
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
  base::RunLoop().RunUntilIdle();

  // Ensure that merging clears this engine.
  test_util()->ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that the database was updated as a result of the
  // merge.
  test_util()->ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) == NULL);
}

// Make sure that load routine doesn't delete prepopulated engines that no
// longer exist in the prepopulate data if it has been modified by the user.
TEST_F(TemplateURLServiceTest, LoadRetainsModifiedProvider) {
  // Create a preloaded template url and add it to a loaded model.
  test_util()->ChangeModelToLoadState();
  TemplateURL* t_url =
      model()->Add(CreatePreloadedTemplateURL(false, kPrepopulatedId));

  // Do the copy after t_url is added so that the id is set.
  std::unique_ptr<TemplateURL> cloned_url =
      std::make_unique<TemplateURL>(t_url->data());
  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Ensure that merging won't clear it if the user has edited it.
  test_util()->ResetModel(true);
  const TemplateURL* url_for_unittest =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
  ASSERT_TRUE(url_for_unittest != NULL);
  AssertEquals(*cloned_url, *url_for_unittest);

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that save/reload retains the item.
  test_util()->ResetModel(true);
  ASSERT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")) != NULL);
}

// Make sure that load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it has been modified by the user.
TEST_F(TemplateURLServiceTest, LoadSavesPrepopulatedDefaultSearchProvider) {
  test_util()->VerifyLoad();
  // Verify that the default search provider is set to something.
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != NULL);
  std::unique_ptr<TemplateURL> cloned_url(
      new TemplateURL(default_search->data()));

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model and check that the default search provider
  // was properly saved.
  test_util()->ResetModel(true);
  default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search != NULL);
  AssertEquals(*cloned_url, *default_search);
}

// Make sure that the load routine doesn't delete
// prepopulated engines that no longer exist in the prepopulate data if
// it is the default search provider.
TEST_F(TemplateURLServiceTest, LoadRetainsDefaultProvider) {
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

  ASSERT_EQ(t_url, model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest")));
  ASSERT_EQ(t_url, model()->GetDefaultSearchProvider());
  base::RunLoop().RunUntilIdle();

  // Ensure that merging won't clear the prepopulated template url
  // which is no longer present if it's the default engine.
  test_util()->ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }

  // Wait for any saves to finish.
  base::RunLoop().RunUntilIdle();

  // Reload the model to verify that the update was saved.
  test_util()->ResetModel(true);
  {
    const TemplateURL* keyword_url =
        model()->GetTemplateURLForKeyword(ASCIIToUTF16("unittest"));
    ASSERT_TRUE(keyword_url != NULL);
    AssertEquals(*cloned_url, *keyword_url);
    ASSERT_EQ(keyword_url, model()->GetDefaultSearchProvider());
  }
}

// Make sure that the load routine sets a default search provider if it was
// missing and not managed.
TEST_F(TemplateURLServiceTest, LoadEnsuresDefaultSearchProviderExists) {
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
      model()->GetTemplateURLForKeyword(default_search->keyword()),
      ASCIIToUTF16("test"), ASCIIToUTF16("test"), "http://example.com/");
  base::RunLoop().RunUntilIdle();

  // Reset the model and load it. There should be a usable default search
  // provider.
  test_util()->ResetModel(true);

  ASSERT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->GetDefaultSearchProvider()->SupportsReplacement(
      search_terms_data()));
}

// Simulates failing to load the webdb and makes sure the default search
// provider is valid.
TEST_F(TemplateURLServiceTest, FailedInit) {
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
TEST_F(TemplateURLServiceTest, TestManagedDefaultSearch) {
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
  managed2.SetShortName(ASCIIToUTF16("test2"));
  managed2.SetKeyword(ASCIIToUTF16("other.com"));
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
  EXPECT_TRUE(NULL == model()->GetDefaultSearchProvider());
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
  TemplateURL* new_default =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("key1"));
  ASSERT_FALSE(new_default == NULL);
  model()->SetUserSelectedDefaultSearchProvider(new_default);
  EXPECT_EQ(new_default, model()->GetDefaultSearchProvider());

  // Now reset the model again but load it after setting the preferences.
  test_util()->ResetModel(false);
  SetManagedDefaultSearchPreferences(*managed, false, test_util()->profile());
  test_util()->VerifyLoad();
  EXPECT_TRUE(model()->is_default_search_managed());
  EXPECT_TRUE(model()->GetDefaultSearchProvider() == NULL);
}

// Test that if we load a TemplateURL with an empty GUID, the load process
// assigns it a newly generated GUID.
TEST_F(TemplateURLServiceTest, PatchEmptySyncGUID) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("google"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
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
  const TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_FALSE(loaded_url == NULL);
  ASSERT_FALSE(loaded_url->sync_guid().empty());
}

// Test that if we load a TemplateURL with duplicate input encodings, the load
// process de-dupes them.
TEST_F(TemplateURLServiceTest, DuplicateInputEncodings) {
  // Add a new TemplateURL.
  test_util()->VerifyLoad();
  const size_t initial_count = model()->GetTemplateURLs().size();

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("google"));
  data.SetKeyword(ASCIIToUTF16("keyword"));
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
  const TemplateURL* loaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_TRUE(loaded_url != NULL);
  EXPECT_EQ(8U, loaded_url->input_encodings().size());

  // Reload the model to verify it was actually saved to the database and the
  // duplicate encodings were removed.
  test_util()->ResetModel(true);
  ASSERT_EQ(initial_count + 1, model()->GetTemplateURLs().size());
  loaded_url = model()->GetTemplateURLForKeyword(ASCIIToUTF16("keyword"));
  ASSERT_FALSE(loaded_url == NULL);
  EXPECT_EQ(4U, loaded_url->input_encodings().size());
}

TEST_F(TemplateURLServiceTest, DefaultExtensionEngine) {
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

TEST_F(TemplateURLServiceTest, SetDefaultExtensionEngineAndRemoveUserDSE) {
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
  std::string dse_guid =
      prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID);
  EXPECT_EQ(user_dse->sync_guid(), dse_guid);

  model()->Remove(user_dse);
  EXPECT_EQ(ext_dse_ptr, model()->GetDefaultSearchProvider());

  test_util()->RemoveExtensionControlledTURL("extension_id");
  // The DSE is set to the fallback search engine.
  EXPECT_TRUE(model()->GetDefaultSearchProvider());
  EXPECT_NE(dse_guid,
            prefs->GetString(prefs::kSyncedDefaultSearchProviderGUID));
}

TEST_F(TemplateURLServiceTest, DefaultExtensionEnginePersist) {
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
  EXPECT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("extension2_keyword")));
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());

  // Non-default extension engines are not persisted across restarts.
  EXPECT_FALSE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("extension1_keyword")));
}

TEST_F(TemplateURLServiceTest, DefaultExtensionEnginePersistsBeforeLoad) {
  // Chrome will load the extension system before the TemplateURLService, so
  // extensions controlling the default search engine may be registered before
  // the service has loaded.
  const TemplateURL* ext_dse =
      AddExtensionSearchEngine("extension1_keyword", "extension1_id", true);
  auto cloned_ext_dse = std::make_unique<TemplateURL>(ext_dse->data());

  // Default search engine from extension must be persisted between browser
  // restarts, and should be available before the TemplateURLService is loaded.
  EXPECT_TRUE(
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("extension1_keyword")));
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());

  // Check extension DSE is the same after service load.
  test_util()->VerifyLoad();
  ExpectSimilar(cloned_ext_dse.get(), model()->GetDefaultSearchProvider());
}

// Checks that correct priority is applied when resolving conflicts between the
// omnibox extension, search engine extension and user search engines with same
// keyword.
TEST_F(TemplateURLServiceTest, CheckEnginesWithSameKeywords) {
  test_util()->VerifyLoad();
  // TemplateURLData used for user engines.
  std::unique_ptr<TemplateURLData> turl_data =
      GenerateDummyTemplateURLData("common_keyword");
  turl_data->safe_for_autoreplace = false;

  // Add non replaceable user engine.
  const TemplateURL* user1 =
      model()->Add(std::make_unique<TemplateURL>(*turl_data));

  // Add default extension engine with same keyword as user engine.
  const TemplateURL* extension = AddExtensionSearchEngine(
      "common_keyword", "extension_id", true, Time::FromDoubleT(2));

  // Add another non replaceable user engine with same keyword as extension.
  const TemplateURL* user2 =
      model()->Add(std::make_unique<TemplateURL>(*turl_data));

  // Check extension DSE is set as default and its keyword is not changed.
  const TemplateURL* current_dse = model()->GetDefaultSearchProvider();
  EXPECT_EQ(extension, current_dse);
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), current_dse->keyword());

  // Register omnibox keyword with same keyword as extension.
  // Use |install_time| value less than in AddExtensionSearchEngine call above
  // to check that omnibox api keyword is ranked higher even if installed
  // earlier.
  model()->RegisterOmniboxKeyword("omnibox_api_extension_id", "extension_name",
                                  "common_keyword", "http://test3",
                                  Time::FromDoubleT(1));
  TemplateURL* omnibox_api = model()->FindTemplateURLForExtension(
      "omnibox_api_extension_id", TemplateURL::OMNIBOX_API_EXTENSION);

  // Expect that first non replaceable user engine keyword is changed because of
  // conflict. Second user engine will keep its keyword.
  EXPECT_NE(ASCIIToUTF16("common_keyword"), user1->keyword());
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), user2->keyword());

  // Check that extensions kept their keywords.
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), extension->keyword());
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), omnibox_api->keyword());

  // Omnibox api is accessible by keyword as most relevant.
  EXPECT_EQ(omnibox_api,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));
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
  EXPECT_EQ(extension,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));
  // Remove extension engine.
  model()->RemoveExtensionControlledTURL(
      "extension_id", TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  EXPECT_NE(extension, model()->GetDefaultSearchProvider());
  // Now latest user engine is returned for keyword.
  EXPECT_EQ(user2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));
}

TEST_F(TemplateURLServiceTest, ConflictingReplaceableEnginesShouldOverwrite) {
  test_util()->VerifyLoad();
  // Add 2 replaceable user engine with different keywords.
  TemplateURL* user1 =
      AddKeywordWithDate("user_engine1", "user1", "http://test1", std::string(),
                         std::string(), std::string(), true);
  AddKeywordWithDate("user_engine2", "user2", "http://test2", std::string(),
                     std::string(), std::string(), true);
  // Update first engine to conflict with second by keyword. This should
  // overwrite the second engine.
  model()->ResetTemplateURL(user1, ASCIIToUTF16("title"), ASCIIToUTF16("user2"),
                            "http://test_search.com");
  // Check that first engine can now be found by new keyword.
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(ASCIIToUTF16("user2")));
  // Update to return first engine original keyword.
  model()->ResetTemplateURL(user1, ASCIIToUTF16("title"), ASCIIToUTF16("user1"),
                            "http://test_search.com");
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(ASCIIToUTF16("user1")));
  // Check that no engine is now found by keyword user2.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(ASCIIToUTF16("user2")));
}

TEST_F(TemplateURLServiceTest, CheckNonreplaceableEnginesKeywordsConflicts) {
  test_util()->VerifyLoad();

  const base::string16 kCommonKeyword = ASCIIToUTF16("common_keyword");
  // 1. Add non replaceable user engine.
  const TemplateURL* user1 =
      AddKeywordWithDate("nonreplaceable", "common_keyword", "http://test1",
                         std::string(), std::string(), std::string(), false);

  // Check it is accessible by keyword and host.
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));

  // 2. Add another non replaceable user engine with same keyword but different
  // search url.
  const TemplateURL* user2 =
      AddKeywordWithDate("nonreplaceable2", "common_keyword", "http://test2",
                         std::string(), std::string(), std::string(), false);
  // Existing engine conflicting keyword must be changed to value generated from
  // its search url. Both engines must be acessible by keyword and host.
  EXPECT_EQ(ASCIIToUTF16("test1"), user1->keyword());
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(ASCIIToUTF16("test1")));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));
  EXPECT_EQ(user2, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user2, model()->GetTemplateURLForHost("test2"));

  // 3. Add another non replaceable user engine with same keyword and same url
  // as in engine that already exists in model.
  const TemplateURL* user3 =
      AddKeywordWithDate("nonreplaceable3", "common_keyword", "http://test2",
                         std::string(), std::string(), std::string(), false);
  // Previous conflicting engine will get keyword generated from url. Both
  // engines must be acessible.
  EXPECT_EQ(ASCIIToUTF16("test2"), user2->keyword());
  EXPECT_EQ(user2, model()->GetTemplateURLForKeyword(ASCIIToUTF16("test2")));
  EXPECT_EQ(kCommonKeyword, user3->keyword());
  EXPECT_EQ(user3, model()->GetTemplateURLForKeyword(kCommonKeyword));
  // Expect user2 or user3 returned for host "test2" because now both user2 and
  // user3 engines correspond to host "test2" and GetTemplateURLForHost returns
  // one of them.
  const TemplateURL* url_for_test2 = model()->GetTemplateURLForHost("test2");
  EXPECT_TRUE(user2 == url_for_test2 || user3 == url_for_test2);

  // 4. Add another non replaceable user engine with common keyword.
  const TemplateURL* user4 =
      AddKeywordWithDate("nonreplaceable4", "common_keyword", "http://test3",
                         std::string(), std::string(), std::string(), false);
  // Existing conflicting engine user3 will get keyword generated by appending
  // '_' to its keyword because it can not get keyword autogenerated from search
  // url - "test2" keyword is already in use by user2 engine. Both engines must
  // be acessible.
  EXPECT_EQ(ASCIIToUTF16("common_keyword_"), user3->keyword());
  EXPECT_EQ(user3,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword_")));
  EXPECT_EQ(kCommonKeyword, user4->keyword());
  EXPECT_EQ(user4, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user4, model()->GetTemplateURLForHost("test3"));

  // Check conflict between search engines with html tags embedded in URL host.
  // URLs with embedded HTML canonicalize to contain uppercase characters in the
  // hostname. Ensure these URLs are still handled correctly for conflict
  // resolution.
  const TemplateURL* user5 =
      AddKeywordWithDate("nonreplaceable5", "embedded.%3chtml%3eweb",
                         "http://embedded.<html>web/?q={searchTerms}",
                         std::string(), std::string(), std::string(), false);
  EXPECT_EQ(ASCIIToUTF16("embedded.%3chtml%3eweb"), user5->keyword());
  EXPECT_EQ(user5, model()->GetTemplateURLForKeyword(
                       ASCIIToUTF16("embedded.%3chtml%3eweb")));
  const TemplateURL* user6 =
      AddKeywordWithDate("nonreplaceable6", "embedded.%3chtml%3eweb",
                         "http://embedded.<html>web/?q={searchTerms}",
                         std::string(), std::string(), std::string(), false);
  EXPECT_EQ(ASCIIToUTF16("embedded.%3chtml%3eweb"), user6->keyword());
  EXPECT_EQ(user6, model()->GetTemplateURLForKeyword(
                       ASCIIToUTF16("embedded.%3chtml%3eweb")));
  // Expect existing engine changed its keyword.
  EXPECT_EQ(ASCIIToUTF16("embedded.%3chtml%3eweb_"), user5->keyword());
  EXPECT_EQ(user5, model()->GetTemplateURLForKeyword(
                       ASCIIToUTF16("embedded.%3chtml%3eweb_")));
}

TEST_F(TemplateURLServiceTest, CheckReplaceableEnginesKeywordsConflicts) {
  test_util()->VerifyLoad();

  const base::string16 kCommonKeyword = ASCIIToUTF16("common_keyword");
  // 1. Add non replaceable user engine with common keyword.
  const TemplateURL* user1 =
      AddKeywordWithDate("nonreplaceable", "common_keyword", "http://test1",
                         std::string(), std::string(), std::string(), false);
  // Check it is accessible by keyword and host.
  EXPECT_EQ(user1, model()->GetTemplateURLForKeyword(kCommonKeyword));
  EXPECT_EQ(user1, model()->GetTemplateURLForHost("test1"));

  // 2. Try to add replaceable user engine with conflicting keyword. Addition
  // must fail.
  const TemplateURL* user2 =
      AddKeywordWithDate("replaceable", "common_keyword", "http://test2",
                         std::string(), std::string(), std::string(), true);
  EXPECT_FALSE(user2);
  EXPECT_FALSE(model()->GetTemplateURLForHost("test2"));

  const base::string16 kCommonKeyword2 = ASCIIToUTF16("common_keyword2");
  // 3. Add replaceable user engine with non conflicting keyword.
  const TemplateURL* user3 =
      AddKeywordWithDate("replaceable2", "common_keyword2", "http://test3",
                         std::string(), std::string(), std::string(), true);
  // New engine must exist and be accessible.
  EXPECT_EQ(user3, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user3, model()->GetTemplateURLForHost("test3"));

  // 4. Add another replaceable user engine with conflicting keyword.
  const TemplateURL* user4 =
      AddKeywordWithDate("replaceable3", "common_keyword2", "http://test4",
                         std::string(), std::string(), std::string(), true);
  // New engine must exist and be accessible. Old replaceable engine must be
  // evicted from model.
  EXPECT_FALSE(model()->GetTemplateURLForHost("test3"));
  EXPECT_EQ(user4, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user4, model()->GetTemplateURLForHost("test4"));

  // 5. Add non replaceable user engine with common_keyword2. Must evict
  // conflicting replaceable engine.
  const TemplateURL* user5 =
      AddKeywordWithDate("nonreplaceable5", "common_keyword2", "http://test5",
                         std::string(), std::string(), std::string(), false);
  EXPECT_FALSE(model()->GetTemplateURLForHost("test4"));
  EXPECT_EQ(user5, model()->GetTemplateURLForKeyword(kCommonKeyword2));
  EXPECT_EQ(user5, model()->GetTemplateURLForHost("test5"));
}

// Check that two extensions with the same engine are handled correctly.
TEST_F(TemplateURLServiceTest, ExtensionsWithSameKeywords) {
  test_util()->VerifyLoad();
  // Add non default extension engine.
  const TemplateURL* extension1 = AddExtensionSearchEngine(
      "common_keyword", "extension_id1", false, Time::FromDoubleT(1));

  // Check that GetTemplateURLForKeyword returns last installed extension.
  EXPECT_EQ(extension1,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));

  // Add default extension engine with the same keyword.
  const TemplateURL* extension2 = AddExtensionSearchEngine(
      "common_keyword", "extension_id2", true, Time::FromDoubleT(2));
  // Check that GetTemplateURLForKeyword now returns extension2 because it was
  // installed later.
  EXPECT_EQ(extension2,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));

  // Add another non default extension with same keyword. This action must not
  // change any keyword due to conflict.
  const TemplateURL* extension3 = AddExtensionSearchEngine(
      "common_keyword", "extension_id3", false, Time::FromDoubleT(3));
  // Check that extension2 is set as default.
  EXPECT_EQ(extension2, model()->GetDefaultSearchProvider());

  // Check that GetTemplateURLForKeyword returns last installed extension.
  EXPECT_EQ(extension3,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("common_keyword")));
  // Check that all keywords for extensions are left unchanged.
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), extension1->keyword());
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), extension2->keyword());
  EXPECT_EQ(ASCIIToUTF16("common_keyword"), extension3->keyword());
}

TEST_F(TemplateURLServiceTest, ExtensionEngineVsPolicy) {
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
  EXPECT_EQ(ext_dse_ptr,
            model()->GetTemplateURLForKeyword(ASCIIToUTF16("ext1")));
  EXPECT_TRUE(model()->is_default_search_managed());
  actual_managed_default = model()->GetDefaultSearchProvider();
  ExpectSimilar(expected_managed_default.get(), actual_managed_default);
}

TEST_F(TemplateURLServiceTest, LastVisitedTimeUpdate) {
  test_util()->VerifyLoad();
  TemplateURL* original_url =
      AddKeywordWithDate("name1", "key1", "http://foo1", "http://suggest1",
                         std::string(), "http://icon1", true, "UTF-8;UTF-16");
  const Time original_last_visited = original_url->last_visited();
  model()->UpdateTemplateURLVisitTime(original_url);
  TemplateURL* modified_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("key1"));
  const Time modified_last_visited = modified_url->last_visited();
  EXPECT_NE(original_last_visited, modified_last_visited);
  test_util()->ResetModel(true);
  TemplateURL* reloaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("key1"));
  AssertTimesEqual(modified_last_visited, reloaded_url->last_visited());
}

TEST_F(TemplateURLServiceTest, LastModifiedTimeUpdate) {
  test_util()->VerifyLoad();
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("test_engine"));
  data.SetKeyword(ASCIIToUTF16("engine_keyword"));
  data.SetURL("http://test_engine");
  data.safe_for_autoreplace = true;
  TemplateURL* original_url = model()->Add(std::make_unique<TemplateURL>(data));
  const Time original_last_modified = original_url->last_modified();
  model()->ResetTemplateURL(original_url, ASCIIToUTF16("test_engine2"),
                            ASCIIToUTF16("engine_keyword"),
                            "http://test_engine");
  TemplateURL* update_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("engine_keyword"));
  const Time update_last_modified = update_url->last_modified();
  model()->SetUserSelectedDefaultSearchProvider(update_url);
  TemplateURL* reloaded_url =
      model()->GetTemplateURLForKeyword(ASCIIToUTF16("engine_keyword"));
  const Time reloaded_last_modified = reloaded_url->last_modified();
  EXPECT_NE(original_last_modified, reloaded_last_modified);
  EXPECT_EQ(update_last_modified, reloaded_last_modified);
}

// Tests checks that Search.DefaultSearchChangeOrigin histogram is correctly
// emitted when TemplateURLService is not yet loaded.
TEST_F(TemplateURLServiceTest, ChangeDefaultEngineBeforeLoad) {
  TemplateURL* search_engine1 = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("keyword1")));
  DCHECK(search_engine1);
  TemplateURL* search_engine2 = model()->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("keyword2")));
  DCHECK(search_engine2);

  base::HistogramTester histogram_tester;
  model()->SetUserSelectedDefaultSearchProvider(search_engine1);
  histogram_tester.ExpectTotalCount("Search.DefaultSearchChangeOrigin", 1);
  model()->SetUserSelectedDefaultSearchProvider(search_engine1);
  histogram_tester.ExpectTotalCount("Search.DefaultSearchChangeOrigin", 1);
  model()->SetUserSelectedDefaultSearchProvider(search_engine2);
  histogram_tester.ExpectTotalCount("Search.DefaultSearchChangeOrigin", 2);
}
