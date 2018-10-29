// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/search_provider.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using base::ASCIIToUTF16;

namespace {

// Returns the first match in |matches| with |allowed_to_be_default_match|
// set to true.
ACMatches::const_iterator FindDefaultMatch(const ACMatches& matches) {
  auto it = matches.begin();
  while ((it != matches.end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

class SuggestionDeletionHandler;
class SearchProviderForTest : public SearchProvider {
 public:
  SearchProviderForTest(AutocompleteProviderClient* client,
                        AutocompleteProviderListener* listener,
                        Profile* profile);
  bool is_success() { return is_success_; }

 protected:
  ~SearchProviderForTest() override;

 private:
  void RecordDeletionResult(bool success) override;
  bool is_success_;
  DISALLOW_COPY_AND_ASSIGN(SearchProviderForTest);
};

SearchProviderForTest::SearchProviderForTest(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    Profile* profile)
    : SearchProvider(client, listener), is_success_(false) {}

SearchProviderForTest::~SearchProviderForTest() {
}

void SearchProviderForTest::RecordDeletionResult(bool success) {
  is_success_ = success;
}

class TestAutocompleteProviderClient : public ChromeAutocompleteProviderClient {
 public:
  TestAutocompleteProviderClient(Profile* profile,
                                 network::TestURLLoaderFactory* loader_factory)
      : ChromeAutocompleteProviderClient(profile),
        is_personalized_url_data_collection_active_(true),
        shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                loader_factory)) {}
  ~TestAutocompleteProviderClient() override {}

  bool IsPersonalizedUrlDataCollectionActive() const override {
    return is_personalized_url_data_collection_active_;
  }

  void set_is_personalized_url_data_collection_active(
      bool is_personalized_url_data_collection_active) {
    is_personalized_url_data_collection_active_ =
        is_personalized_url_data_collection_active;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return shared_factory_;
  }

 private:
  bool is_personalized_url_data_collection_active_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
};

}  // namespace

// SearchProviderTest ---------------------------------------------------------

// The following environment is configured for these tests:
// . The TemplateURL default_t_url_ is set as the default provider.
// . The TemplateURL keyword_t_url_ is added to the TemplateURLService. This
//   TemplateURL has a valid suggest and search URL.
// . The URL created by using the search term term1_ with default_t_url_ is
//   added to history.
// . The URL created by using the search term keyword_term_ with keyword_t_url_
//   is added to history.
// . test_url_loader_factory_ is set as the URLLoaderFactory.
class SearchProviderTest : public testing::Test,
                           public AutocompleteProviderListener {
 public:
  struct ResultInfo {
    ResultInfo() : result_type(AutocompleteMatchType::NUM_TYPES),
                   allowed_to_be_default_match(false) {
    }
    ResultInfo(GURL gurl,
               AutocompleteMatch::Type result_type,
               bool allowed_to_be_default_match,
               base::string16 fill_into_edit)
      : gurl(gurl),
        result_type(result_type),
        allowed_to_be_default_match(allowed_to_be_default_match),
        fill_into_edit(fill_into_edit) {
    }

    const GURL gurl;
    const AutocompleteMatch::Type result_type;
    const bool allowed_to_be_default_match;
    const base::string16 fill_into_edit;
  };

  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const ResultInfo output[3];
  };

  struct ExpectedMatch {
    std::string contents;
    bool allowed_to_be_default_match;
  };

  SearchProviderTest()
      : default_t_url_(nullptr),
        term1_(ASCIIToUTF16("term1")),
        keyword_t_url_(nullptr),
        keyword_term_(ASCIIToUTF16("keyword")),
        run_loop_(nullptr) {
    ResetFieldTrialList();
  }

  // See description above class for what this registers.
  void SetUp() override;
  void TearDown() override;

  void RunTest(TestData* cases, int num_cases, bool prefer_keyword);

 protected:
  // Needed for AutocompleteFieldTrial::ActivateStaticTrials();
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  // Default values used for testing.
  static const char kNotApplicable[];
  static const ExpectedMatch kEmptyExpectedMatch;

  // Adds a search for |term|, using the engine |t_url| to the history, and
  // returns the URL for that search.
  GURL AddSearchToHistory(TemplateURL* t_url,
                          base::string16 term,
                          int visit_count);

  // Looks for a match in |provider_| with |contents| equal to |contents|.
  // Sets |match| to it if found.  Returns whether |match| was set.
  bool FindMatchWithContents(const base::string16& contents,
                             AutocompleteMatch* match);

  // Looks for a match in |provider_| with destination |url|.  Sets |match| to
  // it if found.  Returns whether |match| was set.
  bool FindMatchWithDestination(const GURL& url, AutocompleteMatch* match);

  // AutocompleteProviderListener:
  // If we're waiting for the provider to finish, this exits the message loop.
  void OnProviderUpdate(bool updated_matches) override;

  // Runs a nested run loop until provider_ is done. The message loop is
  // exited by way of OnProviderUpdate.
  void RunTillProviderDone();

  // Invokes Start on provider_, then runs all pending tasks.
  void QueryForInput(const base::string16& text,
                     bool prevent_inline_autocomplete,
                     bool prefer_keyword);

  // Calls QueryForInput(), finishes any suggest query, then if |wyt_match| is
  // not nullptr, sets it to the "what you typed" entry for |text|.
  void QueryForInputAndSetWYTMatch(const base::string16& text,
                                   AutocompleteMatch* wyt_match);

  // Calls QueryForInput(), sets the JSON responses for the default and keyword
  // fetchers, and waits until the responses have been returned and the matches
  // returned.  Use empty responses for each fetcher that shouldn't be set up /
  // configured.
  void QueryForInputAndWaitForFetcherResponses(
      const base::string16& text,
      const bool prefer_keyword,
      const std::string& default_fetcher_response,
      const std::string& keyword_fetcher_response);

  // Notifies the URLFetcher for the suggest query corresponding to the default
  // search provider that it's done.
  // Be sure and wrap calls to this in ASSERT_NO_FATAL_FAILURE.
  void FinishDefaultSuggestQuery(const base::string16& query_text);

  // Verifies that |matches| and |expected_matches| agree on the first
  // |num_expected_matches|, displaying an error message that includes
  // |description| for any disagreement.
  void CheckMatches(const std::string& description,
                    const size_t num_expected_matches,
                    const ExpectedMatch expected_matches[],
                    const ACMatches& matches);

  void ResetFieldTrialList();

  // Enable or disable the specified Omnibox field trial rule.
  base::FieldTrial* CreateFieldTrial(const char* field_trial_rule,
                                     bool enabled);

  void ClearAllResults();

  // See description above class for details of these fields.
  TemplateURL* default_t_url_;
  const base::string16 term1_;
  GURL term1_url_;
  TemplateURL* keyword_t_url_;
  const base::string16 keyword_term_;
  GURL keyword_url_;

  content::TestBrowserThreadBundle thread_bundle_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfile profile_;
  std::unique_ptr<TestAutocompleteProviderClient> client_;
  scoped_refptr<SearchProviderForTest> provider_;

  // If not nullptr, OnProviderUpdate quits the current |run_loop_|.
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

// static
const char SearchProviderTest::kNotApplicable[] = "Not Applicable";
const SearchProviderTest::ExpectedMatch
    SearchProviderTest::kEmptyExpectedMatch = { kNotApplicable, false };

void SearchProviderTest::SetUp() {
  // We need both the history service and template url model loaded.
  ASSERT_TRUE(profile_.CreateHistoryService(true, false));
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  turl_model->Load();

  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("t"));
  data.SetURL("http://defaultturl/{searchTerms}");
  data.suggestions_url = "http://defaultturl2/{searchTerms}";
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);
  TemplateURLID default_provider_id = default_t_url_->id();
  ASSERT_NE(0, default_provider_id);

  // Add url1, with search term term1_.
  term1_url_ = AddSearchToHistory(default_t_url_, term1_, 1);

  // Create another TemplateURL.
  data.SetShortName(ASCIIToUTF16("k"));
  data.SetKeyword(ASCIIToUTF16("k"));
  data.SetURL("http://keyword/{searchTerms}");
  data.suggestions_url = "http://suggest_keyword/{searchTerms}";
  keyword_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_t_url_->id());

  // Add a page and search term for keyword_t_url_.
  keyword_url_ = AddSearchToHistory(keyword_t_url_, keyword_term_, 1);

  // Keywords are updated by the InMemoryHistoryBackend only after the message
  // has been processed on the history thread. Block until history processes all
  // requests to ensure the InMemoryDatabase is the state we expect it.
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_,
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

  client_.reset(
      new TestAutocompleteProviderClient(&profile_, &test_url_loader_factory_));
  provider_ = new SearchProviderForTest(client_.get(), this, &profile_);
  OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs = 0;
}

void SearchProviderTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  // Shutdown the provider before the profile.
  provider_ = nullptr;
}

void SearchProviderTest::RunTest(TestData* cases,
                                 int num_cases,
                                 bool prefer_keyword) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                            ChromeAutocompleteSchemeClassifier(&profile_));
    input.set_prefer_keyword(prefer_keyword);
    provider_->Start(input, false);
    matches = provider_->matches();
    SCOPED_TRACE(
        ASCIIToUTF16("Input was: ") +
        cases[i].input +
        ASCIIToUTF16("; prefer_keyword was: ") +
        (prefer_keyword ? ASCIIToUTF16("true") : ASCIIToUTF16("false")));
    EXPECT_EQ(cases[i].num_results, matches.size());
    if (matches.size() == cases[i].num_results) {
      for (size_t j = 0; j < cases[i].num_results; ++j) {
        EXPECT_EQ(cases[i].output[j].gurl, matches[j].destination_url);
        EXPECT_EQ(cases[i].output[j].result_type, matches[j].type);
        EXPECT_EQ(cases[i].output[j].fill_into_edit,
                  matches[j].fill_into_edit);
        EXPECT_EQ(cases[i].output[j].allowed_to_be_default_match,
                  matches[j].allowed_to_be_default_match);
      }
    }
  }
}

void SearchProviderTest::OnProviderUpdate(bool updated_matches) {
  if (run_loop_ && provider_->done()) {
    run_loop_->Quit();
    run_loop_ = nullptr;
  }
}

void SearchProviderTest::RunTillProviderDone() {
  if (provider_->done())
    return;

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
}

void SearchProviderTest::QueryForInput(const base::string16& text,
                                       bool prevent_inline_autocomplete,
                                       bool prefer_keyword) {
  // Start a query.
  AutocompleteInput input(text, metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input.set_prefer_keyword(prefer_keyword);
  provider_->Start(input, false);

  // RunUntilIdle so that the task scheduled by SearchProvider to create the
  // URLFetchers runs.
  base::RunLoop().RunUntilIdle();
}

void SearchProviderTest::QueryForInputAndSetWYTMatch(
    const base::string16& text,
    AutocompleteMatch* wyt_match) {
  QueryForInput(text, false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(text));
  if (!wyt_match)
    return;
  ASSERT_GE(provider_->matches().size(), 1u);
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(base::CollapseWhitespace(
              text, false)),
          TemplateURLServiceFactory::GetForProfile(
              &profile_)->search_terms_data())),
      wyt_match));
}

void SearchProviderTest::QueryForInputAndWaitForFetcherResponses(
    const base::string16& text,
    const bool prefer_keyword,
    const std::string& default_fetcher_response,
    const std::string& keyword_fetcher_response) {
  test_url_loader_factory_.ClearResponses();
  QueryForInput(text, false, prefer_keyword);

  std::string text8;
  ASSERT_TRUE(base::UTF16ToUTF8(text.data(), text.length(), &text8));

  if (!default_fetcher_response.empty()) {
    test_url_loader_factory_.AddResponse(
        base::StrCat({"http://defaultturl2/", net::EscapePath(text8)}),
        default_fetcher_response);
  }
  if (!keyword_fetcher_response.empty()) {
    // If the query is "k whatever", matching what the keyword provider was
    // registered under in SetUp(), it gets just "whatever" in its URL.
    // FRAGILE: this only handles the most straightforward way of expressing
    // these queries. Tests that use this method and pass in a more complicated
    // ones will likely not terminate.
    std::string keyword = text8;
    if (base::StartsWith(keyword, "k ", base::CompareCase::SENSITIVE))
      keyword = keyword.substr(2);
    test_url_loader_factory_.AddResponse(
        base::StrCat({"http://suggest_keyword/", net::EscapePath(keyword)}),
        keyword_fetcher_response);
  }
  RunTillProviderDone();
}

GURL SearchProviderTest::AddSearchToHistory(TemplateURL* t_url,
                                            base::string16 term,
                                            int visit_count) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      &profile_, ServiceAccessType::EXPLICIT_ACCESS);
  GURL search(t_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(term),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  static base::Time last_added_time;
  last_added_time = std::max(base::Time::Now(),
      last_added_time + base::TimeDelta::FromMicroseconds(1));
  history->AddPageWithDetails(search, base::string16(), visit_count,
                              visit_count, last_added_time, false,
                              history::SOURCE_BROWSED);
  history->SetKeywordSearchTermsForURL(search, t_url->id(), term);
  return search;
}

