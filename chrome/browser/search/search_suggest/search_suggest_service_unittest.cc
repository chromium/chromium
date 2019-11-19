// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::StrictMock;

class FakeSearchSuggestLoader : public SearchSuggestLoader {
 public:
  void Load(const std::string&, SearchSuggestionsCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  GURL GetLoadURLForTesting() const override { return GURL(); }

  size_t GetCallbackCount() const { return callbacks_.size(); }

  void RespondToAllCallbacks(Status status,
                             const base::Optional<SearchSuggestData>& data) {
    for (SearchSuggestionsCallback& callback : callbacks_) {
      std::move(callback).Run(status, data);
    }
    callbacks_.clear();
  }

 private:
  std::vector<SearchSuggestionsCallback> callbacks_;
};

class SearchSuggestServiceTest : public BrowserWithTestWindowTest {
 public:
  SearchSuggestServiceTest() {}
  ~SearchSuggestServiceTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);

    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>(
        &test_url_loader_factory_);
    auto loader = std::make_unique<FakeSearchSuggestLoader>();
    loader_ = loader.get();
    service_ = std::make_unique<SearchSuggestService>(
        profile(), identity_env_->identity_manager(), std::move(loader));

    identity_env_->MakePrimaryAccountAvailable("example@gmail.com");
    identity_env_->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  TestingProfile* CreateProfile() override {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    return profile;
  }

  FakeSearchSuggestLoader* loader() { return loader_; }
  SearchSuggestService* service() { return service_.get(); }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile()->GetTestingPrefService();
  }

  void SignIn() {
    AccountInfo account_info =
        identity_env_->MakeAccountAvailable("test@email.com");
    identity_env_->SetCookieAccounts({{account_info.email, account_info.gaia}});
  }

  void SignOut() { identity_env_->SetCookieAccounts({}); }

  void SetUserSelectedDefaultSearchProvider(const std::string& base_url) {
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(base_url));
    data.SetKeyword(base::UTF8ToUTF16(base_url));
    data.SetURL(base_url + "url?bar={searchTerms}");
    data.new_tab_url = base_url + "newtab";
    data.alternate_urls.push_back(base_url + "alt#quux={searchTerms}");

    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  // Returns a default data object for testing, initializes the impression
  // parameters to values where they won't prevent fetching.
  SearchSuggestData TestSearchSuggestData() {
    SearchSuggestData data;
    data.suggestions_html = "<div></div>";
    data.impression_cap_expire_time_ms = 60000;
    data.request_freeze_time_ms = 60000;
    data.max_impressions = 10;
    return data;
  }

  void RunFor(base::TimeDelta time_period) {
    base::RunLoop run_loop;
    base::CancelableCallback<void()> callback(run_loop.QuitWhenIdleClosure());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, callback.callback(), time_period);
    run_loop.Run();
    callback.Cancel();
  }

 private:
  TemplateURLService* template_url_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;

  // Owned by the service.
  FakeSearchSuggestLoader* loader_;

  std::unique_ptr<SearchSuggestService> service_;
};

TEST_F(SearchSuggestServiceTest, NoRefreshOnSignedOutRequest) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());

  // Request a refresh. That should do nothing as no user is signed-in.
  service()->Refresh();
  EXPECT_EQ(0u, loader()->GetCallbackCount());
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, RefreshesOnSignedInRequest) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());

  // Request another refresh.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // For now, the old data should still be there.
  EXPECT_EQ(data, service()->search_suggest_data());

  // Fulfill the second request.
  SearchSuggestData other_data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, other_data);
  EXPECT_EQ(other_data, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, KeepsCacheOnTransientError) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  ASSERT_EQ(data, service()->search_suggest_data());

  // Request a refresh and respond with a transient error.
  service()->Refresh();
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::TRANSIENT_ERROR,
                                  base::nullopt);
  // Cached data should still be there.
  EXPECT_EQ(data, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, ClearsCacheOnFatalError) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  ASSERT_EQ(data, service()->search_suggest_data());

  // Request a refresh and respond with a fatal error.
  service()->Refresh();
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::FATAL_ERROR,
                                  base::nullopt);
  // Cached data should be gone now.
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, ResetsOnSignOut) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  ASSERT_EQ(data, service()->search_suggest_data());

  // Sign out. This should clear the cached data and notify the observer.
  SignOut();
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, BlocklistSuggestionUpdatesBlocklistString) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash1[5] = {'a', 'b', 'c', 'd', '\0'};
  uint8_t hash2[5] = {'e', 'f', 'g', 'h', '\0'};
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash1);
  service()->BlocklistSearchSuggestion(2, 5678);
  service()->BlocklistSearchSuggestionWithHash(1, 1234, hash2);
  service()->BlocklistSearchSuggestionWithHash(2, 1234, hash1);
  service()->BlocklistSearchSuggestion(4, 1234);
  service()->BlocklistSearchSuggestionWithHash(2, 1234, hash2);
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash2);

  std::string expected =
      "0_1234:abcd,efgh;1_1234:efgh;2_1234:abcd,efgh;2_5678;4_1234";

  ASSERT_EQ(expected, service()->GetBlocklistAsString());
}