bool SearchProviderTest::FindMatchWithContents(const base::string16& contents,
                                               AutocompleteMatch* match) {
  for (auto i = provider_->matches().begin(); i != provider_->matches().end();
       ++i) {
    if (i->contents == contents) {
      *match = *i;
      return true;
    }
  }
  return false;
}

bool SearchProviderTest::FindMatchWithDestination(const GURL& url,
                                                  AutocompleteMatch* match) {
  for (auto i = provider_->matches().begin(); i != provider_->matches().end();
       ++i) {
    if (i->destination_url == url) {
      *match = *i;
      return true;
    }
  }
  return false;
}

void SearchProviderTest::FinishDefaultSuggestQuery(
    const base::string16& query_text) {
  std::string text8;
  ASSERT_TRUE(
      base::UTF16ToUTF8(query_text.data(), query_text.length(), &text8));
  std::string url =
      base::StrCat({"http://defaultturl2/", net::EscapePath(text8)});

  ASSERT_TRUE(test_url_loader_factory_.IsPending(url));

  // Tell the SearchProvider the default suggest query is done.
  test_url_loader_factory_.AddResponse(url, "");
}

void SearchProviderTest::CheckMatches(const std::string& description,
                                      const size_t num_expected_matches,
                                      const ExpectedMatch expected_matches[],
                                      const ACMatches& matches) {
  ASSERT_FALSE(matches.empty());
  ASSERT_LE(matches.size(), num_expected_matches);
  size_t i = 0;
  SCOPED_TRACE(description);
  // Ensure that the returned matches equal the expectations.
  for (; i < matches.size(); ++i) {
    SCOPED_TRACE(" Case # " + base::NumberToString(i));
    EXPECT_EQ(ASCIIToUTF16(expected_matches[i].contents), matches[i].contents);
    EXPECT_EQ(expected_matches[i].allowed_to_be_default_match,
              matches[i].allowed_to_be_default_match);
  }
  // Ensure that no expected matches are missing.
  for (; i < num_expected_matches; ++i) {
    SCOPED_TRACE(" Case # " + base::NumberToString(i));
    EXPECT_EQ(kNotApplicable, expected_matches[i].contents);
  }
}

void SearchProviderTest::ResetFieldTrialList() {
  // Destroy the existing FieldTrialList before creating a new one to avoid
  // a DCHECK.
  field_trial_list_.reset();
  field_trial_list_.reset(new base::FieldTrialList(
      std::make_unique<variations::SHA1EntropyProvider>("foo")));
  variations::testing::ClearAllVariationParams();
}

base::FieldTrial* SearchProviderTest::CreateFieldTrial(
    const char* field_trial_rule,
    bool enabled) {
  std::map<std::string, std::string> params;
  params[std::string(field_trial_rule)] = enabled ?
      "true" : "false";
  variations::AssociateVariationParams(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
  return base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
}

void SearchProviderTest::ClearAllResults() {
  provider_->ClearAllResults();
}

// Actual Tests ---------------------------------------------------------------

// Make sure we query history for the default provider and a URLFetcher is
// created for the default provider suggest results.
TEST_F(SearchProviderTest, QueryDefaultProvider) {
  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // Make sure the default providers suggest service was queried.
  std::string expected_url(
      default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(&profile_)
              ->search_terms_data()));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url));

  // Tell the SearchProvider the suggest query is done.
  test_url_loader_factory_.AddResponse(expected_url, "");

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term term1.
  AutocompleteMatch term1_match;
  EXPECT_TRUE(FindMatchWithDestination(term1_url_, &term1_match));
  // Term1 should not have a description, it's set later.
  EXPECT_TRUE(term1_match.description.empty());

  AutocompleteMatch wyt_match;
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(
              &profile_)->search_terms_data())),
      &wyt_match));
  EXPECT_TRUE(wyt_match.description.empty());

  // The match for term1 should be more relevant than the what you typed match.
  EXPECT_GT(term1_match.relevance, wyt_match.relevance);
  // This longer match should be inlineable.
  EXPECT_TRUE(term1_match.allowed_to_be_default_match);
  // The what you typed match should be too, of course.
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Make sure we get a query-what-you-typed result from the default search
// provider even if the default search provider's keyword is renamed in the
// middle of processing the query.
TEST_F(SearchProviderTest, HasQueryWhatYouTypedIfDefaultKeywordChanges) {
  base::string16 query = ASCIIToUTF16("query");
  QueryForInput(query, false, false);

  // Make sure the default provider's suggest service was queried.
  EXPECT_TRUE(test_url_loader_factory_.IsPending("http://defaultturl2/query"));

  // Look up the TemplateURL for the keyword and modify its keyword.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile_);
  TemplateURL* template_url =
      template_url_service->GetTemplateURLForKeyword(default_t_url_->keyword());
  EXPECT_TRUE(template_url);
  template_url_service->ResetTemplateURL(
      template_url, template_url->short_name(),
      ASCIIToUTF16("new_keyword_asdf"), template_url->url());

  // In resetting the default provider, the fetcher should've been canceled.
  EXPECT_FALSE(test_url_loader_factory_.IsPending("http://defaultturl2/query"));
  RunTillProviderDone();

  // Makes sure the query-what-you-typed match is there.
  AutocompleteMatch wyt_match;
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(query),
          TemplateURLServiceFactory::GetForProfile(
              &profile_)->search_terms_data())),
      &wyt_match));
  EXPECT_TRUE(wyt_match.description.empty());
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

TEST_F(SearchProviderTest, HonorPreventInlineAutocomplete) {
  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, true, false);

  ASSERT_FALSE(provider_->matches().empty());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  EXPECT_TRUE(provider_->matches()[0].allowed_to_be_default_match);
}

// Issues a query that matches the registered keyword and makes sure history
// is queried as well as URLFetchers getting created.
TEST_F(SearchProviderTest, QueryKeywordProvider) {
  base::string16 term = keyword_term_.substr(0, keyword_term_.length() - 1);
  QueryForInput(ASCIIToUTF16("k ") + term, false, false);

  // Make sure the default providers suggest service was queried.
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending("http://defaultturl2/k%20keywor"));

  // Tell the SearchProvider the default suggest query is done.
  test_url_loader_factory_.AddResponse("http://defaultturl2/k%20keywor", "");

  // Make sure the keyword providers suggest service was queried, with
  // the URL we expected.
  std::string expected_url(
      keyword_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(&profile_)
              ->search_terms_data()));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url));

  // Tell the SearchProvider the keyword suggest query is done.
  test_url_loader_factory_.AddResponse("http://suggest_keyword/keywor", "");

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term keyword.
  AutocompleteMatch match;
  EXPECT_TRUE(FindMatchWithDestination(keyword_url_, &match));

  // The match should have an associated keyword.
  EXPECT_FALSE(match.keyword.empty());

  // The fill into edit should contain the keyword.
  EXPECT_EQ(keyword_t_url_->keyword() + base::char16(' ') + keyword_term_,
            match.fill_into_edit);
}

TEST_F(SearchProviderTest, SendDataToSuggestAtAppropriateTimes) {
  struct {
    std::string input;
    const bool expect_to_send_to_default_provider;
  } cases[] = {
    // None of the following input strings should be sent to the default
    // suggest server because they may contain potentially private data.
    { "username:password",                  false },
    { "User:f",                             false },
    { "http://username:password",           false },
    { "https://username:password",          false },
    { "username:password@hostname",         false },
    { "http://username:password@hostname/", false },
    { "file://filename",                    false },
    { "data://data",                        false },
    { "unknownscheme:anything",             false },
    { "http://hostname/?query=q",           false },
    { "http://hostname/path#ref",           false },
    { "http://hostname/path #ref",          false },
    { "https://hostname/path",              false },
    // For all of the following input strings, it doesn't make much difference
    // if we allow them to be sent to the default provider or not.  The strings
    // need to be in this list of test cases however so that they are tested
    // against the keyword provider and verified that they are allowed to be
    // sent to it.
    { "User:",                              false },
    { "User::",                             false },
    { "User:!",                             false },
    // All of the following input strings should be sent to the default suggest
    // server because they should not get caught by the private data checks.
    { "User",                               true },
    { "query",                              true },
    { "query with spaces",                  true },
    { "http://hostname",                    true },
    { "http://hostname/path",               true },
    { "http://hostname #ref",               true },
    { "www.hostname.com #ref",              true },
    { "https://hostname",                   true },
    { "#hashtag",                           true },
    { "foo https://hostname/path",          true },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE("for input=" + cases[i].input);
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    // Make sure the default provider's suggest service was or was not queried
    // as appropriate.
    EXPECT_EQ(cases[i].expect_to_send_to_default_provider,
              test_url_loader_factory_.IsPending(base::StrCat(
                  {"http://defaultturl2/", net::EscapePath(cases[i].input)})));

    // Send the same input with an explicitly invoked keyword.  In all cases,
    // it's okay to send the request to the keyword suggest server.
    QueryForInput(ASCIIToUTF16("k ") + ASCIIToUTF16(cases[i].input), false,
                  false);
    EXPECT_TRUE(test_url_loader_factory_.IsPending(base::StrCat(
        {"http://suggest_keyword/", net::EscapePath(cases[i].input)})));
  }
}

TEST_F(SearchProviderTest, DontAutocompleteURLLikeTerms) {
  GURL url = AddSearchToHistory(default_t_url_,
                                ASCIIToUTF16("docs.google.com"), 1);

  // Add the term as a url.
  HistoryServiceFactory::GetForProfile(&profile_,
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPageWithDetails(GURL("http://docs.google.com"), base::string16(), 1,
                           1, base::Time::Now(), false,
                           history::SOURCE_BROWSED);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("docs"),
                                                      &wyt_match));

  // There should be two matches, one for what you typed, the other for
  // 'docs.google.com'. The search term should have a lower priority than the
  // what you typed match.
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(url, &term_match));
  EXPECT_GT(wyt_match.relevance, term_match.relevance);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
}