TEST_F(SearchSuggestServiceTest, BlocklistUnchangedOnInvalidHash) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash1[5] = {'a', 'b', '?', 'd', '\0'};
  uint8_t hash2[5] = {'a', '_', 'b', 'm', '\0'};
  uint8_t hash3[5] = {'A', 'B', 'C', 'D', '\0'};
  std::string expected = std::string();

  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash1);
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash2);
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash3);
  ASSERT_EQ(expected, service()->GetBlocklistAsString());
}

TEST_F(SearchSuggestServiceTest, ShortHashDoesNotUpdateBlackist) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash1[4] = {'a', 'b', 'c', '\0'};
  uint8_t hash2[5] = {'d', 'e', '\0', 'f', '\0'};
  std::string expected = std::string();

  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash1);
  service()->BlocklistSearchSuggestionWithHash(1, 5678, hash2);
  ASSERT_EQ(expected, service()->GetBlocklistAsString());
}

TEST_F(SearchSuggestServiceTest, LongHashIsTruncated) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash1[6] = {'a', 'b', 'c', 'd', 'e', '\0'};
  uint8_t hash2[7] = {'d', 'e', 'f', 'g', '\0', 'h', 'i'};
  std::string expected = "0_1234:abcd;1_5678:defg";

  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash1);
  service()->BlocklistSearchSuggestionWithHash(1, 5678, hash2);
  ASSERT_EQ(expected, service()->GetBlocklistAsString());
}

TEST_F(SearchSuggestServiceTest,
       BlocklistSuggestionOverridesBlackistSuggestionWithHash) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash[5] = {'a', 'b', 'c', 'd', '\0'};
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash);
  ASSERT_EQ("0_1234:abcd", service()->GetBlocklistAsString());

  service()->BlocklistSearchSuggestion(0, 1234);
  ASSERT_EQ("0_1234", service()->GetBlocklistAsString());
}

TEST_F(SearchSuggestServiceTest, BlocklistClearsCachedDataAndIssuesRequest) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());

  // Select a suggestion to blocklist.
  uint8_t hash[5] = {'a', 'b', 'c', 'd', '\0'};
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash);
  ASSERT_EQ("0_1234:abcd", service()->GetBlocklistAsString());
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill the second request.
  SearchSuggestData other_data;
  other_data.suggestions_html = "<div>Different!</div>";
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, other_data);
  EXPECT_EQ(other_data, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest,
       SuggestionSelectedClearsCachedDataAndIssuesRequest) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());

  // Select a suggestion to blocklist.
  uint8_t hash[5] = {'a', 'b', 'c', 'd', '\0'};
  service()->SearchSuggestionSelected(0, 1234, hash);

  // The local blocklist should not be updated.
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill the second request.
  SearchSuggestData other_data;
  other_data.suggestions_html = "<div>Different!</div>";
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, other_data);
  EXPECT_EQ(other_data, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, OptOutPreventsRequests) {
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  service()->OptOutOfSearchSuggestions();

  // Request a refresh. That should do nothing as the user opted-out.
  service()->Refresh();
  EXPECT_EQ(0u, loader()->GetCallbackCount());
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, SuggestionAPIsDoNothingWithNonGoogleDSP) {
  SetUserSelectedDefaultSearchProvider("https://search.com/");
  ASSERT_EQ(std::string(), service()->GetBlocklistAsString());

  uint8_t hash[5] = {'a', 'b', 'c', 'd', '\0'};
  service()->BlocklistSearchSuggestionWithHash(0, 1234, hash);
  EXPECT_EQ(std::string(), service()->GetBlocklistAsString());

  service()->BlocklistSearchSuggestion(1, 2345);
  EXPECT_EQ(std::string(), service()->GetBlocklistAsString());

  service()->OptOutOfSearchSuggestions();
  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kNtpSearchSuggestionsOptOut));

  service()->SearchSuggestionSelected(0, 1234, hash);
  EXPECT_EQ(0u, loader()->GetCallbackCount());
}

TEST_F(SearchSuggestServiceTest, UpdateImpressionCapParameters) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());

  // Request another refresh.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // For now, the old data should still be there.
  EXPECT_EQ(data, service()->search_suggest_data());

  // Fulfill the second request.
  SearchSuggestData other_data;
  other_data.suggestions_html = "<div>different</div>";
  other_data.impression_cap_expire_time_ms = 1234;
  other_data.request_freeze_time_ms = 4321;
  other_data.max_impressions = 456;
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, other_data);
  EXPECT_EQ(other_data, service()->search_suggest_data());

  // Ensure the pref parses successfully.
  const base::DictionaryValue* dict =
      pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  int impression_cap_expire_time_ms = 0;
  ASSERT_TRUE(dict->GetInteger("impression_cap_expire_time_ms",
                               &impression_cap_expire_time_ms));
  int request_freeze_time_ms = 0;
  ASSERT_TRUE(
      dict->GetInteger("request_freeze_time_ms", &request_freeze_time_ms));
  int max_impressions = 0;
  ASSERT_TRUE(dict->GetInteger("max_impressions", &max_impressions));

  EXPECT_EQ(1234, impression_cap_expire_time_ms);
  EXPECT_EQ(4321, request_freeze_time_ms);
  EXPECT_EQ(456, max_impressions);
}

TEST_F(SearchSuggestServiceTest, DontRequestWhenImpressionCapped) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  const base::DictionaryValue* dict =
      pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  int impressions_count = 0;
  ASSERT_TRUE(dict->GetInteger("impressions_count", &impressions_count));
  EXPECT_EQ(0, impressions_count);

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  data.max_impressions = 2;
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());
  service()->SuggestionsDisplayed();

  dict = pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  ASSERT_TRUE(dict->GetInteger("impressions_count", &impressions_count));
  EXPECT_EQ(1, impressions_count);

  // Request another refresh.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());
  data.suggestions_html = "<div>Different!</div>";
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());
  service()->SuggestionsDisplayed();

  dict = pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  ASSERT_TRUE(dict->GetInteger("impressions_count", &impressions_count));
  EXPECT_EQ(2, impressions_count);

  // Should not make another request as we've reached the cap
  service()->Refresh();
  EXPECT_EQ(0u, loader()->GetCallbackCount());
}

TEST_F(SearchSuggestServiceTest, ImpressionCountResetsAfterTimeout) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  const base::DictionaryValue* dict =
      pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  int impressions_count = 0;
  ASSERT_TRUE(dict->GetInteger("impressions_count", &impressions_count));
  EXPECT_EQ(0, impressions_count);

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  data.max_impressions = 1;
  data.impression_cap_expire_time_ms = 1000;
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());
  service()->SuggestionsDisplayed();

  dict = pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  ASSERT_TRUE(dict->GetInteger("impressions_count", &impressions_count));
  EXPECT_EQ(1, impressions_count);

  // The impression cap has been reached.
  service()->Refresh();
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());

  RunFor(base::TimeDelta::FromMilliseconds(1000));

  // The impression cap timeout has expired.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());
}

TEST_F(SearchSuggestServiceTest, RequestsFreezeOnEmptyResponse) {
  ASSERT_EQ(base::nullopt, service()->search_suggest_data());
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  // Fulfill it.
  SearchSuggestData data = TestSearchSuggestData();
  data.request_freeze_time_ms = 1000;
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());

  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITHOUT_SUGGESTIONS, data);

  const base::DictionaryValue* dict =
      pref_service()->GetDictionary(prefs::kNtpSearchSuggestionsImpressions);
  bool is_request_frozen;
  ASSERT_TRUE(dict->GetBoolean("is_request_frozen", &is_request_frozen));
  EXPECT_TRUE(is_request_frozen);

  // No request should be made since they are frozen.
  service()->Refresh();
  EXPECT_EQ(base::nullopt, service()->search_suggest_data());

  RunFor(base::TimeDelta::FromMilliseconds(1000));

  // The freeze timeout has expired.
  service()->Refresh();
  EXPECT_EQ(1u, loader()->GetCallbackCount());
  loader()->RespondToAllCallbacks(
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, data);
  EXPECT_EQ(data, service()->search_suggest_data());
}