// A multiword search with one visit should not autocomplete until multiple
// words are typed.
TEST_F(SearchProviderTest, DontAutocompleteUntilMultipleWordsTyped) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("one search"),
                                   1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("on"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(wyt_match.relevance, term_match.relevance);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("one se"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// A multiword search with more than one visit should autocomplete immediately.
TEST_F(SearchProviderTest, AutocompleteMultipleVisitsImmediately) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("two searches"),
                                   2));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("tw"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Autocompletion should work at a word boundary after a space, and should
// offer a suggestion for the trimmed search query.
TEST_F(SearchProviderTest, AutocompleteAfterSpace) {
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("two  searches "), 2);
  GURL suggested_url(default_t_url_->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("two searches")),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("two "),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(suggested_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_EQ(ASCIIToUTF16("searches"), term_match.inline_autocompletion);
  EXPECT_EQ(ASCIIToUTF16("two searches"), term_match.fill_into_edit);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Newer multiword searches should score more highly than older ones.
TEST_F(SearchProviderTest, ScoreNewerSearchesHigher) {
  GURL term_url_a(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("three searches aaa"), 1));
  GURL term_url_b(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("three searches bbb"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("three se"),
                                                      &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());
  AutocompleteMatch term_match_a;
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  AutocompleteMatch term_match_b;
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_GT(term_match_b.relevance, term_match_a.relevance);
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// If ScoreHistoryResults doesn't properly clear its output vector it can skip
// scoring the actual results and just return results from a previous run.
TEST_F(SearchProviderTest, ResetResultsBetweenRuns) {
  GURL term_url_a(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("games"), 1));
  GURL term_url_b(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("gangnam style"), 1));
  GURL term_url_c(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("gundam"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("f"),
                                                      &wyt_match));
  ASSERT_EQ(1u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("g"),
                                                      &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("ga"),
                                                      &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("gan"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("gans"),
                                                      &wyt_match));
  ASSERT_EQ(1u, provider_->matches().size());
}

// An autocompleted multiword search should not be replaced by a different
// autocompletion while the user is still typing a valid prefix unless the
// user has typed the prefix as a query before.
TEST_F(SearchProviderTest, DontReplacePreviousAutocompletion) {
  GURL term_url_a(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches aaa"), 3));
  GURL term_url_b(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches bbb"), 1));
  GURL term_url_c(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("fo"),
                                                      &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());
  AutocompleteMatch term_match_a;
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  AutocompleteMatch term_match_b;
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  AutocompleteMatch term_match_c;
  EXPECT_TRUE(FindMatchWithDestination(term_url_c, &term_match_c));
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  // We don't care about the relative order of b and c.
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_c.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_c.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("four se"),
                                                      &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_TRUE(FindMatchWithDestination(term_url_c, &term_match_c));
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_c.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_c.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);

  // For the exact previously-issued query, the what-you-typed match should win.
  ASSERT_NO_FATAL_FAILURE(
      QueryForInputAndSetWYTMatch(ASCIIToUTF16("four searches"), &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_GT(wyt_match.relevance, term_match_a.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Non-completable multiword searches should not crowd out single-word searches.
TEST_F(SearchProviderTest, DontCrowdOutSingleWords) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("five"), 1));
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches bbb"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches ccc"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches ddd"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches eee"), 1);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("fi"),
                                                      &wyt_match));
  ASSERT_EQ(AutocompleteProvider::kMaxMatches + 1, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Inline autocomplete matches regardless of case differences from the input.
TEST_F(SearchProviderTest, InlineMixedCaseMatches) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("FOO"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("f"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_EQ(ASCIIToUTF16("FOO"), term_match.fill_into_edit);
  EXPECT_EQ(ASCIIToUTF16("OO"), term_match.inline_autocompletion);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  // Make sure the case doesn't affect the highlighting.
  // (SearchProvider intentionally marks the new text as MATCH; that's why
  // the tests below look backwards.)
  ASSERT_EQ(2U, term_match.contents_class.size());
  EXPECT_EQ(0U, term_match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::NONE,
            term_match.contents_class[0].style);
  EXPECT_EQ(1U, term_match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::MATCH,
            term_match.contents_class[1].style);
}

// Verifies AutocompleteControllers return results (including keyword
// results) in the right order and set descriptions for them correctly.
TEST_F(SearchProviderTest, KeywordOrderingAndDescriptions) {
  // Add an entry that corresponds to a keyword search with 'term2'.
  AddSearchToHistory(keyword_t_url_, ASCIIToUTF16("term2"), 1);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteController controller(
      std::make_unique<TestAutocompleteProviderClient>(
          &profile_, &test_url_loader_factory_),
      nullptr, AutocompleteProvider::TYPE_SEARCH);
  AutocompleteInput input(ASCIIToUTF16("k t"),
                          metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  controller.Start(input);
  const AutocompleteResult& result = controller.result();

  // There should be three matches, one for the keyword history, one for
  // keyword provider's what-you-typed, and one for the default provider's
  // what you typed, in that order.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, result.match_at(0).type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            result.match_at(1).type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            result.match_at(2).type);
  EXPECT_GT(result.match_at(0).relevance, result.match_at(1).relevance);
  EXPECT_GT(result.match_at(1).relevance, result.match_at(2).relevance);
  EXPECT_TRUE(result.match_at(0).allowed_to_be_default_match);
  EXPECT_TRUE(result.match_at(1).allowed_to_be_default_match);
  EXPECT_FALSE(result.match_at(2).allowed_to_be_default_match);

  // The two keyword results should come with the keyword we expect.
  EXPECT_EQ(ASCIIToUTF16("k"), result.match_at(0).keyword);
  EXPECT_EQ(ASCIIToUTF16("k"), result.match_at(1).keyword);
  // The default provider has a different keyword.  (We don't explicitly
  // set it during this test, so all we do is assert that it's different.)
  EXPECT_NE(result.match_at(0).keyword, result.match_at(2).keyword);

  // The top result will always have a description.  The third result,
  // coming from a different provider than the first two, should also.
  // Whether the second result has one doesn't matter much.  (If it was
  // missing, people would infer that it's the same search provider as
  // the one above it.)
  EXPECT_FALSE(result.match_at(0).description.empty());
  EXPECT_FALSE(result.match_at(2).description.empty());
  EXPECT_NE(result.match_at(0).description, result.match_at(2).description);
}

TEST_F(SearchProviderTest, KeywordVerbatim) {
  TestData cases[] = {
    // Test a simple keyword input.
    { ASCIIToUTF16("k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo") ) } },

    // Make sure extra whitespace after the keyword doesn't change the
    // keyword verbatim query.  Also verify that interior consecutive
    // whitespace gets trimmed.
    { ASCIIToUTF16("k   foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo")) } },
    // Leading whitespace should be stripped before SearchProvider gets the
    // input; hence there are no tests here about how it handles those inputs.

    // Verify that interior consecutive whitespace gets trimmed in either case.
    { ASCIIToUTF16("k  foo  bar"), 2,
      { ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo bar")),
        ResultInfo(GURL("http://defaultturl/k%20foo%20bar"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo bar")) } },

    // Verify that trailing whitespace gets trimmed.
    { ASCIIToUTF16("k foo bar  "), 2,
      { ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo bar")),
        ResultInfo(GURL("http://defaultturl/k%20foo%20bar"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo bar")) } },

    // Keywords can be prefixed by certain things that should get ignored
    // when constructing the keyword match.
    { ASCIIToUTF16("www.k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/www.k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("www.k foo")) } },
    { ASCIIToUTF16("http://k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/http%3A//k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("http://k foo")) } },
    { ASCIIToUTF16("http://www.k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/http%3A//www.k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("http://www.k foo")) } },

    // A keyword with no remaining input shouldn't get a keyword
    // verbatim match.
    { ASCIIToUTF16("k"), 1,
      { ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   true,
                   ASCIIToUTF16("k")) } },
    // Ditto.  Trailing whitespace shouldn't make a difference.
    { ASCIIToUTF16("k "), 1,
      { ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   true,
                   ASCIIToUTF16("k")) } }

    // The fact that verbatim queries to keyword are handled by KeywordProvider
    // not SearchProvider is tested in
    // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc.
  };

  // Test not in keyword mode.
  RunTest(cases, arraysize(cases), false);

  // Test in keyword mode.  (Both modes should give the same result.)
  RunTest(cases, arraysize(cases), true);
}

// Ensures command-line flags are reflected in the URLs the search provider
// generates.
TEST_F(SearchProviderTest, CommandLineOverrides) {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("default"));
  data.SetKeyword(data.short_name());
  data.SetURL("{google:baseURL}{searchTerms}");
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.bar.com/");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");

  TestData cases[] = {
    { ASCIIToUTF16("k a"), 2,
      { ResultInfo(GURL("http://keyword/a"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k a")),
        ResultInfo(GURL("http://www.bar.com/k%20a?a=b"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k a")) } },
  };

  RunTest(cases, arraysize(cases), false);
}

// Verifies Navsuggest results don't set a TemplateURL, which Instant relies on.
// Also verifies that just the *first* navigational result is listed as a match
// if suggested relevance scores were not sent.
TEST_F(SearchProviderTest, NavSuggestNoSuggestedRelevanceScores) {
  QueryForInputAndWaitForFetcherResponses(
      ASCIIToUTF16("a.c"), false,
      "[\"a.c\",[\"a.com\", \"a.com/b\"],[\"a\", \"b\"],[],"
      "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      std::string());

  // Make sure the only match is 'a.com' and it doesn't have a template_url.
  AutocompleteMatch nav_match;
  EXPECT_TRUE(FindMatchWithDestination(GURL("http://a.com"), &nav_match));
  EXPECT_TRUE(nav_match.keyword.empty());
  EXPECT_FALSE(nav_match.allowed_to_be_default_match);
  EXPECT_FALSE(FindMatchWithDestination(GURL("http://a.com/b"), &nav_match));
}

// Verifies that the most relevant suggest results are added properly.
TEST_F(SearchProviderTest, SuggestRelevance) {
  QueryForInputAndWaitForFetcherResponses(
      ASCIIToUTF16("a"), false, "[\"a\",[\"a1\", \"a2\", \"a3\", \"a4\"]]",
      std::string());

  // Check the expected verbatim and (first 3) suggestions' relative relevances.
  AutocompleteMatch verbatim, match_a1, match_a2, match_a3, match_a4;
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a"), &verbatim));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a1"), &match_a1));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a2"), &match_a2));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a3"), &match_a3));
  EXPECT_FALSE(FindMatchWithContents(ASCIIToUTF16("a4"), &match_a4));
  EXPECT_GT(verbatim.relevance, match_a1.relevance);
  EXPECT_GT(match_a1.relevance, match_a2.relevance);
  EXPECT_GT(match_a2.relevance, match_a3.relevance);
  EXPECT_TRUE(verbatim.allowed_to_be_default_match);
  EXPECT_FALSE(match_a1.allowed_to_be_default_match);
  EXPECT_FALSE(match_a2.allowed_to_be_default_match);
  EXPECT_FALSE(match_a3.allowed_to_be_default_match);
}

// Verifies that the default provider abandons suggested relevance scores
// when in keyword mode.  This should happen regardless of whether the
// keyword provider returns suggested relevance scores.
TEST_F(SearchProviderTest, DefaultProviderNoSuggestRelevanceInKeywordMode) {
  struct {
    const std::string default_provider_json;
    const std::string keyword_provider_json;
    const std::string matches[5];
  } cases[] = {
    // First, try an input where the keyword provider does not deliver
    // suggested relevance scores.
    { "[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
      "{\"google:verbatimrelevance\":9700,"
      "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
      "\"google:suggestrelevance\":[9900, 9800]}]",
      "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"]}]",
      { "a", "akeyword-query", "k a", "adefault.com", "k adefault-query" } },

    // Now try with keyword provider suggested relevance scores.
    { "[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
      "{\"google:verbatimrelevance\":9700,"
      "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
      "\"google:suggestrelevance\":[9900, 9800]}]",
      "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"],"
      "\"google:verbatimrelevance\":9500,"
      "\"google:suggestrelevance\":[9600]}]",
      { "akeyword-query", "a", "k a", "adefault.com", "k adefault-query" } }
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      QueryForInputAndWaitForFetcherResponses(
          ASCIIToUTF16("k a"), true, cases[i].default_provider_json,
          cases[i].keyword_provider_json);
    }

    SCOPED_TRACE(
        "for input with default_provider_json=" +
        cases[i].default_provider_json + " and keyword_provider_json=" +
        cases[i].keyword_provider_json);
    const ACMatches& matches = provider_->matches();
    ASSERT_LE(matches.size(), arraysize(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j)
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]), matches[j].contents);
    // Ensure that no expected matches are missing.
    for (; j < arraysize(cases[i].matches); ++j)
      EXPECT_EQ(std::string(), cases[i].matches[j]);
  }
}

// Verifies that suggest results with relevance scores are added
// properly when using the default fetcher.  When adding a new test
// case to this test, please consider adding it to the tests in
// KeywordFetcherSuggestRelevance below.
TEST_F(SearchProviderTest, DefaultFetcherSuggestRelevance) {
  struct {
    const std::string json;
    const ExpectedMatch matches[6];
    const std::string inline_autocompletion;
  } cases[] = {
    // Ensure that suggestrelevance scores reorder matches.
    { "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a", true }, { "c", false }, { "b", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", true }, { "c.com", false }, { "b.com", false },
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },

    // Without suggested relevance scores, we should only allow one
    // navsuggest result to be be displayed.
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      { { "a", true }, { "b.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },

    // Ensure that verbatimrelevance scores reorder or suppress verbatim.
    // Negative values will have no effect; the calculated value will be used.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9999,"
                             "\"google:suggestrelevance\":[9998]}]",
      { { "a", true}, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9998,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, { "a", true }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":-1,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, { "a", true }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9999,"
        "\"google:suggestrelevance\":[9998]}]",
      { { "a", true }, { "a.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9998,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, { "a", true }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      ".com" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      ".com" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":-1,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, { "a", true }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      ".com" },

    // Ensure that both types of relevance scores reorder matches together.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[9999, 9997],"
                                     "\"google:verbatimrelevance\":9998}]",
      { { "a1", true }, { "a", true }, { "a2", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },

    // Check that an inlineable result appears first regardless of its score.
    // Also, if the result set lacks a single inlineable result, abandon the
    // request to suppress verbatim (verbatim_relevance=0), which will then
    // cause verbatim to appear (first).
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999]}]",
      { { "a", true }, { "b", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999],"
                            "\"google:verbatimrelevance\":0}]",
      { { "a", true }, { "b", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a", true }, { "b.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a", true }, { "b.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },

    // Allow low-scoring matches.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a1", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":10}]",
      { { "a1", true }, { "a", true }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[10],"
                             "\"google:verbatimrelevance\":0}]",
      { { "a1", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "1" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 20],"
                                     "\"google:verbatimrelevance\":0}]",
      { { "a2", true }, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "2" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 30],"
      "\"google:verbatimrelevance\":20}]",
      { { "a2", true }, { "a", true }, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      "2" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[10],"
        "\"google:verbatimrelevance\":0}]",
      { { "a.com", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      ".com" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[10, 20],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2.com", true }, { "a1.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      "2.com" },

    // Ensure that all suggestions are considered, regardless of order.
    { "[\"a\",[\"b\", \"c\", \"d\", \"e\", \"f\", \"g\", \"h\"],[],[],"
       "{\"google:suggestrelevance\":[10, 20, 30, 40, 50, 60, 70]}]",
      { { "a", true }, { "h", false }, { "g", false }, { "f", false },
        { "e", false }, { "d", false } },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"http://d.com\","
              "\"http://e.com\", \"http://f.com\", \"http://g.com\","
              "\"http://h.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[10, 20, 30, 40, 50, 60, 70]}]",
      { { "a", true }, { "h.com", false }, { "g.com", false },
        { "f.com", false }, { "e.com", false }, { "d.com", false } },
      std::string() },

    // Ensure that incorrectly sized suggestion relevance lists are ignored.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10]}]",
      { { "a", true }, { "a1", false }, { "a2", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[9999, 10]}]",
      { { "a", true }, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[10]}]",
      { { "a", true }, { "a1.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9999, 10]}]",
      { { "a", true }, { "a1.com", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },

    // Ensure that all 'verbatim' results are merged with their maximum score.
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a2", true }, { "a", true }, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      "2" },
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2", true }, { "a", true }, { "a1", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch },
      "2" },

    // Ensure that verbatim is always generated without other suggestions.
    // TODO(msw): Ensure verbatimrelevance is respected (except suppression).
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch, kEmptyExpectedMatch, kEmptyExpectedMatch },
      std::string() },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      QueryForInputAndWaitForFetcherResponses(
          ASCIIToUTF16("a"), false, cases[i].json, std::string());
    }

    const std::string description = "for input with json=" + cases[i].json;
    CheckMatches(description, arraysize(cases[i].matches), cases[i].matches,
                 provider_->matches());
  }
}

// Verifies that suggest results with relevance scores are added
// properly when using the keyword fetcher.  This is similar to the
// test DefaultFetcherSuggestRelevance above but this uses inputs that
// trigger keyword suggestions (i.e., "k a" rather than "a") and has
// different expectations (because now the results are a mix of
// keyword suggestions and default provider suggestions).  When a new
// test is added to this TEST_F, please consider if it would be
// appropriate to add to DefaultFetcherSuggestRelevance as well.
TEST_F(SearchProviderTest, KeywordFetcherSuggestRelevance) {
  struct KeywordFetcherMatch {
    std::string contents;
    bool from_keyword;
    bool allowed_to_be_default_match;
  };
  const KeywordFetcherMatch kEmptyMatch = { kNotApplicable, false, false };
  struct {
    const std::string json;
    const KeywordFetcherMatch matches[6];
    const std::string inline_autocompletion;
  } cases[] = {
    // clang-format off
    // Ensure that suggest relevance scores reorder matches and that
    // the keyword verbatim (lacking a suggested verbatim score) beats
    // the default provider verbatim.
    { "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a",   true,  true },
        { "k a", false, false },
        { "c",   true,  false },
        { "b",   true,  false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    // Again, check that relevance scores reorder matches, just this
    // time with navigation matches.  This also checks that with
    // suggested relevance scores we allow multiple navsuggest results.
    // Note that navsuggest results that come from a keyword provider
    // are marked as not a keyword result.  (They don't go to a
    // keyword search engine.)
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"d\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
       "\"google:suggestrelevance\":[1301, 1302, 1303]}]",
      { { "a",     true,  true },
        { "d",     true,  false },
        { "c.com", false, false },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch },
      std::string() },

    // Without suggested relevance scores, we should only allow one
    // navsuggest result to be be displayed.
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that verbatimrelevance scores reorder or suppress verbatim.
    // Negative values will have no effect; the calculated value will be used.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9999,"
                             "\"google:suggestrelevance\":[9998]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9998,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":-1,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9999,"
        "\"google:suggestrelevance\":[9998]}]",
      { { "a",     true,  true },
        { "a.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that both types of relevance scores reorder matches together.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[9999, 9997],"
                                     "\"google:verbatimrelevance\":9998}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "a2",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "1" },

    // Check that an inlineable match appears first regardless of its score.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999]}]",
      { { "a",   true,  true },
        { "b",   true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // If there is no inlineable match, restore the keyword verbatim score.
    // The keyword verbatim match will then appear first.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999],"
                            "\"google:verbatimrelevance\":0}]",
      { { "a",   true,  true },
        { "b",   true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // The top result does not have to score as highly as calculated
    // verbatim.  i.e., there are no minimum score restrictions in
    // this provider.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":10}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[10],"
                             "\"google:verbatimrelevance\":0}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 20],"
                                     "\"google:verbatimrelevance\":0}]",
      { { "a2",  true,  true },
        { "k a", false, false },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 30],"
      "\"google:verbatimrelevance\":20}]",
      { { "a2",  true,  true },
        { "k a", false, false },
        { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that all suggestions are considered, regardless of order.
    { "[\"a\",[\"b\", \"c\", \"d\", \"e\", \"f\", \"g\", \"h\"],[],[],"
       "{\"google:suggestrelevance\":[10, 20, 30, 40, 50, 60, 70]}]",
      { { "a",   true,  true },
        { "k a", false, false },
        { "h",   true,  false },
        { "g",   true,  false },
        { "f",   true,  false },
        { "e",   true,  false } },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"http://d.com\","
              "\"http://e.com\", \"http://f.com\", \"http://g.com\","
              "\"http://h.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[10, 20, 30, 40, 50, 60, 70]}]",
      { { "a",     true,  true },
        { "k a",   false, false },
        { "h.com", false, false },
        { "g.com", false, false },
        { "f.com", false, false },
        { "e.com", false, false } },
      std::string() },

    // Ensure that incorrectly sized suggestion relevance lists are ignored.
    // Note that keyword suggestions by default (not in suggested relevance
    // mode) score more highly than the default verbatim.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        { "a2",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // In this case, ignoring the suggested relevance scores means we keep
    // only one navsuggest result.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that all 'verbatim' results are merged with their maximum score.
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that verbatim is always generated without other suggestions.
    // TODO(mpearson): Ensure the value of verbatimrelevance is respected
    // (except when suggested relevances are ignored).
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // In reorder mode, navsuggestions will not need to be demoted (because
    // they are marked as not allowed to be default match and will be
    // reordered as necessary).
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999]}]",
      { { "a",      true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"https://a/\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a",   true,  true },
        { "a",   false, false },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // Check when navsuggest scores more than verbatim and there is query
    // suggestion but it scores lower.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999, 1300]}]",
      { { "a",      true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "a3",     true,  false },
        { "k a",    false, false },
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 1300]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a3",     true,  false },
        { "k a",    false, false },
        kEmptyMatch },
      std::string() },
    // Check when navsuggest scores more than a query suggestion.  There is
    // a verbatim but it scores lower.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999, 9997]}]",
      { { "a3",     true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9998, 9999, 9997]}]",
      { { "a3",     true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      "3" },
    // Check when there is neither verbatim nor a query suggestion that,
    // because we can't demote navsuggestions below a query suggestion,
    // we restore the keyword verbatim score.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9998, 9999]}]",
      { { "a",      true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    // More checks that everything works when it's not necessary to demote.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9997, 9998, 9999]}]",
      { { "a3",     true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    // clang-format on
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      test_url_loader_factory_.ClearResponses();
      QueryForInput(ASCIIToUTF16("k a"), false, true);

      // Set up a default fetcher with no results.
      ASSERT_TRUE(
          test_url_loader_factory_.IsPending("http://defaultturl2/k%20a"));
      test_url_loader_factory_.AddResponse("http://defaultturl2/k%20a", "");

      // Set up a keyword fetcher with provided results.
      ASSERT_TRUE(
          test_url_loader_factory_.IsPending("http://suggest_keyword/a"));
      test_url_loader_factory_.AddResponse("http://suggest_keyword/a",
                                           cases[i].json);

      RunTillProviderDone();
    }

    SCOPED_TRACE("for input with json=" + cases[i].json);
    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());
    // Find the first match that's allowed to be the default match and check
    // its inline_autocompletion.
    auto it = FindDefaultMatch(matches);
    ASSERT_NE(matches.end(), it);
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              it->inline_autocompletion);

    ASSERT_LE(matches.size(), arraysize(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j].contents),
                matches[j].contents);
      EXPECT_EQ(cases[i].matches[j].from_keyword,
                matches[j].keyword == ASCIIToUTF16("k"));
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
    // Ensure that no expected matches are missing.
    for (; j < arraysize(cases[i].matches); ++j) {
      SCOPED_TRACE(" Case # " + base::NumberToString(i));
      EXPECT_EQ(kNotApplicable, cases[i].matches[j].contents);
    }
  }
}

TEST_F(SearchProviderTest, DontInlineAutocompleteAsynchronously) {
  // This test sends two separate queries, each receiving different JSON
  // replies, and checks that at each stage of processing (receiving first
  // asynchronous response, handling new keystroke synchronously / sending the
  // second request, and receiving the second asynchronous response) we have the
  // expected matches.  In particular, receiving the second response shouldn't
  // cause an unexpected inline autcompletion.
  struct {
    const std::string first_json;
    const ExpectedMatch first_async_matches[4];
    const ExpectedMatch sync_matches[4];
    const std::string second_json;
    const ExpectedMatch second_async_matches[4];
  } cases[] = {
    // A simple test that verifies we don't inline autocomplete after the
    // first asynchronous response, but we do at the next keystroke if the
    // response's results were good enough.  Furthermore, we should continue
    // inline autocompleting after the second asynchronous response if the new
    // top suggestion is the same as the old inline autocompleted suggestion.
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab1", true }, { "ab2", true }, { "ab", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "ab1", true }, { "ab2", false }, { "ab", true },
        kEmptyExpectedMatch } },
    // Ditto, just for a navigation suggestion.
    { "[\"a\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "a", true }, { "ab1.com", false }, { "ab2.com", false },
        kEmptyExpectedMatch },
      { { "ab1.com", true }, { "ab2.com", true }, { "ab", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "ab1.com", true }, { "ab2.com", false }, { "ab", true },
        kEmptyExpectedMatch } },
    // A more realistic test of the same situation.
    { "[\"a\",[\"abcdef\", \"abcdef.com\", \"abc\"],[],[],"
       "{\"google:verbatimrelevance\":900,"
        "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1250, 1200, 1000]}]",
      { { "a", true }, { "abcdef", false }, { "abcdef.com", false },
        { "abc", false } },
      { { "abcdef", true }, { "abcdef.com", true }, { "abc", true },
        { "ab", true } },
      "[\"ab\",[\"abcdef\", \"abcdef.com\", \"abc\"],[],[],"
       "{\"google:verbatimrelevance\":900,"
        "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1250, 1200, 1000]}]",
      { { "abcdef", true }, { "abcdef.com", false }, { "abc", false },
        { "ab", true } } },

    // Without an original inline autcompletion, a new inline autcompletion
    // should be rejected.
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[8000, 7000]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab", true }, { "ab1", true }, { "ab2", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "ab", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch } },
    // For the same test except with the queries scored in the opposite order
    // on the second JSON response, the queries should be ordered by the second
    // response's scores, not the first.
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[8000, 7000]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab", true }, { "ab1", true }, { "ab2", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9001, 9002]}]",
      { { "ab", true }, { "ab2", false }, { "ab1", false },
        kEmptyExpectedMatch } },
    // Now, the same verifications but with the new inline autocompletion as a
    // navsuggestion.  The new autocompletion should still be rejected.
    { "[\"a\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[8000, 7000]}]",
      { { "a", true }, { "ab1.com", false }, { "ab2.com", false },
        kEmptyExpectedMatch },
      { { "ab", true }, { "ab1.com", true }, { "ab2.com", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "ab", true }, { "ab1.com", false }, { "ab2.com", false },
        kEmptyExpectedMatch } },
    { "[\"a\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[8000, 7000]}]",
      { { "a", true }, { "ab1.com", false }, { "ab2.com", false },
        kEmptyExpectedMatch },
      { { "ab", true }, { "ab1.com", true }, { "ab2.com", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1.com\", \"ab2.com\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9001, 9002]}]",
      { { "ab", true }, { "ab2.com", false }, { "ab1.com", false },
        kEmptyExpectedMatch } },

    // It's okay to abandon an inline autocompletion asynchronously.
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab1", true }, { "ab2", true }, { "ab", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[8000, 7000]}]",
      { { "ab", true }, { "ab1", true }, { "ab2", false },
        kEmptyExpectedMatch } },

    // If a suggestion is equivalent to the verbatim suggestion, it should be
    // collapsed into one.  Furthermore, it should be allowed to be the default
    // match even if it was not previously displayed inlined.  This test is
    // mainly for checking the first_async_matches.
    { "[\"a\",[\"A\"],[],[],"
       "{\"google:verbatimrelevance\":9000, "
        "\"google:suggestrelevance\":[9001]}]",
      { { "A", true }, kEmptyExpectedMatch, kEmptyExpectedMatch,
        kEmptyExpectedMatch },
      { { "ab", true }, { "A", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch },
      std::string(),
      { { "ab", true }, { "A", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch } },

    // Note: it's possible that the suggest server returns a suggestion with
    // an inline autocompletion (that as usual we delay in allowing it to
    // be displayed as an inline autocompletion until the next keystroke),
    // then, in response to the next keystroke, the server returns a different
    // suggestion as an inline autocompletion.  This is not likely to happen.
    // Regardless, if it does, one could imagine three different behaviors:
    // - keep the original inline autocompletion until the next keystroke
    //   (i.e., don't abandon an inline autocompletion asynchronously), then
    //   use the new suggestion
    // - abandon all inline autocompletions upon the server response, then use
    //   the new suggestion on the next keystroke
    // - ignore the new inline autocompletion provided by the server, yet
    //   possibly keep the original if it scores well in the most recent
    //   response, then use the new suggestion on the next keystroke
    // All of these behaviors are reasonable.  The main thing we want to
    // ensure is that the second asynchronous response shouldn't cause *a new*
    // inline autocompletion to be displayed.  We test that here.
    // The current implementation does the third bullet, but all of these
    // behaviors seem reasonable.
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab1", true }, { "ab2", true }, { "ab", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab3\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9900]}]",
      { { "ab1", true }, { "ab3", false }, { "ab", true },
        kEmptyExpectedMatch } },
    { "[\"a\",[\"ab1\", \"ab2\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[9002, 9001]}]",
      { { "a", true }, { "ab1", false }, { "ab2", false },
        kEmptyExpectedMatch },
      { { "ab1", true }, { "ab2", true }, { "ab", true },
        kEmptyExpectedMatch },
      "[\"ab\",[\"ab1\", \"ab3\"],[],[],"
       "{\"google:verbatimrelevance\":9000,"
        "\"google:suggestrelevance\":[8000, 9500]}]",
      { { "ab", true }, { "ab3", false }, { "ab1", true },
        kEmptyExpectedMatch } },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // First, send the query "a" and receive the JSON response |first_json|.
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16("a"), false, cases[i].first_json, std::string());

    // Verify that the matches after the asynchronous results are as expected.
    std::string description = "first asynchronous response for input with "
        "first_json=" + cases[i].first_json;
    CheckMatches(description, arraysize(cases[i].first_async_matches),
                 cases[i].first_async_matches, provider_->matches());

    // Then, send the query "ab" and check the synchronous matches.
    description = "synchronous response after the first keystroke after input "
        "with first_json=" + cases[i].first_json;
    QueryForInput(ASCIIToUTF16("ab"), false, false);
    CheckMatches(description, arraysize(cases[i].sync_matches),
                 cases[i].sync_matches, provider_->matches());

    // Finally, get the provided JSON response, |second_json|, and verify the
    // matches after the second asynchronous response are as expected.
    description = "second asynchronous response after input with first_json=" +
        cases[i].first_json + " and second_json=" + cases[i].second_json;
    ASSERT_TRUE(test_url_loader_factory_.IsPending("http://defaultturl2/ab"));
    test_url_loader_factory_.AddResponse("http://defaultturl2/ab",
                                         cases[i].second_json);
    RunTillProviderDone();
    CheckMatches(description, arraysize(cases[i].second_async_matches),
                 cases[i].second_async_matches, provider_->matches());
  }
}

TEST_F(SearchProviderTest, DontCacheCalculatorSuggestions) {
  // This test sends two separate queries and checks that at each stage of
  // processing (receiving first asynchronous response, handling new keystroke
  // synchronously) we have the expected matches.  The new keystroke should
  // immediately invalidate old calculator suggestions.
  struct {
    const std::string json;
    const ExpectedMatch async_matches[4];
    const ExpectedMatch sync_matches[4];
  } cases[] = {
    { "[\"1+2\",[\"3\", \"1+2+3+4+5\"],[],[],"
       "{\"google:verbatimrelevance\":1300,"
        "\"google:suggesttype\":[\"CALCULATOR\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1200, 900]}]",
      { { "1+2", true }, { "3", false }, { "1+2+3+4+5", false },
        kEmptyExpectedMatch },
      { { "1+23", true }, { "1+2+3+4+5", false }, kEmptyExpectedMatch,
        kEmptyExpectedMatch } },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // First, send the query "1+2" and receive the JSON response |first_json|.
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16("1+2"), false, cases[i].json, std::string());

    // Verify that the matches after the asynchronous results are as expected.
    std::string description = "first asynchronous response for input with "
        "json=" + cases[i].json;
    CheckMatches(description, arraysize(cases[i].async_matches),
                 cases[i].async_matches, provider_->matches());

    // Then, send the query "1+23" and check the synchronous matches.
    description = "synchronous response after the first keystroke after input "
        "with json=" + cases[i].json;
    QueryForInput(ASCIIToUTF16("1+23"), false, false);
    CheckMatches(description, arraysize(cases[i].sync_matches),
                 cases[i].sync_matches, provider_->matches());
  }
}

TEST_F(SearchProviderTest, LocalAndRemoteRelevances) {
  // We hardcode the string "term1" below, so ensure that the search term that
  // got added to history already is that string.
  ASSERT_EQ(ASCIIToUTF16("term1"), term1_);
  base::string16 term = term1_.substr(0, term1_.length() - 1);

  AddSearchToHistory(default_t_url_, term + ASCIIToUTF16("2"), 2);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  struct {
    const base::string16 input;
    const std::string json;
    const std::string matches[6];
  } cases[] = {
    // The history results outscore the default verbatim score.  term2 has more
    // visits so it outscores term1.  The suggestions are still returned since
    // they're server-scored.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1, 2, 3]}]",
      { "term2", "term1", "term", "a3", "a2", "a1" } },
    // Because we already have three suggestions by the time we see the history
    // results, they don't get returned.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420]}]",
      { "term", "a1", "a2", "a3", kNotApplicable, kNotApplicable } },
    // If we only have two suggestions, we have room for a history result.
    { term,
      "[\"term\",[\"a1\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1430, 1410]}]",
      { "term", "a1", "a2", "term2", kNotApplicable, kNotApplicable } },
    // If we have more than three suggestions, they should all be returned as
    // long as we have enough total space for them.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420, 1410]}]",
      { "term", "a1", "a2", "a3", "a4", kNotApplicable } },
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\", \"a5\", \"a6\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\","
                                "\"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420, 1410, 1400, 1390]}]",
      { "term", "a1", "a2", "a3", "a4", "a5" } },
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1430, 1410, 1390, 1370]}]",
      { "term", "a1", "a2", "term2", "a3", "a4" } }
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        cases[i].input, false, cases[i].json, std::string());

    const std::string description = "for input with json=" + cases[i].json;
    const ACMatches& matches = provider_->matches();

    // Ensure no extra matches are present.
    ASSERT_LE(matches.size(), arraysize(cases[i].matches));

    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j)
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]),
                matches[j].contents) << description;
    // Ensure that no expected matches are missing.
    for (; j < arraysize(cases[i].matches); ++j)
      EXPECT_EQ(kNotApplicable, cases[i].matches[j]) <<
          "Case # " << i << " " << description;
  }
}

// Verifies suggest relevance behavior for URL input.
TEST_F(SearchProviderTest, DefaultProviderSuggestRelevanceScoringUrlInput) {
  struct DefaultFetcherUrlInputMatch {
    const std::string match_contents;
    AutocompleteMatch::Type match_type;
    bool allowed_to_be_default_match;
  };
  const DefaultFetcherUrlInputMatch kEmptyMatch =
      { kNotApplicable, AutocompleteMatchType::NUM_TYPES, false };
  struct {
    const std::string input;
    const std::string json;
    const DefaultFetcherUrlInputMatch output[4];
  } cases[] = {
    // clang-format off
    // Ensure NAVIGATION matches are allowed to be listed first for URL input.
    // Non-inlineable matches should not be allowed to be the default match.
    // Note that the top-scoring inlineable match is moved to the top
    // regardless of its score.
    { "a.com", "[\"a.com\",[\"http://b.com/\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "b.com",   AutocompleteMatchType::NAVSUGGEST,            false },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"https://b.com\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "b.com",   AutocompleteMatchType::NAVSUGGEST,            false },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"http://a.com/a\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "a.com/a", AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },

    // Ensure topmost inlineable SUGGEST matches are NOT allowed for URL
    // input.  SearchProvider disregards search and verbatim suggested
    // relevances.
    { "a.com", "[\"a.com\",[\"a.com info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true  },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        false },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"a.com info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true  },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        false },
        kEmptyMatch, kEmptyMatch } },

    // Ensure the fallback mechanism allows inlineable NAVIGATION matches.
    { "a.com", "[\"a.com\",[\"a.com info\", \"http://a.com/b\"],[],[],"
                "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a.com/b",    AutocompleteMatchType::NAVSUGGEST,            true  },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        false },
        { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true  },
        kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"a.com info\", \"http://a.com/b\"],[],[],"
                "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9998, 9997],"
                 "\"google:verbatimrelevance\":9999}]",
      { { "a.com/b",    AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        false },
        kEmptyMatch } },

    // Ensure non-inlineable SUGGEST matches are allowed for URL input
    // assuming the best inlineable match is not a query (i.e., is a
    // NAVSUGGEST).  The best inlineable match will be at the top of the
    // list regardless of its score.
    { "a.com", "[\"a.com\",[\"info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true  },
        { "info",  AutocompleteMatchType::SEARCH_SUGGEST,        false },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true  },
        { "info",  AutocompleteMatchType::SEARCH_SUGGEST,        false },
        kEmptyMatch, kEmptyMatch } },

    // Ensure that if the user explicitly enters a scheme, a navsuggest
    // result for a URL with a different scheme is not inlineable.
    { "http://a.com", "[\"http://a.com\","
               "[\"http://a.com/1\", \"https://a.com/\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9000, 8000]}]",
      { { "http://a.com/1", AutocompleteMatchType::NAVSUGGEST,   true  },
        { "https://a.com", AutocompleteMatchType::NAVSUGGEST,    false },
        { "http://a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                                                                 true  },
        kEmptyMatch } },
    // clang-format on
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      QueryForInputAndWaitForFetcherResponses(
          ASCIIToUTF16(cases[i].input), false, cases[i].json, std::string());
    }

    SCOPED_TRACE("input=" + cases[i].input + " json=" + cases[i].json);
    size_t j = 0;
    const ACMatches& matches = provider_->matches();
    ASSERT_LE(matches.size(), arraysize(cases[i].output));
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].output[j].match_contents),
                matches[j].contents);
      EXPECT_EQ(cases[i].output[j].match_type, matches[j].type);
      EXPECT_EQ(cases[i].output[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
    // Ensure that no expected matches are missing.
    for (; j < arraysize(cases[i].output); ++j) {
      EXPECT_EQ(kNotApplicable, cases[i].output[j].match_contents);
      EXPECT_EQ(AutocompleteMatchType::NUM_TYPES,
                cases[i].output[j].match_type);
      EXPECT_FALSE(cases[i].output[j].allowed_to_be_default_match);
    }
  }
}

// A basic test that verifies the field trial triggered parsing logic.
TEST_F(SearchProviderTest, FieldTrialTriggeredParsing) {
  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "DefaultGroup");
  trial->group();

  QueryForInputAndWaitForFetcherResponses(
      ASCIIToUTF16("foo"), false,
      "[\"foo\",[\"foo bar\"],[\"\"],[],"
      "{\"google:suggesttype\":[\"QUERY\"],"
      "\"google:fieldtrialtriggered\":true}]",
      std::string());

  {
    // Check for the match and field trial triggered bits.
    AutocompleteMatch match;
    EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("foo bar"), &match));
    ProvidersInfo providers_info;
    provider_->AddProviderInfo(&providers_info);
    ASSERT_EQ(1U, providers_info.size());
    EXPECT_EQ(1, providers_info[0].field_trial_triggered_size());
    EXPECT_EQ(1, providers_info[0].field_trial_triggered_in_session_size());
  }
  {
    // Reset the session and check that bits are reset.
    provider_->ResetSession();
    ProvidersInfo providers_info;
    provider_->AddProviderInfo(&providers_info);
    ASSERT_EQ(1U, providers_info.size());
    EXPECT_EQ(0, providers_info[0].field_trial_triggered_size());
    EXPECT_EQ(0, providers_info[0].field_trial_triggered_in_session_size());
  }
}
// A basic test that verifies the specific type identifier parsing logic.
TEST_F(SearchProviderTest, SpecificTypeIdentifierParsing) {
  struct Match {
    std::string contents;
    int subtype_identifier;
  };

  struct {
    const std::string input_text;
    const std::string provider_response_json;
    // The order of the expected matches is not important.
    const Match expected_matches[2];
  } cases[] = {
      // Check that the specific type is set to 0 when these values are not
      // provide in the response.
      {"a",
       "[\"a\",[\"ab\",\"http://b.com\"],[],[], "
       "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"]}]",
       {{"ab", 0}, {"b.com", 0}}},

      // Check that the specific type works for zero-suggest suggestions.
      {"c",
       "[\"c\",[\"cd\",\"http://d.com\"],[],[], "
       "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
       "\"google:subtypeid\":[1, 3]}]",
       {{"cd", 1}, {"d.com", 3}}},

      // Check that the specific type is set to zero when the number of
      // suggestions is smaller than the number of id's provided.
      {"foo",
       "[\"foo\",[\"foo bar\", \"foo baz\"],[],[], "
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
       "\"google:subtypeid\":[1, 2, 3]}]",
       {{"foo bar", 0}, {"foo baz", 0}}},

      // Check that the specific type is set to zero when the number of
      // suggestions is larger than the number of id's provided.
      {"bar",
       "[\"bar\",[\"bar foo\", \"bar foz\"],[],[], "
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
       "\"google:subtypeid\":[1]}]",
       {{"bar foo", 0}, {"bar foz", 0}}},

      // Check that ids stick to their suggestions when these are reordered
      // based on suggestion relevance values.
      {"e",
       "[\"e\",[\"ef\",\"http://e.com\"],[],[], "
       "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9300, 9800],"
       "\"google:subtypeid\":[2, 4]}]",
       {{"ef", 2}, {"e.com", 4}}}};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text), false,
        cases[i].provider_response_json, std::string());

    // Check for the match and field trial triggered bits.
    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());
    for (size_t j = 0; j < arraysize(cases[i].expected_matches); ++j) {
      if (cases[i].expected_matches[j].contents == kNotApplicable)
        continue;
      AutocompleteMatch match;
      EXPECT_TRUE(FindMatchWithContents(
          ASCIIToUTF16(cases[i].expected_matches[j].contents), &match));
      EXPECT_EQ(cases[i].expected_matches[j].subtype_identifier,
                match.subtype_identifier);
    }
  }
}

// Verifies inline autocompletion of navigational results.
TEST_F(SearchProviderTest, NavigationInline) {
  struct {
    const std::string input;
    const std::string url;
    // Test the expected fill_into_edit, which may drop "http://".
    // Some cases do not trim "http://" to match from the start of the scheme.
    const std::string fill_into_edit;
    const std::string inline_autocompletion;
    const bool allowed_to_be_default_match_in_regular_mode;
    const bool allowed_to_be_default_match_in_prevent_inline_mode;
  } cases[] = {
    // Do not inline matches that do not contain the input; trim http as needed.
    { "x",                 "http://www.abc.com",
                                  "www.abc.com",  std::string(), false, false },
    { "https:",            "http://www.abc.com",
                                  "www.abc.com",  std::string(), false, false },
    { "http://www.abc.com/a", "http://www.abc.com",
                              "http://www.abc.com",  std::string(), false,
                                                                    false },

    // Do not inline matches with invalid input prefixes; trim http as needed.
    { "ttp",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "://w",             "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "ww.",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { ".ab",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "bc",               "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { ".com",             "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },

    // Do not inline matches that omit input domain labels; trim http as needed.
    { "www.a",            "http://a.com",
                                 "a.com",       std::string(), false, false },
    { "http://www.a",     "http://a.com",
                          "http://a.com",       std::string(), false, false },
    { "www.a",            "ftp://a.com",
                          "ftp://a.com",        std::string(), false, false },
    { "ftp://www.a",      "ftp://a.com",
                          "ftp://a.com",        std::string(), false, false },

    // Input matching but with nothing to inline will not yield an offset, but
    // will be allowed to be default.
    { "abc.com",             "http://www.abc.com",
                                    "www.abc.com", std::string(), true, true },
    { "http://www.abc.com",  "http://www.abc.com",
                             "http://www.abc.com", std::string(), true, true },

    // Inputs with trailing whitespace should inline when possible.
    { "abc.com ",      "http://www.abc.com",
                              "www.abc.com",      std::string(), true,  true },
    { "abc.com ",      "http://www.abc.com/bar",
                              "www.abc.com/bar",  "/bar",        false, false },

    // Inline matches when the input is a leading substring of the scheme.
    { "h",             "http://www.abc.com",
                       "http://www.abc.com", "ttp://www.abc.com", true, false },
    { "http",          "http://www.abc.com",
                       "http://www.abc.com", "://www.abc.com",    true, false },

    // Inline matches when the input is a leading substring of the full URL.
    { "http:",             "http://www.abc.com",
                           "http://www.abc.com", "//www.abc.com", true, false },
    { "http://w",          "http://www.abc.com",
                           "http://www.abc.com", "ww.abc.com",    true, false },
    { "http://www.",       "http://www.abc.com",
                           "http://www.abc.com", "abc.com",       true, false },
    { "http://www.ab",     "http://www.abc.com",
                           "http://www.abc.com", "c.com",         true, false },
    { "http://www.abc.com/p", "http://www.abc.com/path/file.htm?q=x#foo",
                              "http://www.abc.com/path/file.htm?q=x#foo",
                                                  "ath/file.htm?q=x#foo",
                                                                  true, false },
    { "http://abc.com/p",     "http://abc.com/path/file.htm?q=x#foo",
                              "http://abc.com/path/file.htm?q=x#foo",
                                              "ath/file.htm?q=x#foo",
                                                                  true, false},

    // Inline matches with valid URLPrefixes; only trim "http://".
    { "w",               "http://www.abc.com",
                                "www.abc.com", "ww.abc.com", true, false },
    { "www.a",           "http://www.abc.com",
                                "www.abc.com", "bc.com",     true, false },
    { "abc",             "http://www.abc.com",
                                "www.abc.com", ".com",       true, false },
    { "abc.c",           "http://www.abc.com",
                                "www.abc.com", "om",         true, false },
    { "abc.com/p",       "http://www.abc.com/path/file.htm?q=x#foo",
                                "www.abc.com/path/file.htm?q=x#foo",
                                             "ath/file.htm?q=x#foo",
                                                             true, false },
    { "abc.com/p",       "http://abc.com/path/file.htm?q=x#foo",
                                "abc.com/path/file.htm?q=x#foo",
                                         "ath/file.htm?q=x#foo",
                                                             true, false },

    // Inline matches using the maximal URLPrefix components.
    { "h",               "http://help.com",
                                "help.com", "elp.com",     true, false },
    { "http",            "http://http.com",
                                "http.com", ".com",        true, false },
    { "h",               "http://www.help.com",
                                "www.help.com", "elp.com", true, false },
    { "http",            "http://www.http.com",
                                "www.http.com", ".com",    true, false },
    { "w",               "http://www.www.com",
                                "www.www.com",  "ww.com",  true, false },

    // Test similar behavior for the ftp and https schemes.
    { "ftp://www.ab",  "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                  "c.com/path/file.htm?q=x#foo",  true, false },
    { "www.ab",        "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",            "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",            "ftp://abc.com/path/file.htm?q=x#foo",
                       "ftp://abc.com/path/file.htm?q=x#foo",
                               "c.com/path/file.htm?q=x#foo",     true, false },
    { "https://www.ab",  "https://www.abc.com/path/file.htm?q=x#foo",
                         "https://www.abc.com/path/file.htm?q=x#foo",
                                       "c.com/path/file.htm?q=x#foo",
                                                                  true, false },
    { "www.ab",      "https://www.abc.com/path/file.htm?q=x#foo",
                     "https://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",          "https://www.abc.com/path/file.htm?q=x#foo",
                     "https://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",          "https://abc.com/path/file.htm?q=x#foo",
                     "https://abc.com/path/file.htm?q=x#foo",
                               "c.com/path/file.htm?q=x#foo",     true, false },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // First test regular mode.
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    SearchSuggestionParser::NavigationResult result(
        ChromeAutocompleteSchemeClassifier(&profile_), GURL(cases[i].url),
        AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
        false, 0, false, ASCIIToUTF16(cases[i].input));
    result.set_received_after_last_keystroke(false);
    AutocompleteMatch match(provider_->NavigationToMatch(result));
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              match.inline_autocompletion);
    EXPECT_EQ(ASCIIToUTF16(cases[i].fill_into_edit), match.fill_into_edit);
    EXPECT_EQ(cases[i].allowed_to_be_default_match_in_regular_mode,
              match.allowed_to_be_default_match);

    // Then test prevent-inline-autocomplete mode.
    QueryForInput(ASCIIToUTF16(cases[i].input), true, false);
    SearchSuggestionParser::NavigationResult result_prevent_inline(
        ChromeAutocompleteSchemeClassifier(&profile_), GURL(cases[i].url),
        AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
        false, 0, false, ASCIIToUTF16(cases[i].input));
    result_prevent_inline.set_received_after_last_keystroke(false);
    AutocompleteMatch match_prevent_inline(
        provider_->NavigationToMatch(result_prevent_inline));
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              match_prevent_inline.inline_autocompletion);
    EXPECT_EQ(ASCIIToUTF16(cases[i].fill_into_edit),
              match_prevent_inline.fill_into_edit);
    EXPECT_EQ(cases[i].allowed_to_be_default_match_in_prevent_inline_mode,
              match_prevent_inline.allowed_to_be_default_match);
  }
}

// Verifies that "http://" is not trimmed for input that is a leading substring.
TEST_F(SearchProviderTest, NavigationInlineSchemeSubstring) {
  const base::string16 input(ASCIIToUTF16("ht"));
  const base::string16 url(ASCIIToUTF16("http://a.com"));
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, input);
  result.set_received_after_last_keystroke(false);

  // Check the offset and strings when inline autocompletion is allowed.
  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_inline.fill_into_edit);
  EXPECT_EQ(url.substr(2), match_inline.inline_autocompletion);
  EXPECT_TRUE(match_inline.allowed_to_be_default_match);
  EXPECT_EQ(url, match_inline.contents);

  // Check the same strings when inline autocompletion is prevented.
  QueryForInput(input, true, false);
  AutocompleteMatch match_prevent(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_prevent.fill_into_edit);
  EXPECT_FALSE(match_prevent.allowed_to_be_default_match);
  EXPECT_EQ(url, match_prevent.contents);
}

// Verifies that input "w" marks a more significant domain label than "www.".
TEST_F(SearchProviderTest, NavigationInlineDomainClassify) {
  QueryForInput(ASCIIToUTF16("w"), false, false);
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL("http://www.wow.com"),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, ASCIIToUTF16("w"));
  result.set_received_after_last_keystroke(false);
  AutocompleteMatch match(provider_->NavigationToMatch(result));
  EXPECT_EQ(ASCIIToUTF16("ow.com"), match.inline_autocompletion);
  EXPECT_TRUE(match.allowed_to_be_default_match);
  EXPECT_EQ(ASCIIToUTF16("www.wow.com"), match.fill_into_edit);
  EXPECT_EQ(ASCIIToUTF16("wow.com"), match.contents);

  // Ensure that the match for input "w" is marked on "wow" and not "www".
  ASSERT_EQ(2U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL |
                AutocompleteMatch::ACMatchClassification::MATCH,
            match.contents_class[0].style);
  EXPECT_EQ(1U, match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[1].style);
}

// Verifies that "http://" is trimmed in the general case.
TEST_F(SearchProviderTest, DoTrimHttpScheme) {
  const base::string16 input(ASCIIToUTF16("face book"));
  const base::string16 url(ASCIIToUTF16("http://www.facebook.com"));
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(ASCIIToUTF16("facebook.com"), match_inline.contents);
}

// Verifies that "http://" is not trimmed for input that has a scheme, even if
// the input doesn't match the URL.
TEST_F(SearchProviderTest, DontTrimHttpSchemeIfInputHasScheme) {
  const base::string16 input(ASCIIToUTF16("https://face book"));
  const base::string16 url(ASCIIToUTF16("http://www.facebook.com"));
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(ASCIIToUTF16("http://facebook.com"), match_inline.contents);
}

// Verifies that "https://" is not trimmed for input that has a (non-matching)
// scheme.
TEST_F(SearchProviderTest, DontTrimHttpsSchemeIfInputHasScheme) {
  const base::string16 input(ASCIIToUTF16("http://face book"));
  const base::string16 url(ASCIIToUTF16("https://www.facebook.com"));
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(ASCIIToUTF16("https://facebook.com"), match_inline.contents);
}

// Verifies that "https://" is trimmed in the general case.
TEST_F(SearchProviderTest, DoTrimHttpsScheme) {
  const base::string16 input(ASCIIToUTF16("face book"));
  const base::string16 url(ASCIIToUTF16("https://www.facebook.com"));
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST, 0, base::string16(), std::string(),
      false, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(ASCIIToUTF16("facebook.com"), match_inline.contents);
}

#if !defined(OS_WIN)
// Verify entity suggestion parsing.
TEST_F(SearchProviderTest, ParseEntitySuggestion) {
  struct Match {
    std::string contents;
    std::string description;
    std::string query_params;
    std::string fill_into_edit;
    AutocompleteMatchType::Type type;
  };
  const Match kEmptyMatch = {
    kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable,
    AutocompleteMatchType::NUM_TYPES};

  struct {
    const std::string input_text;
    const std::string response_json;
    const Match matches[5];
  } cases[] = {
    // A query and an entity suggestion with different search terms.
    { "x",
      "[\"x\",[\"xy\", \"yy\"],[\"\",\"\"],[],"
      " {\"google:suggestdetail\":[{},"
      "   {\"a\":\"A\",\"t\":\"xy\",\"q\":\"p=v\"}],"
      "\"google:suggesttype\":[\"QUERY\",\"ENTITY\"]}]",
      { { "x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST },
        { "xy", "A", "p=v", "yy",
          AutocompleteMatchType::SEARCH_SUGGEST_ENTITY },
        kEmptyMatch,
        kEmptyMatch
      },
    },
    // A query and an entity suggestion with same search terms.
    { "x",
      "[\"x\",[\"xy\", \"xy\"],[\"\",\"\"],[],"
      " {\"google:suggestdetail\":[{},"
      "   {\"a\":\"A\",\"t\":\"xy\",\"q\":\"p=v\"}],"
      "\"google:suggesttype\":[\"QUERY\",\"ENTITY\"]}]",
      { { "x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST },
        { "xy", "A", "p=v", "xy",
          AutocompleteMatchType::SEARCH_SUGGEST_ENTITY },
        kEmptyMatch,
        kEmptyMatch
      },
    },
  };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text), false, cases[i].response_json,
        std::string());

    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());

    SCOPED_TRACE("for input with json = " + cases[i].response_json);

    ASSERT_LE(matches.size(), arraysize(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      const Match& match = cases[i].matches[j];
      SCOPED_TRACE(" and match index: " + base::NumberToString(j));
      EXPECT_EQ(match.contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(match.description,
                base::UTF16ToUTF8(matches[j].description));
      EXPECT_EQ(match.query_params,
                matches[j].search_terms_args->additional_query_params);
      EXPECT_EQ(match.fill_into_edit,
                base::UTF16ToUTF8(matches[j].fill_into_edit));
      EXPECT_EQ(match.type, matches[j].type);
    }
    // Ensure that no expected matches are missing.
    for (; j < arraysize(cases[i].matches); ++j) {
      SCOPED_TRACE(" and match index: " + base::NumberToString(j));
      EXPECT_EQ(cases[i].matches[j].contents, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].description, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].query_params, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].fill_into_edit, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].type, AutocompleteMatchType::NUM_TYPES);
    }
  }
}
#endif  // !defined(OS_WIN)


// A basic test that verifies the prefetch metadata parsing logic.
TEST_F(SearchProviderTest, PrefetchMetadataParsing) {
  struct Match {
    std::string contents;
    bool allowed_to_be_prefetched;
    AutocompleteMatchType::Type type;
    bool from_keyword;
  };
  const Match kEmptyMatch = { kNotApplicable,
                              false,
                              AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                              false };

  struct {
    const std::string input_text;
    bool prefer_keyword_provider_results;
    const std::string default_provider_response_json;
    const std::string keyword_provider_response_json;
    const Match matches[5];
  } cases[] = {
      // Default provider response does not have prefetch details. Ensure that
      // the suggestions are not marked as prefetch query.
      {
          "a",
          false,
          "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
          std::string(),
          {{"a", false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false},
           {"c", false, AutocompleteMatchType::SEARCH_SUGGEST, false},
           {"b", false, AutocompleteMatchType::SEARCH_SUGGEST, false},
           kEmptyMatch,
           kEmptyMatch},
      },
      // Ensure that default provider suggest response prefetch details are
      // parsed and recorded in AutocompleteMatch.
      {
          "ab",
          false,
          "[\"ab\",[\"abc\", \"http://b.com\", \"http://c.com\"],[],[],"
          "{\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\", \"NAVIGATION\"],"
          "\"google:suggestrelevance\":[999, 12, 1]}]",
          std::string(),
          {{"ab", false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false},
           {"abc", true, AutocompleteMatchType::SEARCH_SUGGEST, false},
           {"b.com", false, AutocompleteMatchType::NAVSUGGEST, false},
           {"c.com", false, AutocompleteMatchType::NAVSUGGEST, false},
           kEmptyMatch},
      },
      // Default provider suggest response has prefetch details.
      // SEARCH_WHAT_YOU_TYPE suggestion outranks SEARCH_SUGGEST suggestion for
      // the same query string. Ensure that the prefetch details from
      // SEARCH_SUGGEST match are set onto SEARCH_WHAT_YOU_TYPE match.
      {
          "ab",
          false,
          "[\"ab\",[\"ab\", \"http://ab.com\"],[],[],"
          "{\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
          "\"google:suggestrelevance\":[99, 98]}]",
          std::string(),
          {{"ab", true, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false},
           {"ab.com", false, AutocompleteMatchType::NAVSUGGEST, false},
           kEmptyMatch,
           kEmptyMatch,
           kEmptyMatch},
      },
      // Default provider response has prefetch details. We prefer keyword
      // provider results. Ensure that prefetch bit for a suggestion from the
      // default search provider does not get copied onto a higher-scoring match
      // for the same query string from the keyword provider.
      {
          "k a",
          true,
          "[\"k a\",[\"a\", \"ab\"],[],[], {\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
          "\"google:suggestrelevance\":[9, 12]}]",
          "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
          {{"a", false, AutocompleteMatchType::SEARCH_OTHER_ENGINE, true},
           {"k a", false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false},
           {"ab", false, AutocompleteMatchType::SEARCH_SUGGEST, false},
           {"c", false, AutocompleteMatchType::SEARCH_SUGGEST, true},
           {"b", false, AutocompleteMatchType::SEARCH_SUGGEST, true}},
      }};

  for (size_t i = 0; i < arraysize(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text),
        cases[i].prefer_keyword_provider_results,
        cases[i].default_provider_response_json,
        cases[i].prefer_keyword_provider_results ?
            cases[i].keyword_provider_response_json : std::string());

    const std::string description =
        "for input with json =" + cases[i].default_provider_response_json;
    const ACMatches& matches = provider_->matches();
    // The top match must inline and score as highly as calculated verbatim.
    ASSERT_FALSE(matches.empty());
    EXPECT_GE(matches[0].relevance, 1300);

    ASSERT_LE(matches.size(), arraysize(cases[i].matches));
    // Ensure that the returned matches equal the expectations.
    for (size_t j = 0; j < matches.size(); ++j) {
      SCOPED_TRACE(description);
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_prefetched,
                SearchProvider::ShouldPrefetch(matches[j]));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
      EXPECT_EQ(cases[i].matches[j].from_keyword,
                matches[j].keyword == ASCIIToUTF16("k"));
    }
  }
}

TEST_F(SearchProviderTest, XSSIGuardedJSONParsing_InvalidResponse) {
  ClearAllResults();

  std::string input_str("abc");
  QueryForInputAndWaitForFetcherResponses(
      ASCIIToUTF16(input_str), false, "this is a bad non-json response",
      std::string());

  const ACMatches& matches = provider_->matches();

  // Should have exactly one "search what you typed" match
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(input_str, base::UTF16ToUTF8(matches[0].contents));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            matches[0].type);
}

// A basic test that verifies that the XSSI guarded JSON response is parsed
// correctly.
TEST_F(SearchProviderTest, XSSIGuardedJSONParsing_ValidResponses) {
  struct Match {
    std::string contents;
    AutocompleteMatchType::Type type;
  };
  const Match kEmptyMatch = {
      kNotApplicable, AutocompleteMatchType::NUM_TYPES
  };

  struct {
    const std::string input_text;
    const std::string default_provider_response_json;
    const Match matches[4];
  } cases[] = {
    // No XSSI guard.
    { "a",
      "[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
    // Standard XSSI guard - )]}'\n.
    { "a",
      ")]}'\n[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
    // Modified XSSI guard - contains "[".
    { "a",
      ")]}'\n[)\"[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text), false,
        cases[i].default_provider_response_json, std::string());

    const ACMatches& matches = provider_->matches();
    // The top match must inline and score as highly as calculated verbatim.
    ASSERT_FALSE(matches.empty());
    EXPECT_GE(matches[0].relevance, 1300);

    SCOPED_TRACE("for case: " + base::NumberToString(i));
    ASSERT_LE(matches.size(), arraysize(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      SCOPED_TRACE("and match: " + base::NumberToString(j));
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
    }
    for (; j < arraysize(cases[i].matches); ++j) {
      SCOPED_TRACE("and match: " + base::NumberToString(j));
      EXPECT_EQ(cases[i].matches[j].contents, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].type, AutocompleteMatchType::NUM_TYPES);
    }
  }
}

// Test that deletion url gets set on an AutocompleteMatch when available for a
// personalized query or a personalized URL.
TEST_F(SearchProviderTest, ParseDeletionUrl) {
  struct Match {
    std::string contents;
    std::string deletion_url;
    AutocompleteMatchType::Type type;
  };

  const Match kEmptyMatch = {kNotApplicable, std::string(),
                             AutocompleteMatchType::NUM_TYPES};

  const char* url[] = {
      "http://defaultturl/complete/deleteitems"
      "?delq=ab&client=chrome&deltok=xsrf124",
      "http://defaultturl/complete/deleteitems"
      "?delq=www.amazon.com&client=chrome&deltok=xsrf123",
  };

  struct {
    const std::string input_text;
    const std::string response_json;
    const Match matches[5];
  } cases[] = {
      // clang-format off
      // A deletion URL on a personalized query should be reflected in the
      // resulting AutocompleteMatch.
      { "a",
        "[\"a\",[\"ab\", \"ac\",\"www.amazon.com\"],[],[],"
        "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
         "\"PERSONALIZED_NAVIGATION\"],"
        "\"google:suggestrelevance\":[3, 2, 1],"
        "\"google:suggestdetail\":[{\"du\":"
        "\"/complete/deleteitems?delq=ab&client=chrome"
         "&deltok=xsrf124\"}, {}, {\"du\":"
        "\"/complete/deleteitems?delq=www.amazon.com&"
        "client=chrome&deltok=xsrf123\"}]}]",
        { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
          { "ab", url[0], AutocompleteMatchType::SEARCH_SUGGEST },
          { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
          { "amazon.com", url[1],
             AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
          kEmptyMatch,
        },
      },
      // Personalized queries or a personalized URL without deletion URLs
      // shouldn't cause errors.
      { "a",
        "[\"a\",[\"ab\", \"ac\"],[],[],"
        "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
        "\"PERSONALIZED_NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2],"
        "\"google:suggestdetail\":[{}, {}]}]",
        { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
          { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
          { "ab", "", AutocompleteMatchType::SEARCH_SUGGEST },
          { "amazon.com", "",
             AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
          kEmptyMatch,
        },
      },
      // Personalized queries or a personalized URL without
      // google:suggestdetail shouldn't cause errors.
      { "a",
        "[\"a\",[\"ab\", \"ac\"],[],[],"
        "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
        "\"PERSONALIZED_NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2]}]",
        { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
          { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
          { "ab", "", AutocompleteMatchType::SEARCH_SUGGEST },
          { "amazon.com", "",
             AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
          kEmptyMatch,
        },
      },
      // clang-format on
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(ASCIIToUTF16(cases[i].input_text),
                                            false, cases[i].response_json,
                                            std::string());

    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());

    SCOPED_TRACE("for input with json = " + cases[i].response_json);

    for (size_t j = 0; j < matches.size(); ++j) {
      const Match& match = cases[i].matches[j];
      SCOPED_TRACE(" and match index: " + base::NumberToString(j));
      EXPECT_EQ(match.contents, base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(match.deletion_url,
                matches[j].GetAdditionalInfo("deletion_url"));
    }
  }
}

TEST_F(SearchProviderTest, CanSendURL) {
  TemplateURLData template_url_data;
  template_url_data.SetShortName(ASCIIToUTF16("t"));
  template_url_data.SetURL("http://www.google.com/{searchTerms}");
  template_url_data.suggestions_url = "http://www.google.com/{searchTerms}";
  template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(template_url_data);

  // All conditions should be met.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));

  // Invalid page URL.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("badpageurl"), GURL("https://www.google.com/complete/search"),
      &google_template_url, metrics::OmniboxEventProto::OTHER,
      SearchTermsData(), client_.get()));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS,
      SearchTermsData(), client_.get()));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      SearchTermsData(), client_.get()));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::NTP, SearchTermsData(), client_.get()));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OBSOLETE_INSTANT_NTP, SearchTermsData(),
      client_.get()));

  // HTTPS page URL on same domain as provider.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("https://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));

  // Non-HTTP[S] page URL on same domain as provider.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("ftp://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));

  // Non-HTTP page URL on different domain.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("https://www.notgoogle.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));

  // Non-HTTPS provider.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("http://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));

  // Suggest disabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));
  profile_.GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  // Incognito.
  ChromeAutocompleteProviderClient client_incognito(
      profile_.GetOffTheRecordProfile());
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(),
      &client_incognito));

  // Personalized URL data collection not active.
  client_->set_is_personalized_url_data_collection_active(false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));
  client_->set_is_personalized_url_data_collection_active(true);

  // Check that there were no side effects from previous tests.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), client_.get()));
}

TEST_F(SearchProviderTest, TestDeleteMatch) {
  const char kDeleteUrl[] = "https://www.google.com/complete/deleteitem?q=foo";
  AutocompleteMatch match(
      provider_.get(), 0, true, AutocompleteMatchType::SEARCH_SUGGEST);
  match.RecordAdditionalInfo(SearchProvider::kDeletionUrlKey, kDeleteUrl);

  // Test a successful deletion request.
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_handlers_.empty());
  EXPECT_TRUE(provider_->matches_.empty());

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kDeleteUrl));
  test_url_loader_factory_.AddResponse(kDeleteUrl, "");

  // Need to spin the event loop to let the fetch result go through.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->deletion_handlers_.empty());
  EXPECT_TRUE(provider_->is_success());

  // Test a failing deletion request.
  test_url_loader_factory_.ClearResponses();
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_handlers_.empty());
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kDeleteUrl));

  network::ResourceResponseHead head;
  std::string headers("HTTP/1.1 500 Owiee\nContent-type: application/json\n\n");
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
  head.mime_type = "application/json";
  test_url_loader_factory_.AddResponse(GURL(kDeleteUrl), head, "",
                                       network::URLLoaderCompletionStatus());

  profile_.BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(provider_->deletion_handlers_.empty());
  EXPECT_FALSE(provider_->is_success());
}

TEST_F(SearchProviderTest, TestDeleteHistoryQueryMatch) {
  GURL term_url(
      AddSearchToHistory(default_t_url_, ASCIIToUTF16("flash games"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch games;
  QueryForInput(ASCIIToUTF16("fla"), false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(ASCIIToUTF16("fla")));
  ASSERT_TRUE(FindMatchWithContents(ASCIIToUTF16("flash games"), &games));

  size_t matches_before = provider_->matches().size();
  provider_->DeleteMatch(games);
  EXPECT_EQ(matches_before - 1, provider_->matches().size());

  // Process history deletions.
  profile_.BlockUntilHistoryProcessesPendingRequests();

  // Check that the match is gone.
  test_url_loader_factory_.ClearResponses();
  QueryForInput(ASCIIToUTF16("fla"), false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(ASCIIToUTF16("fla")));
  EXPECT_FALSE(FindMatchWithContents(ASCIIToUTF16("flash games"), &games));
}

// Verifies that duplicates are preserved in AddMatchToMap().
TEST_F(SearchProviderTest, CheckDuplicateMatchesSaved) {
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("a"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("alpha"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("avid"), 1);

  profile_.BlockUntilHistoryProcessesPendingRequests();
  QueryForInputAndWaitForFetcherResponses(
      ASCIIToUTF16("a"), false,
      "[\"a\",[\"a\", \"alpha\", \"avid\", \"apricot\"],[],[],"
      "{\"google:suggestrelevance\":[1450, 1200, 1150, 1100],"
      "\"google:verbatimrelevance\":1350}]",
      std::string());

  AutocompleteMatch verbatim, match_alpha, match_apricot, match_avid;
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a"), &verbatim));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("alpha"), &match_alpha));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("apricot"), &match_apricot));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("avid"), &match_avid));

  // Verbatim match duplicates are added such that each one has a higher
  // relevance than the previous one.
  EXPECT_EQ(2U, verbatim.duplicate_matches.size());

  // Other match duplicates are added in descending relevance order.
  EXPECT_EQ(1U, match_alpha.duplicate_matches.size());
  EXPECT_EQ(1U, match_avid.duplicate_matches.size());

  EXPECT_EQ(0U, match_apricot.duplicate_matches.size());
}

TEST_F(SearchProviderTest, SuggestQueryUsesToken) {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("default"));
  data.SetKeyword(data.short_name());
  data.SetURL("http://example/{searchTerms}{google:sessionToken}");
  data.suggestions_url =
      "http://suggest/?q={searchTerms}&{google:sessionToken}";
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // And the URL matches what we expected.
  TemplateURLRef::SearchTermsArgs search_terms_args(term);
  search_terms_args.session_token = provider_->current_token_;
  std::string expected_url(
      default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          search_terms_args, turl_model->search_terms_data()));

  // Make sure the default provider's suggest service was queried.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url));

  // Complete running the fetcher to clean up.
  test_url_loader_factory_.AddResponse(expected_url, "");
  RunTillProviderDone();
}

TEST_F(SearchProviderTest, SessionToken) {
  // Subsequent calls always get the same token.
  std::string token = provider_->GetSessionToken();
  std::string token2 = provider_->GetSessionToken();
  EXPECT_EQ(token, token2);
  EXPECT_FALSE(token.empty());

  // Calls do not regenerate a token.
  provider_->current_token_ = "PRE-EXISTING TOKEN";
  token = provider_->GetSessionToken();
  EXPECT_EQ(token, "PRE-EXISTING TOKEN");

  // ... unless the token has expired.
  provider_->current_token_.clear();
  const base::TimeDelta kSmallDelta = base::TimeDelta::FromMilliseconds(1);
  provider_->token_expiration_time_ = base::TimeTicks::Now() - kSmallDelta;
  token = provider_->GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, provider_->current_token_);

  // The expiration time is always updated.
  provider_->GetSessionToken();
  base::TimeTicks expiration_time_1 = provider_->token_expiration_time_;
  base::PlatformThread::Sleep(kSmallDelta);
  provider_->GetSessionToken();
  base::TimeTicks expiration_time_2 = provider_->token_expiration_time_;
  EXPECT_GT(expiration_time_2, expiration_time_1);
  EXPECT_GE(expiration_time_2, expiration_time_1 + kSmallDelta);
}

TEST_F(SearchProviderTest, AnswersCache) {
  AutocompleteResult result;
  ACMatches matches;
  AutocompleteMatch match1;
  match1.answer_contents = base::ASCIIToUTF16("m1");
  match1.answer_type = base::ASCIIToUTF16("2334");
  match1.fill_into_edit = base::ASCIIToUTF16("weather los angeles");

  AutocompleteMatch non_answer_match1;
  non_answer_match1.fill_into_edit = base::ASCIIToUTF16("weather laguna beach");

  // Test that an answer in the first slot populates the cache.
  matches.push_back(match1);
  matches.push_back(non_answer_match1);
  result.AppendMatches(AutocompleteInput(), matches);
  provider_->RegisterDisplayedAnswers(result);
  ASSERT_FALSE(provider_->answers_cache_.empty());

  // Without scored results, no answers will be retrieved.
  AnswersQueryData answer = provider_->FindAnswersPrefetchData();
  EXPECT_TRUE(answer.full_query_text.empty());
  EXPECT_TRUE(answer.query_type.empty());

  // Inject a scored result, which will trigger answer retrieval.
  base::string16 query = base::ASCIIToUTF16("weather los angeles");
  SearchSuggestionParser::SuggestResult suggest_result(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*subtype_identifier=*/0, /*from_keyword_provider=*/false,
      /*relevance=*/1200, /*relevance_from_server=*/false,
      /*input_text=*/query);
  QueryForInput(ASCIIToUTF16("weather l"), false, false);
  provider_->transformed_default_history_results_.push_back(suggest_result);
  answer = provider_->FindAnswersPrefetchData();
  EXPECT_EQ(base::ASCIIToUTF16("weather los angeles"), answer.full_query_text);
  EXPECT_EQ(base::ASCIIToUTF16("2334"), answer.query_type);
}

TEST_F(SearchProviderTest, RemoveExtraAnswers) {
  SuggestionAnswer answer1;
  answer1.set_type(42);
  SuggestionAnswer answer2;
  answer2.set_type(1983);
  SuggestionAnswer answer3;
  answer3.set_type(423);

  ACMatches matches;
  AutocompleteMatch match1, match2, match3, match4, match5;
  match1.answer = answer1;
  match1.answer_contents = base::ASCIIToUTF16("the answer");
  match1.answer_type = base::ASCIIToUTF16("42");
  match3.answer = answer2;
  match3.answer_contents = base::ASCIIToUTF16("not to play");
  match3.answer_type = base::ASCIIToUTF16("1983");
  match5.answer = answer3;
  match5.answer_contents = base::ASCIIToUTF16("a person");
  match5.answer_type = base::ASCIIToUTF16("423");

  matches.push_back(match1);
  matches.push_back(match2);
  matches.push_back(match3);
  matches.push_back(match4);
  matches.push_back(match5);

  SearchProvider::RemoveExtraAnswers(&matches);
  EXPECT_EQ(base::ASCIIToUTF16("the answer"), matches[0].answer_contents);
  EXPECT_EQ(base::ASCIIToUTF16("42"), matches[0].answer_type);
  EXPECT_TRUE(answer1.Equals(*matches[0].answer));
  EXPECT_TRUE(matches[1].answer_contents.empty());
  EXPECT_TRUE(matches[1].answer_type.empty());
  EXPECT_FALSE(matches[1].answer);
  EXPECT_TRUE(matches[2].answer_contents.empty());
  EXPECT_TRUE(matches[2].answer_type.empty());
  EXPECT_FALSE(matches[2].answer);
  EXPECT_TRUE(matches[3].answer_contents.empty());
  EXPECT_TRUE(matches[3].answer_type.empty());
  EXPECT_FALSE(matches[3].answer);
  EXPECT_TRUE(matches[4].answer_contents.empty());
  EXPECT_TRUE(matches[4].answer_type.empty());
  EXPECT_FALSE(matches[4].answer);
}

TEST_F(SearchProviderTest, DoesNotProvideOnFocus) {
  AutocompleteInput input(base::ASCIIToUTF16("f"),
                          metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  input.set_prefer_keyword(true);
  input.set_from_omnibox_focus(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

// SearchProviderWarmUpTest --------------------------------------------------
//
// Like SearchProviderTest.  The only addition is that it's a
// TestWithParam<bool>, where the boolean parameter represents whether the
// omnibox::kSearchProviderWarmUpOnFocus feature flag should be enabled.
class SearchProviderWarmUpTest : public SearchProviderTest,
                                 public testing::WithParamInterface<bool> {
 public:
  SearchProviderWarmUpTest() {}
  void SetUp() override;

 protected:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderWarmUpTest);
};

void SearchProviderWarmUpTest::SetUp() {
  if (GetParam())
    feature_list_.InitAndEnableFeature(omnibox::kSearchProviderWarmUpOnFocus);
  else
    feature_list_.InitAndDisableFeature(omnibox::kSearchProviderWarmUpOnFocus);
  SearchProviderTest::SetUp();
}

#if defined(THREAD_SANITIZER)
// SearchProviderTest.SendsWarmUpRequestOnFocus is flaky on Linux TSan Tests
// crbug.com/891959.
#define MAYBE_SendsWarmUpRequestOnFocus DISABLED_SendsWarmUpRequestOnFocus
#else
#define MAYBE_SendsWarmUpRequestOnFocus SendsWarmUpRequestOnFocus
#endif  // defined(THREAD_SANITIZER)
TEST_P(SearchProviderWarmUpTest, MAYBE_SendsWarmUpRequestOnFocus) {
  AutocompleteInput input(base::ASCIIToUTF16("f"),
                          metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  input.set_prefer_keyword(true);
  input.set_from_omnibox_focus(true);

  if (!GetParam()) {  // The warm-up feature ought to be disabled.
    // The provider immediately terminates with no matches.
    provider_->Start(input, false);
    // RunUntilIdle so that SearchProvider has a chance to create the
    // URLFetchers (if it wants to, which it shouldn't in this case).
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(provider_->matches().empty());
  } else {  // The warm-up feature ought to be enabled.
    provider_->Start(input, false);
    // RunUntilIdle so that SearchProvider create the URLFetcher.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(provider_->done());
    EXPECT_TRUE(provider_->matches().empty());
    // Make sure the default provider's suggest service was queried with an
    // empty query.
    EXPECT_TRUE(test_url_loader_factory_.IsPending("http://defaultturl2/"));
    // Even if the fetcher returns results, we should still have no suggestions
    // (though the provider should now be done).
    test_url_loader_factory_.AddResponse("http://defaultturl2/",
                                         R"(["",["a", "b"],[],[],{}])");
    RunTillProviderDone();
    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(provider_->matches().empty());
  }
}

INSTANTIATE_TEST_CASE_P(SearchProviderTest,
                        SearchProviderWarmUpTest,
                        testing::Values(false, true));
