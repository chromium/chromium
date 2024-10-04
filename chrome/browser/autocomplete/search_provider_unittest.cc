// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/search_provider.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/document_suggestions_service_factory.h"
#include "chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/navigational_intent.pb.h"
#include "ui/base/device_form_factor.h"

using base::ASCIIToUTF16;
using testing::_;

namespace {

// Returns the first match in |matches| with |allowed_to_be_default_match|
// set to true.
ACMatches::const_iterator FindDefaultMatch(const ACMatches& matches) {
  auto it = matches.begin();
  while ((it != matches.end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

class SearchProviderForTest : public SearchProvider {
 public:
  SearchProviderForTest(AutocompleteProviderClient* client,
                        AutocompleteProviderListener* listener,
                        Profile* profile);
  SearchProviderForTest(const SearchProviderForTest&) = delete;
  SearchProviderForTest& operator=(const SearchProviderForTest&) = delete;
  bool is_success() const { return is_success_; }

 protected:
  ~SearchProviderForTest() override;

 private:
  void RecordDeletionResult(bool success) override;
  bool is_success_ = false;
};

SearchProviderForTest::SearchProviderForTest(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    Profile* profile)
    : SearchProvider(client, listener), is_success_(false) {}

SearchProviderForTest::~SearchProviderForTest() = default;

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

std::unique_ptr<KeyedService> BuildRemoteSuggestionsServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  return std::make_unique<RemoteSuggestionsService>(
      DocumentSuggestionsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context), /*create_if_necessary=*/true),
      test_url_loader_factory->GetSafeWeakWrapper());
}

std::string SerializeAndEncodeEntityInfo(
    const omnibox::EntityInfo& entity_info) {
  std::string serialized_entity_info;
  entity_info.SerializeToString(&serialized_entity_info);
  return base::Base64Encode(serialized_entity_info);
}

}  // namespace

// SearchProviderFeatureTestComponent -----------------------------------------
// Handles field trial, feature flag, and command line state for SearchProvider
// tests. This is done as a base class, so that it runs before
// BrowserTaskEnvironment is initialized.

class SearchProviderFeatureTestComponent {
 public:
  explicit SearchProviderFeatureTestComponent(
      const bool command_line_overrides);

  ~SearchProviderFeatureTestComponent() {
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

SearchProviderFeatureTestComponent::SearchProviderFeatureTestComponent(
    const bool command_line_overrides) {
  if (command_line_overrides) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kGoogleBaseURL, "http://www.bar.com/");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kExtraSearchQueryParams, "a=b");
  }
}

// BaseSearchProviderTest -----------------------------------------------------

// Base class that configures following environment:
// . The TemplateURL default_t_url_ is set as the default provider.
// . The TemplateURL keyword_t_url_ is added to the TemplateURLService.
//   TemplateURL values are set by subclasses. Most tests use SearchProviderTest
//   with valid ones.
// . The URL created by using the search term term1_ with default_t_url_ is
//   added to history.
// . The URL created by using the search term keyword_term_ with keyword_t_url_
//   is added to history.
// . test_url_loader_factory_ is set as the URLLoaderFactory.
//
// Most tests use SearchProviderTest subclass, see below.
class BaseSearchProviderTest : public testing::Test,
                               public AutocompleteProviderListener {
 public:
  struct ResultInfo {
    ResultInfo() : result_type(AutocompleteMatchType::NUM_TYPES),
                   allowed_to_be_default_match(false) {
    }
    ResultInfo(GURL gurl,
               AutocompleteMatch::Type result_type,
               bool allowed_to_be_default_match,
               std::u16string fill_into_edit)
        : gurl(gurl),
          result_type(result_type),
          allowed_to_be_default_match(allowed_to_be_default_match),
          fill_into_edit(fill_into_edit) {}

    const GURL gurl;
    const AutocompleteMatch::Type result_type;
    const bool allowed_to_be_default_match;
    const std::u16string fill_into_edit;
  };

  struct TestData {
    const std::u16string input;
    const size_t num_results;
    const ResultInfo output[3];
  };

  struct ExpectedMatch {
    std::string contents;
    bool allowed_to_be_default_match;
  };

  explicit BaseSearchProviderTest(const bool command_line_overrides = false)
      : feature_test_component_(command_line_overrides) {
    // We need the history service, the template url model, and the signin
    // client and the remote suggestions service initialized with a
    // TestURLLoaderFactory.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            &test_url_loader_factory_));
    profile_builder.AddTestingFactory(
        RemoteSuggestionsServiceFactory::GetInstance(),
        base::BindRepeating(&BuildRemoteSuggestionsServiceWithURLLoader,
                            &test_url_loader_factory_));
    profile_ = profile_builder.Build();
  }

  BaseSearchProviderTest(const BaseSearchProviderTest&) = delete;
  BaseSearchProviderTest& operator=(const BaseSearchProviderTest&) = delete;

  void TearDown() override;

  void RunTest(TestData* cases, int num_cases, bool prefer_keyword);

 protected:
  // Default values used for testing.
  static const char kNotApplicable[];
  static const ExpectedMatch kEmptyExpectedMatch;

  // Adds a search for |term|, using the engine |t_url| to the history, and
  // returns the URL for that search.
  GURL AddSearchToHistory(TemplateURL* t_url,
                          std::u16string term,
                          int visit_count);

  // Used in SetUp in subclasses. See description above this class about common
  // settings that this method sets up.
  void CustomizableSetUp(const std::string& search_url,
                         const std::string& suggestions_url);

  // Looks for a match in |provider_| with |contents| equal to |contents|.
  // Sets |match| to it if found.  Returns whether |match| was set.
  bool FindMatchWithContents(const std::u16string& contents,
                             AutocompleteMatch* match);

  // Looks for a match in |provider_| with destination |url|.  Sets |match| to
  // it if found.  Returns whether |match| was set.
  bool FindMatchWithDestination(const GURL& url, AutocompleteMatch* match);

  // AutocompleteProviderListener:
  // If we're waiting for the provider to finish, this exits the message loop.
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  // Runs a nested run loop until provider_ is done. The message loop is
  // exited by way of OnProviderUpdate.
  void RunTillProviderDone();

  // Invokes Start on provider_, then runs all pending tasks.
  void QueryForInput(const AutocompleteInput& input);

  // Invokes Start on provider_, then runs all pending tasks.
  void QueryForInput(const std::u16string& text,
                     bool prevent_inline_autocomplete,
                     bool prefer_keyword);

  // Calls QueryForInput(), finishes any suggest query, then if |wyt_match| is
  // not nullptr, sets it to the "what you typed" entry for |text|.
  void QueryForInputAndSetWYTMatch(const std::u16string& text,
                                   AutocompleteMatch* wyt_match);

  // Calls QueryForInput(), sets the JSON responses for the default and keyword
  // fetchers, and waits until the responses have been returned and the matches
  // returned.  Use empty responses for each fetcher that shouldn't be set up /
  // configured.
  void QueryForInputAndWaitForFetcherResponses(
      const std::u16string& text,
      const bool prefer_keyword,
      const std::string& default_fetcher_response,
      const std::string& keyword_fetcher_response);

  // Notifies the URLFetcher for the suggest query corresponding to the default
  // search provider that it's done.
  // Be sure and wrap calls to this in ASSERT_NO_FATAL_FAILURE.
  void FinishDefaultSuggestQuery(const std::u16string& query_text);

  // Verifies that |matches| and |expected_matches| agree on the first
  // |num_expected_matches|, displaying an error message that includes
  // |description| for any disagreement.
  void CheckMatches(const std::string& description,
                    const size_t num_expected_matches,
                    const ExpectedMatch expected_matches[],
                    const ACMatches& matches);

  void ClearAllResults();

  // SearchProviderFeatureTestComponent must come before BrowserTaskEnvironment,
  // to avoid a possible race.
  SearchProviderFeatureTestComponent feature_test_component_;
  content::BrowserTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestAutocompleteProviderClient> client_;
  scoped_refptr<SearchProviderForTest> provider_;

  // See description above class for details of these fields.
  // TemplateURLs can not outlive `profile_`.
  raw_ptr<TemplateURL> default_t_url_ = nullptr;
  const std::u16string term1_ = u"term1";
  GURL term1_url_;
  raw_ptr<TemplateURL> keyword_t_url_ = nullptr;
  const std::u16string keyword_term_ = u"keyword";
  GURL keyword_url_;

  // If not nullptr, OnProviderUpdate quits the current |run_loop_|.
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

// SearchProviderTest ---------------------------------------------------------

// Test environment with valid suggest and search URL.
class SearchProviderTest : public BaseSearchProviderTest {
 public:
  explicit SearchProviderTest(const bool command_line_overrides = false)
      : BaseSearchProviderTest(command_line_overrides) {}

  void SetUp() override {
    CustomizableSetUp(
        /* search_url */ "http://defaultturl/{searchTerms}",
        /* suggestions_url */
        "https://defaultturl2/{searchTerms}");
  }
};

// InvalidSearchProviderTest --------------------------------------------------

// Test environment without valid suggest and search URL.
class InvalidSearchProviderTest : public BaseSearchProviderTest {
 public:
  void SetUp() override {
    CustomizableSetUp(
        /* search_url */ "http://defaulturl/{searchTerms}",
        /* suggestions_url */ "http://defaulturl/{searchTerms}");
  }
};

// Implementation of classes --------------------------------------------------

// static
const char BaseSearchProviderTest::kNotApplicable[] = "Not Applicable";
const BaseSearchProviderTest::ExpectedMatch
    BaseSearchProviderTest::kEmptyExpectedMatch = {kNotApplicable, false};

void BaseSearchProviderTest::CustomizableSetUp(
    const std::string& search_url,
    const std::string& suggestions_url) {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  turl_model->Load();

  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetShortName(u"t");
  data.SetURL(search_url);
  data.suggestions_url = suggestions_url;
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);
  TemplateURLID default_provider_id = default_t_url_->id();
  ASSERT_NE(0, default_provider_id);

  // Add url1, with search term term1_.
  term1_url_ = AddSearchToHistory(default_t_url_, term1_, 1);

  // Create another TemplateURL.
  data.SetShortName(u"k");
  data.SetKeyword(u"k");
  data.SetURL("http://keyword/{searchTerms}");
  data.suggestions_url = "http://suggest_keyword/{searchTerms}";
  keyword_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_t_url_->id());

  // Add a page and search term for keyword_t_url_.
  keyword_url_ = AddSearchToHistory(keyword_t_url_, keyword_term_, 1);

  // Keywords are updated by the InMemoryHistoryBackend only after the message
  // has been processed on the history thread. Block until history processes all
  // requests to ensure the InMemoryDatabase is the state we expect it.
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(&AutocompleteClassifierFactory::BuildInstanceFor));

  client_ = std::make_unique<TestAutocompleteProviderClient>(
      profile_.get(), &test_url_loader_factory_);
  provider_ = new SearchProviderForTest(client_.get(), this, profile_.get());
  OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs = 0;
}

void BaseSearchProviderTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  // Shutdown the provider before the profile.
  provider_ = nullptr;
}

void BaseSearchProviderTest::RunTest(TestData* cases,
                                     int num_cases,
                                     bool prefer_keyword) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                            ChromeAutocompleteSchemeClassifier(profile_.get()));
    input.set_prefer_keyword(prefer_keyword);
    provider_->Start(input, false);
    matches = provider_->matches();
    SCOPED_TRACE(u"Input was: " + cases[i].input + u"; prefer_keyword was: " +
                 (prefer_keyword ? u"true" : u"false"));
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

void BaseSearchProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  if (run_loop_ && provider_->done()) {
    run_loop_->Quit();
    run_loop_ = nullptr;
  }
}

void BaseSearchProviderTest::RunTillProviderDone() {
  if (provider_->done())
    return;

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
}

void BaseSearchProviderTest::QueryForInput(const AutocompleteInput& input) {
  // Start a query.
  provider_->Start(input, false);

  // RunUntilIdle so that the task scheduled by SearchProvider to create the
  // URLFetchers runs.
  base::RunLoop().RunUntilIdle();
}

void BaseSearchProviderTest::QueryForInput(const std::u16string& text,
                                           bool prevent_inline_autocomplete,
                                           bool prefer_keyword) {
  AutocompleteInput input(text, metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input.set_prefer_keyword(prefer_keyword);
  QueryForInput(input);
}

void BaseSearchProviderTest::QueryForInputAndSetWYTMatch(
    const std::u16string& text,
    AutocompleteMatch* wyt_match) {
  QueryForInput(text, false, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(text));
  if (!wyt_match)
    return;
  ASSERT_GE(provider_->matches().size(), 1u);
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(
              base::CollapseWhitespace(text, false)),
          TemplateURLServiceFactory::GetForProfile(profile_.get())
              ->search_terms_data())),
      wyt_match));
}

void BaseSearchProviderTest::QueryForInputAndWaitForFetcherResponses(
    const std::u16string& text,
    const bool prefer_keyword,
    const std::string& default_fetcher_response,
    const std::string& keyword_fetcher_response) {
  test_url_loader_factory_.ClearResponses();
  QueryForInput(text, false, prefer_keyword);

  std::string text8;
  ASSERT_TRUE(base::UTF16ToUTF8(text.data(), text.length(), &text8));

  if (!default_fetcher_response.empty()) {
    test_url_loader_factory_.AddResponse(
        base::StrCat({"https://defaultturl2/", base::EscapePath(text8)}),
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
        base::StrCat({"http://suggest_keyword/", base::EscapePath(keyword)}),
        keyword_fetcher_response);
  }
  RunTillProviderDone();
}

GURL BaseSearchProviderTest::AddSearchToHistory(TemplateURL* t_url,
                                                std::u16string term,
                                                int visit_count) {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  GURL search(t_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(term),
      TemplateURLServiceFactory::GetForProfile(profile_.get())
          ->search_terms_data()));
  static base::Time last_added_time;
  last_added_time =
      std::max(base::Time::Now(), last_added_time + base::Microseconds(1));
  history->AddPageWithDetails(search, std::u16string(), visit_count,
                              visit_count, last_added_time, false,
                              history::SOURCE_BROWSED);
  history->SetKeywordSearchTermsForURL(search, t_url->id(), term);
  return search;
}

bool BaseSearchProviderTest::FindMatchWithContents(
    const std::u16string& contents,
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

bool BaseSearchProviderTest::FindMatchWithDestination(
    const GURL& url,
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

void BaseSearchProviderTest::FinishDefaultSuggestQuery(
    const std::u16string& query_text) {
  std::string text8;
  ASSERT_TRUE(
      base::UTF16ToUTF8(query_text.data(), query_text.length(), &text8));
  std::string url =
      base::StrCat({"https://defaultturl2/", base::EscapePath(text8)});

  ASSERT_TRUE(test_url_loader_factory_.IsPending(url));

  // Tell the SearchProvider the default suggest query is done.
  test_url_loader_factory_.AddResponse(url, "");
}

void BaseSearchProviderTest::CheckMatches(
    const std::string& description,
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

void BaseSearchProviderTest::ClearAllResults() {
  provider_->ClearAllResults();
}

// Actual Tests ---------------------------------------------------------------

// Make sure we query history for the default provider and a URLFetcher is
// created for the default provider suggest results.
TEST_F(SearchProviderTest, QueryDefaultProvider) {
  std::u16string term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // Make sure the default providers suggest service was queried.
  std::string expected_url(
      default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(profile_.get())
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
          TemplateURLServiceFactory::GetForProfile(profile_.get())
              ->search_terms_data())),
      &wyt_match));
  EXPECT_TRUE(wyt_match.description.empty());

  // The match for term1 should be more relevant than the what you typed match.
  EXPECT_GT(term1_match.relevance, wyt_match.relevance);
  // This longer match should be inlineable.
  EXPECT_TRUE(term1_match.allowed_to_be_default_match);
  // The what you typed match should be too, of course.
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Make sure we do NOT query history for the default provider. However a
// URLFetcher is created for the default provider suggest results.
TEST_F(SearchProviderTest, QueryDefaultProvider_LensSearchbox) {
  std::u16string term = term1_.substr(0, term1_.length() - 1);
  AutocompleteInput input(term,
                          metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  QueryForInput(input);

  // Make sure the default provider's suggest service was queried.
  std::string expected_url(
      default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(profile_.get())
              ->search_terms_data()));
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url));

  // Tell the SearchProvider the Suggest request is done.
  test_url_loader_factory_.AddResponse(
      expected_url,
      "[\"term\",[\"term2\"],[],[],{\"google:suggestrelevance\":[10],"
      "\"google:verbatimrelevance\":0}]");

  // Run until the SearchProvider is done.
  RunTillProviderDone();

  // Make sure the SearchProvider does NOT have a history result for "term1".
  AutocompleteMatch term1_match;
  EXPECT_FALSE(FindMatchWithContents(term1_, &term1_match));

  // Make sure the SearchProvider has a Suggest result for "term2".
  AutocompleteMatch term2_match;
  EXPECT_TRUE(FindMatchWithContents(u"term2", &term2_match));

  // Make sure the SearchProvider has a what you typed match.
  AutocompleteMatch wyt_match;
  EXPECT_TRUE(FindMatchWithContents(u"term", &wyt_match));

  // The "term2" match should be more relevant than the what you typed match.
  EXPECT_GT(term2_match.relevance, wyt_match.relevance);
}

// Make sure we get a query-what-you-typed result from the default search
// provider even if the default search provider's keyword is renamed in the
// middle of processing the query.
TEST_F(SearchProviderTest, HasQueryWhatYouTypedIfDefaultKeywordChanges) {
  std::u16string query = u"query";
  QueryForInput(query, false, false);

  // Make sure the default provider's suggest service was queried.
  EXPECT_TRUE(test_url_loader_factory_.IsPending("https://defaultturl2/query"));

  // Look up the TemplateURL for the keyword and modify its keyword.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  TemplateURL* template_url =
      template_url_service->GetTemplateURLForKeyword(default_t_url_->keyword());
  EXPECT_TRUE(template_url);
  template_url_service->ResetTemplateURL(
      template_url, template_url->short_name(), u"new_keyword_asdf",
      template_url->url());

  // In resetting the default provider, the fetcher should've been canceled.
  EXPECT_FALSE(
      test_url_loader_factory_.IsPending("https://defaultturl2/query"));
  RunTillProviderDone();

  // Makes sure the query-what-you-typed match is there.
  AutocompleteMatch wyt_match;
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(query),
          TemplateURLServiceFactory::GetForProfile(profile_.get())
              ->search_terms_data())),
      &wyt_match));
  EXPECT_TRUE(wyt_match.description.empty());
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

TEST_F(SearchProviderTest, HonorPreventInlineAutocomplete) {
  std::u16string term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, true, false);

  ASSERT_FALSE(provider_->matches().empty());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  EXPECT_TRUE(provider_->matches()[0].allowed_to_be_default_match);
}

// Issues a query that matches the registered keyword and makes sure history
// is queried as well as URLFetchers getting created.
TEST_F(SearchProviderTest, QueryKeywordProvider) {
  std::u16string term = keyword_term_.substr(0, keyword_term_.length() - 1);
  QueryForInput(u"k " + term, false, false);

  // Make sure the default providers suggest service was queried.
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending("https://defaultturl2/k%20keywor"));

  // Tell the SearchProvider the default suggest query is done.
  test_url_loader_factory_.AddResponse("https://defaultturl2/k%20keywor", "");

  // Make sure the keyword providers suggest service was queried, with
  // the URL we expected.
  std::string expected_url(
      keyword_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(profile_.get())
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
  EXPECT_EQ(keyword_t_url_->keyword() + u' ' + keyword_term_,
            match.fill_into_edit);
}

TEST_F(SearchProviderTest, SendDataToSuggestAtAppropriateTimes) {
  constexpr bool fileNameTreatedAsQuery =
      (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID));
  struct {
    std::string input;
    const bool expect_to_send_to_default_provider;
  } cases[] = {
      // None of the following input strings should be sent to the default
      // suggest server because they may contain potentially private data.
      {"username:password", false},
      {"User:f", false},
      {"http://username:password", false},
      {"https://username:password", false},
      {"username:password@hostname", false},
      {"http://username:password@hostname/", false},
      {"file://filename", fileNameTreatedAsQuery},
      {"data://data", false},
      {"unknownscheme:anything", false},
      {"http://hostname/?query=q", false},
      {"http://hostname/path#ref", false},
      {"http://hostname/path #ref", false},
      {"https://hostname/path", false},
      // For all of the following input strings, it doesn't make much difference
      // if we allow them to be sent to the default provider or not.  The
      // strings
      // need to be in this list of test cases however so that they are tested
      // against the keyword provider and verified that they are allowed to be
      // sent to it.
      {"User:", false},
      {"User::", false},
      {"User:!", false},
      // All of the following input strings should be sent to the default
      // suggest
      // server because they should not get caught by the private data checks.
      {"User", true},
      {"query", true},
      {"query with spaces", true},
      {"http://hostname", true},
      {"http://hostname/path", true},
      {"http://hostname #ref", true},
      {"www.hostname.com #ref", true},
      {"https://hostname", true},
      {"#hashtag", true},
      {"foo https://hostname/path", true},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE("for input=" + cases[i].input);
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    // Make sure the default provider's suggest service was or was not queried
    // as appropriate.
    EXPECT_EQ(
        cases[i].expect_to_send_to_default_provider,
        test_url_loader_factory_.IsPending(base::StrCat(
            {"https://defaultturl2/", base::EscapePath(cases[i].input)})));

    // Send the same input with an explicitly invoked keyword.  In all cases,
    // it's okay to send the request to the keyword suggest server.
    QueryForInput(u"k " + ASCIIToUTF16(cases[i].input), false, false);
    EXPECT_TRUE(test_url_loader_factory_.IsPending(base::StrCat(
        {"http://suggest_keyword/", base::EscapePath(cases[i].input)})));
  }
}

TEST_F(SearchProviderTest, DontAutocompleteURLLikeTerms) {
  GURL url = AddSearchToHistory(default_t_url_, u"docs.google.com", 1);

  // Add the term as a url.
  HistoryServiceFactory::GetForProfile(profile_.get(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPageWithDetails(GURL("http://docs.google.com"), std::u16string(), 1,
                           1, base::Time::Now(), false,
                           history::SOURCE_BROWSED);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"docs", &wyt_match));

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
  GURL term_url(AddSearchToHistory(default_t_url_, u"one search", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"on", &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(wyt_match.relevance, term_match.relevance);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"one se", &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// A multiword search with more than one visit should autocomplete immediately.
TEST_F(SearchProviderTest, AutocompleteMultipleVisitsImmediately) {
  GURL term_url(AddSearchToHistory(default_t_url_, u"two searches", 2));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"tw", &wyt_match));
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
  AddSearchToHistory(default_t_url_, u"two  searches ", 2);
  GURL suggested_url(default_t_url_->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"two searches"),
      TemplateURLServiceFactory::GetForProfile(profile_.get())
          ->search_terms_data()));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"two ", &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(suggested_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_EQ(u"searches", term_match.inline_autocompletion);
  EXPECT_EQ(u"two searches", term_match.fill_into_edit);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Newer multiword searches should score more highly than older ones.
TEST_F(SearchProviderTest, ScoreNewerSearchesHigher) {
  GURL term_url_a(AddSearchToHistory(default_t_url_, u"three searches aaa", 1));
  GURL term_url_b(AddSearchToHistory(default_t_url_, u"three searches bbb", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"three se", &wyt_match));
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
  GURL term_url_a(AddSearchToHistory(default_t_url_, u"games", 1));
  GURL term_url_b(AddSearchToHistory(default_t_url_, u"gangnam style", 1));
  GURL term_url_c(AddSearchToHistory(default_t_url_, u"gundam", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"f", &wyt_match));
  ASSERT_EQ(1u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"g", &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"ga", &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"gan", &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"gans", &wyt_match));
  ASSERT_EQ(1u, provider_->matches().size());
}

// An autocompleted multiword search should not be replaced by a different
// autocompletion while the user is still typing a valid prefix unless the
// user has typed the prefix as a query before.
TEST_F(SearchProviderTest, DontReplacePreviousAutocompletion) {
  GURL term_url_a(AddSearchToHistory(default_t_url_, u"four searches aaa", 3));
  GURL term_url_b(AddSearchToHistory(default_t_url_, u"four searches bbb", 1));
  GURL term_url_c(AddSearchToHistory(default_t_url_, u"four searches", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"fo", &wyt_match));
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

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"four se", &wyt_match));
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
      QueryForInputAndSetWYTMatch(u"four searches", &wyt_match));
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
  GURL term_url(AddSearchToHistory(default_t_url_, u"five", 1));
  AddSearchToHistory(default_t_url_, u"five searches bbb", 1);
  AddSearchToHistory(default_t_url_, u"five searches ccc", 1);
  AddSearchToHistory(default_t_url_, u"five searches ddd", 1);
  AddSearchToHistory(default_t_url_, u"five searches eee", 1);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"fi", &wyt_match));
  ASSERT_EQ(provider_->provider_max_matches() + 1, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Inline autocomplete matches regardless of case differences from the input.
TEST_F(SearchProviderTest, InlineMixedCaseMatches) {
  GURL term_url(AddSearchToHistory(default_t_url_, u"FOO", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(u"f", &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  if (base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions)) {
    EXPECT_TRUE(FindMatchWithDestination(
        GURL(base::ToLowerASCII(term_url.spec())), &term_match));
    EXPECT_EQ(u"foo", term_match.fill_into_edit);
    EXPECT_EQ(u"oo", term_match.inline_autocompletion);
  } else {
    EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
    EXPECT_EQ(u"FOO", term_match.fill_into_edit);
    EXPECT_EQ(u"OO", term_match.inline_autocompletion);
  }
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
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
  AddSearchToHistory(keyword_t_url_, u"term2", 1);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteController controller(
      std::make_unique<TestAutocompleteProviderClient>(
          profile_.get(), &test_url_loader_factory_),
      AutocompleteProvider::TYPE_SEARCH);
  AutocompleteInput input(u"k t", metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  controller.Start(input);
  const AutocompleteResult& result = controller.result();

  // There should be two matches, one for the keyword history, and one for
  // keyword provider's what-you-typed, in that order.
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, result.match_at(0).type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            result.match_at(1).type);
  EXPECT_GT(result.match_at(0).relevance, result.match_at(1).relevance);
  EXPECT_TRUE(result.match_at(0).allowed_to_be_default_match);
  EXPECT_TRUE(result.match_at(1).allowed_to_be_default_match);

  // The two keyword results should come with the keyword we expect.
  EXPECT_EQ(u"k", result.match_at(0).keyword);
  EXPECT_EQ(u"k", result.match_at(1).keyword);

  // The top result will always have a description. Whether the second result
  // has one doesn't matter much.  (If it was missing, people would infer that
  // it's the same search provider as the one above it.)
  EXPECT_FALSE(result.match_at(0).description.empty());
}

TEST_F(SearchProviderTest, KeywordVerbatim) {
  TestData cases[] = {
      // Test a simple keyword input.
      {u"k foo",
       1,
       {ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo")}},

      // Make sure extra whitespace after the keyword doesn't change the
      // keyword verbatim query.  Also verify that interior consecutive
      // whitespace gets trimmed.
      {u"k   foo",
       1,
       {ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo")}},
      // Leading whitespace should be stripped before SearchProvider gets the
      // input; hence there are no tests here about how it handles those inputs.

      // Verify that interior consecutive whitespace gets trimmed in either
      // case.
      {u"k  foo  bar",
       1,
       {ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo bar")}},

      // Verify that trailing whitespace gets trimmed.
      {u"k foo bar  ",
       1,
       {ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo bar")}},

      // Keywords can be prefixed by certain things that should get ignored
      // when constructing the keyword match.
      {u"www.k foo",
       1,
       {ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo")}},
      {u"http://k foo",
       1,
       {ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo")}},
      {u"http://www.k foo",
       1,
       {ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true,
                   u"k foo")}},

      // A keyword with no remaining input shouldn't get a keyword
      // verbatim match.
      {u"k",
       1,
       {ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true, u"k")}},
      // Ditto.  Trailing whitespace shouldn't make a difference.
      {u"k ",
       1,
       {ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true, u"k")}}

      // The fact that verbatim queries to keyword are handled by
      // KeywordProvider
      // not SearchProvider is tested in
      // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc.
  };

  // Test not in keyword mode.
  RunTest(cases, std::size(cases), false);

  // Test in keyword mode.  (Both modes should give the same result.)
  RunTest(cases, std::size(cases), true);
}

// Verifies Navsuggest results don't set a TemplateURL, which Instant relies on.
// Also verifies that just the *first* navigational result is listed as a match
// if suggested relevance scores were not sent.
TEST_F(SearchProviderTest, NavSuggestNoSuggestedRelevanceScores) {
  QueryForInputAndWaitForFetcherResponses(
      u"a.c", false,
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
      u"a", false, "[\"a\",[\"a1\", \"a2\", \"a3\", \"a4\"]]", std::string());

  // Check the expected verbatim and (first 3) suggestions' relative relevances.
  AutocompleteMatch verbatim, match_a1, match_a2, match_a3, match_a4;
  EXPECT_TRUE(FindMatchWithContents(u"a", &verbatim));
  EXPECT_TRUE(FindMatchWithContents(u"a1", &match_a1));
  EXPECT_TRUE(FindMatchWithContents(u"a2", &match_a2));
  EXPECT_TRUE(FindMatchWithContents(u"a3", &match_a3));
  EXPECT_FALSE(FindMatchWithContents(u"a4", &match_a4));
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
      {"[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
       "{\"google:verbatimrelevance\":9700,"
       "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9900, 9800]}]",
       "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"]}"
       "]",
       {"a", "akeyword-query", "", "", ""}},

      // Now try with keyword provider suggested relevance scores.
      {"[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
       "{\"google:verbatimrelevance\":9700,"
       "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9900, 9800]}]",
       "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"],"
       "\"google:verbatimrelevance\":9500,"
       "\"google:suggestrelevance\":[9600]}]",
       {"akeyword-query", "a", "", "", ""}}};

  for (size_t i = 0; i < std::size(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      QueryForInputAndWaitForFetcherResponses(u"k a", true,
                                              cases[i].default_provider_json,
                                              cases[i].keyword_provider_json);
    }

    SCOPED_TRACE(
        "for input with default_provider_json=" +
        cases[i].default_provider_json + " and keyword_provider_json=" +
        cases[i].keyword_provider_json);
    const ACMatches& matches = provider_->matches();
    ASSERT_LE(matches.size(), std::size(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j)
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]), matches[j].contents);
    // Ensure that no expected matches are missing.
    for (; j < std::size(cases[i].matches); ++j)
      EXPECT_EQ(std::string(), cases[i].matches[j]);
  }
}

// Verifies that suggest results with relevance scores are added
// properly when using the default fetcher.  When adding a new test
// case to this test, please consider adding it to the tests in
// KeywordFetcherSuggestRelevance below.
TEST_F(SearchProviderTest, DefaultFetcherSuggestRelevance) {
  // This test was written assuming a different default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {omnibox::kUIExperimentMaxAutocompleteMatches,
           {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}}},
      },
      {omnibox::kDynamicMaxAutocomplete});
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      QueryForInputAndWaitForFetcherResponses(u"a", false, cases[i].json,
                                              std::string());
    }

    const std::string description = "for input with json=" + cases[i].json;
    CheckMatches(description, std::size(cases[i].matches), cases[i].matches,
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
  // This test was written assuming a different default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {omnibox::kUIExperimentMaxAutocompleteMatches,
           {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}}},
      },
      {omnibox::kDynamicMaxAutocomplete});

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
    // Ensure that suggest relevance scores reorder matches.
    { "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a",   true,  true },
        { "c",   true,  false },
        { "b",   true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch },
      std::string() },

    // Without suggested relevance scores, we should only allow one
    // navsuggest result to be be displayed.
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that verbatimrelevance scores reorder or suppress verbatim.
    // Negative values will have no effect; the calculated value will be used.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9999,"
                             "\"google:suggestrelevance\":[9998]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9998,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":-1,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9999,"
        "\"google:suggestrelevance\":[9998]}]",
      { { "a",     true,  true },
        { "a.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that both types of relevance scores reorder matches together.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[9999, 9997],"
                                     "\"google:verbatimrelevance\":9998}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "a2",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },

    // Check that an inlineable match appears first regardless of its score.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999]}]",
      { { "a",   true,  true },
        { "b",   true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // If there is no inlineable match, restore the keyword verbatim score.
    // The keyword verbatim match will then appear first.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999],"
                            "\"google:verbatimrelevance\":0}]",
      { { "a",   true,  true },
        { "b",   true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // The top result does not have to score as highly as calculated
    // verbatim.  i.e., there are no minimum score restrictions in
    // this provider.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a1",  true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":10}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[10],"
                             "\"google:verbatimrelevance\":0}]",
      { { "a1",  true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 20],"
                                     "\"google:verbatimrelevance\":0}]",
      { { "a2",  true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[10, 30],"
      "\"google:verbatimrelevance\":20}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that all suggestions are considered, regardless of order.
    { "[\"a\",[\"b\", \"c\", \"d\", \"e\", \"f\", \"g\", \"h\"],[],[],"
       "{\"google:suggestrelevance\":[10, 20, 30, 40, 50, 60, 70]}]",
      { { "a",   true,  true },
        { "h",   true,  false },
        { "g",   true,  false },
        { "f",   true,  false },
        { "e",   true,  false },
        { "d",   true,  false }, },
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
        { "h.com", false, false },
        { "g.com", false, false },
        { "f.com", false, false },
        { "e.com", false, false },
        { "d.com", false, false }, },
      std::string() },

    // Ensure that incorrectly sized suggestion relevance lists are ignored.
    // Note that keyword suggestions by default (not in suggested relevance
    // mode) score more highly than the default verbatim.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        { "a2",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // In this case, ignoring the suggested relevance scores means we keep
    // only one navsuggest result.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that all 'verbatim' results are merged with their maximum score.
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that verbatim is always generated without other suggestions.
    // TODO(mpearson): Ensure the value of verbatimrelevance is respected
    // (except when suggested relevances are ignored).
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"https://a/\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a",   true,  true },
        { "a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 1300]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a3",     true,  false },
        kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9998, 9999, 9997]}]",
      { { "a3",     true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
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
        kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        kEmptyMatch, kEmptyMatch },
      "3" },
      // clang-format on
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    // Send the query twice in order to have a synchronous pass after the first
    // response is received.  This is necessary because SearchProvider doesn't
    // allow an asynchronous response to change the default match.
    for (size_t j = 0; j < 2; ++j) {
      test_url_loader_factory_.ClearResponses();
      QueryForInput(u"k a", false, true);

      // Set up a default fetcher with no results.
      ASSERT_TRUE(
          test_url_loader_factory_.IsPending("https://defaultturl2/k%20a"));
      test_url_loader_factory_.AddResponse("https://defaultturl2/k%20a", "");

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

    ASSERT_LE(matches.size(), std::size(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j].contents),
                matches[j].contents);
      EXPECT_EQ(cases[i].matches[j].from_keyword, matches[j].keyword == u"k");
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
    // Ensure that no expected matches are missing.
    for (; j < std::size(cases[i].matches); ++j) {
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    // First, send the query "a" and receive the JSON response |first_json|.
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(u"a", false, cases[i].first_json,
                                            std::string());

    // Verify that the matches after the asynchronous results are as expected.
    std::string description = "first asynchronous response for input with "
        "first_json=" + cases[i].first_json;
    CheckMatches(description, std::size(cases[i].first_async_matches),
                 cases[i].first_async_matches, provider_->matches());

    // Then, send the query "ab" and check the synchronous matches.
    description = "synchronous response after the first keystroke after input "
        "with first_json=" + cases[i].first_json;
    QueryForInput(u"ab", false, false);
    CheckMatches(description, std::size(cases[i].sync_matches),
                 cases[i].sync_matches, provider_->matches());

    // Finally, get the provided JSON response, |second_json|, and verify the
    // matches after the second asynchronous response are as expected.
    description = "second asynchronous response after input with first_json=" +
        cases[i].first_json + " and second_json=" + cases[i].second_json;
    ASSERT_TRUE(test_url_loader_factory_.IsPending("https://defaultturl2/ab"));
    test_url_loader_factory_.AddResponse("https://defaultturl2/ab",
                                         cases[i].second_json);
    RunTillProviderDone();
    CheckMatches(description, std::size(cases[i].second_async_matches),
                 cases[i].second_async_matches, provider_->matches());
  }
}

TEST_F(SearchProviderTest, DontCacheCalculatorSuggestions) {
  // This test sends two separate queries and checks that at each stage of
  // processing (receiving first asynchronous response, handling new keystroke
  // synchronously) we have the expected matches.  The new keystroke should
  // immediately invalidate old calculator suggestions.
  struct {
    std::string json;
    ExpectedMatch async_matches[4];
    ExpectedMatch sync_matches[4];
  } cases[] = {
      {"[\"1+2\",[\"= 3\", \"1+2+3+4+5\"],[],[],"
       "{\"google:verbatimrelevance\":1300,"
       "\"google:suggesttype\":[\"CALCULATOR\", \"QUERY\"],"
       "\"google:suggestrelevance\":[1200, 900]}]",
       // The contents of the second match here are set to the query (the result
       // is placed in the description instead) and therefore the
       // allowed_to_default_match value is true for the second match (despite
       // being received asynchronously) because of the logic in
       // SearchProvider::PersistTopSuggestions which allows it to be promoted
       // based on the fact that it has the same contents as the previous top
       // match.
       {{"1+2", true},
        {"= 3", false},
        {"1+2+3+4+5", false},
        kEmptyExpectedMatch},
       {{"1+23", true},
        {"1+2+3+4+5", false},
        kEmptyExpectedMatch,
        kEmptyExpectedMatch}},
  };

  // Note: SearchSuggestionParser::ParseSuggestResults swaps the content and
  // answer fields on Desktop. See https://crbug.com/1325124#c1.
  // As a result of the field flip, the Calculator answer is only permitted
  // to be the default suggestion on the Desktop.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP)
    cases[0].async_matches[1].contents = "1+2 = 3";

  for (size_t i = 0; i < std::size(cases); ++i) {
    // First, send the query "1+2" and receive the JSON response |first_json|.
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(u"1+2", false, cases[i].json,
                                            std::string());

    // Verify that the matches after the asynchronous results are as expected.
    std::string description = "first asynchronous response for input with "
        "json=" + cases[i].json;
    CheckMatches(description, std::size(cases[i].async_matches),
                 cases[i].async_matches, provider_->matches());

    // Then, send the query "1+23" and check the synchronous matches.
    description = "synchronous response after the first keystroke after input "
        "with json=" + cases[i].json;
    QueryForInput(u"1+23", false, false);
    CheckMatches(description, std::size(cases[i].sync_matches),
                 cases[i].sync_matches, provider_->matches());
  }
}

TEST_F(SearchProviderTest, LocalAndRemoteRelevances) {
  // This test was written assuming a different default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {omnibox::kUIExperimentMaxAutocompleteMatches,
           {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}}},
      },
      {omnibox::kDynamicMaxAutocomplete});
  // We hardcode the string "term1" below, so ensure that the search term that
  // got added to history already is that string.
  ASSERT_EQ(u"term1", term1_);
  std::u16string term = term1_.substr(0, term1_.length() - 1);

  AddSearchToHistory(default_t_url_, term + u"2", 2);
  profile_->BlockUntilHistoryProcessesPendingRequests();

  struct {
    const std::u16string input;
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        cases[i].input, false, cases[i].json, std::string());

    const std::string description = "for input with json=" + cases[i].json;
    const ACMatches& matches = provider_->matches();

    // Ensure no extra matches are present.
    ASSERT_LE(matches.size(), std::size(cases[i].matches));

    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j)
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]),
                matches[j].contents) << description;
    // Ensure that no expected matches are missing.
    for (; j < std::size(cases[i].matches); ++j)
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

  for (size_t i = 0; i < std::size(cases); ++i) {
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
    ASSERT_LE(matches.size(), std::size(cases[i].output));
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].output[j].match_contents),
                matches[j].contents);
      EXPECT_EQ(cases[i].output[j].match_type, matches[j].type);
      EXPECT_EQ(cases[i].output[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
    // Ensure that no expected matches are missing.
    for (; j < std::size(cases[i].output); ++j) {
      EXPECT_EQ(kNotApplicable, cases[i].output[j].match_contents);
      EXPECT_EQ(AutocompleteMatchType::NUM_TYPES,
                cases[i].output[j].match_type);
      EXPECT_FALSE(cases[i].output[j].allowed_to_be_default_match);
    }
  }
}

// A basic test that verifies the field trial triggered parsing logic.
TEST_F(SearchProviderTest, FieldTrialTriggeredParsing) {
  const auto test = [&](bool trigger) {
    client_->GetOmniboxTriggeredFeatureService()->ResetSession();
    QueryForInputAndWaitForFetcherResponses(
        u"foo", false,
        "[\"foo\",[\"foo bar\"],[\"\"],[],"
        "{\"google:suggesttype\":[\"QUERY\"],"
        "\"google:fieldtrialtriggered\":" +
            std::string(trigger ? "true" : "false") + "}]",
        std::string());

    // Check for the match and field trial triggered bits.
    AutocompleteMatch match;
    EXPECT_TRUE(FindMatchWithContents(u"foo bar", &match));
    EXPECT_EQ(client_->GetOmniboxTriggeredFeatureService()
                  ->GetFeatureTriggeredInSession(
                      metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE),
              trigger);
  };

  {
    SCOPED_TRACE("Feature triggered.");
    test(true);
  }

  {
    SCOPED_TRACE("Feature not triggered.");
    test(false);
  }
}

// A basic test that verifies the specific type identifier parsing logic.
TEST_F(SearchProviderTest, SpecificTypeIdentifierParsing) {
  struct Match {
    std::string contents;
    base::flat_set<omnibox::SuggestSubtype> subtypes;
  };

  struct {
    const std::string input_text;
    const std::string provider_response_json;
    // The order of the expected matches is not important.
    const std::vector<Match> expected_matches;
  } cases[] = {
      // Check that the specific type is set to 0 when these values are not
      // provide in the response.
      {"a",
       R"(["a",["ab","http://b.com"],[],[], {
         "google:suggesttype":["QUERY", "NAVIGATION"]
       }])",
       {{"ab"}, {"b.com"}}},

      // Check that the specific type works for zero-suggest suggestions.
      {"c",
       R"(["c",["cd","http://d.com"],[],[], {
         "google:suggesttype":     ["QUERY", "NAVIGATION"],
         "google:suggestsubtypes": [[1,7,12], [3,22,49]]
       }])",
       {{"cd",
         {static_cast<omnibox::SuggestSubtype>(1),
          static_cast<omnibox::SuggestSubtype>(7),
          static_cast<omnibox::SuggestSubtype>(12)}},
        {"d.com",
         {static_cast<omnibox::SuggestSubtype>(3),
          static_cast<omnibox::SuggestSubtype>(22),
          static_cast<omnibox::SuggestSubtype>(49)}}}},

      // Check that legacy subtypeid is populated alongside the suggestsubtypes.
      {"c",
       R"(["c",["cd","http://d.com"],[],[],{
         "google:suggesttype":     ["QUERY", "NAVIGATION"],
         "google:suggestsubtypes": [[1,7], [3,49]],
         "google:subtypeid":       [9, 11]
       }])",
       {{"cd",
         {static_cast<omnibox::SuggestSubtype>(1),
          static_cast<omnibox::SuggestSubtype>(7),
          static_cast<omnibox::SuggestSubtype>(9)}},
        {"d.com",
         {static_cast<omnibox::SuggestSubtype>(3),
          static_cast<omnibox::SuggestSubtype>(11),
          static_cast<omnibox::SuggestSubtype>(49)}}}},

      // Check that the specific type is set to zero when the number of
      // suggestions is smaller than the number of id's provided.
      {"foo",
       R"(["foo",["foo bar", "foo baz"],[],[],{
         "google:suggesttype":     ["QUERY", "QUERY"],
         "google:suggestsubtypes": [[17], [26]],
         "google:subtypeid":       [1, 2, 3]
       }])",
       {{"foo bar", {static_cast<omnibox::SuggestSubtype>(17)}},
        {"foo baz", {static_cast<omnibox::SuggestSubtype>(26)}}}},

      // Check that the specific type is set to zero when the number of
      // suggestions is larger than the number of id's provided.
      {"bar",
       R"(["bar",["bar foo", "bar foz"],[],[], {
         "google:suggesttype":     ["QUERY", "QUERY"],
         "google:suggestsubtypes": [[19], [31]],
         "google:subtypeid":       [1]
       }])",
       {{"bar foo", {static_cast<omnibox::SuggestSubtype>(19)}},
        {"bar foz", {static_cast<omnibox::SuggestSubtype>(31)}}}},

      // Check that in the event of receiving both suggestsubtypes and subtypeid
      // we try to preserve both, deduplicating repetitive numbers.
      {"bar",
       R"(["bar",["bar foo", "bar foz"],[],[], {
         "google:suggesttype":     ["QUERY", "QUERY"],
         "google:suggestsubtypes": [[19], [31]],
         "google:subtypeid":       [1, 31]
       }])",
       {{"bar foo",
         {static_cast<omnibox::SuggestSubtype>(1),
          static_cast<omnibox::SuggestSubtype>(19)}},
        {"bar foz", {static_cast<omnibox::SuggestSubtype>(31)}}}},

      // Check that in the event of receiving partially invalid subtypes we
      // extract as much information as reasonably possible.
      {"bar",
       R"(["bar",["barbados", "barn", "barry"],[],[], {
         "google:suggesttype":     ["QUERY", "QUERY", "QUERY"],
         "google:suggestsubtypes": [22, 0, [99, 10.3, "abc", 1]],
         "google:subtypeid":       [19, 11, 27]
       }])",
       {{"barbados", {static_cast<omnibox::SuggestSubtype>(19)}},
        {"barn", {static_cast<omnibox::SuggestSubtype>(11)}},
        {"barry",
         {static_cast<omnibox::SuggestSubtype>(27),
          static_cast<omnibox::SuggestSubtype>(99),
          static_cast<omnibox::SuggestSubtype>(1)}}}},

      // Check that ids stick to their suggestions when these are reordered
      // based on suggestion relevance values.
      {"e",
       R"(["e",["ef","http://e.com"],[],[], {
         "google:suggesttype":      ["QUERY", "NAVIGATION"],
         "google:suggestrelevance": [9300, 9800],
         "google:suggestsubtypes":  [[99], [100]],
         "google:subtypeid":        [2, 4]
       }])",
       {{"ef",
         {static_cast<omnibox::SuggestSubtype>(2),
          static_cast<omnibox::SuggestSubtype>(99)}},
        {"e.com",
         {static_cast<omnibox::SuggestSubtype>(4),
          static_cast<omnibox::SuggestSubtype>(100)}}}}};

  for (const auto& test : cases) {
    QueryForInputAndWaitForFetcherResponses(ASCIIToUTF16(test.input_text),
                                            false, test.provider_response_json,
                                            std::string());

    // Check for the match and subtypes.
    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());
    for (const auto& expected_match : test.expected_matches) {
      if (expected_match.contents == kNotApplicable)
        continue;
      AutocompleteMatch match;
      EXPECT_TRUE(
          FindMatchWithContents(ASCIIToUTF16(expected_match.contents), &match));
      EXPECT_EQ(expected_match.subtypes, match.subtypes);
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    // First test regular mode.
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    SearchSuggestionParser::NavigationResult result(
        ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(cases[i].url),
        AutocompleteMatchType::NAVSUGGEST,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
        std::u16string(), std::string(), false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false,
        ASCIIToUTF16(cases[i].input));
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
        ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(cases[i].url),
        AutocompleteMatchType::NAVSUGGEST,
        /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
        std::u16string(), std::string(), false,
        /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false,
        ASCIIToUTF16(cases[i].input));
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
  const std::u16string input(u"http:");
  const std::u16string url(u"http://a.com");
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, input);
  result.set_received_after_last_keystroke(false);

  // Check the offset and strings when inline autocompletion is allowed.
  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_inline.fill_into_edit);
  EXPECT_EQ(url.substr(5), match_inline.inline_autocompletion);
  EXPECT_TRUE(match_inline.allowed_to_be_default_match);
  EXPECT_EQ(url, match_inline.contents);

  // Check the same strings when inline autocompletion is prevented.
  QueryForInput(input, true, false);
  AutocompleteMatch match_prevent(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_prevent.fill_into_edit);
  EXPECT_FALSE(match_prevent.allowed_to_be_default_match);
  EXPECT_EQ(url, match_prevent.contents);
}

// Verifies that input "h" matches navsuggest "http://www.[h]ttp.com/http" and
// "http://www." is trimmed.
TEST_F(SearchProviderTest, NavigationInlineDomainClassify) {
  QueryForInput(u"h", false, false);
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()),
      GURL("http://www.http.com/http"), AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, u"h");
  result.set_received_after_last_keystroke(false);
  AutocompleteMatch match(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"ttp.com/http", match.inline_autocompletion);
  EXPECT_TRUE(match.allowed_to_be_default_match);
  EXPECT_EQ(u"www.http.com/http", match.fill_into_edit);
  EXPECT_EQ(u"http.com/http", match.contents);

  ASSERT_EQ(2U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL |
                AutocompleteMatch::ACMatchClassification::MATCH,
            match.contents_class[0].style);
  EXPECT_EQ(1U, match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[1].style);
}

// Verifies navsuggests prefer prefix matching even when a URL prefix prevents
// the input from being a perfect prefix of the suggest text; e.g., the input
// 'moon.com', matches 'http://[moon.com]/moon' and the 2nd 'moon' is unmatched.
TEST_F(SearchProviderTest, NavigationPrefixClassify) {
  QueryForInput(u"moon", false, false);
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()),
      GURL("http://moon.com/moon"), AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, u"moon");
  result.set_received_after_last_keystroke(false);
  AutocompleteMatch match(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"moon.com/moon", match.contents);
  ASSERT_EQ(2U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::MATCH |
                AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[0].style);
  EXPECT_EQ(4U, match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[1].style);
}

// Verifies navsuggests prohibit mid-word matches; e.g., 'f[acebook].com'.
TEST_F(SearchProviderTest, NavigationMidWordClassify) {
  QueryForInput(u"acebook", false, false);
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()),
      GURL("http://www.facebook.com"), AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, u"acebook");
  result.set_received_after_last_keystroke(false);
  AutocompleteMatch match(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"facebook.com", match.contents);
  ASSERT_EQ(1U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[0].style);
}

// Verifies navsuggests break user and suggest texts on words;
// e.g., the input 'duck', matches 'yellow-animals.com/[duck]'
TEST_F(SearchProviderTest, NavigationWordBreakClassify) {
  QueryForInput(u"duck", false, false);
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()),
      GURL("http://www.yellow-animals.com/duck"),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, u"duck");
  result.set_received_after_last_keystroke(false);
  AutocompleteMatch match(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"yellow-animals.com/duck", match.contents);
  ASSERT_EQ(2U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[0].style);
  EXPECT_EQ(19U, match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::MATCH |
                AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[1].style);
}

// Verifies that "http://" is trimmed in the general case.
TEST_F(SearchProviderTest, DoTrimHttpScheme) {
  const std::u16string input(u"face book");
  const std::u16string url(u"http://www.facebook.com");
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"facebook.com", match_inline.contents);
}

// Verifies that "http://" is not trimmed for input that has a scheme, even if
// the input doesn't match the URL.
TEST_F(SearchProviderTest, DontTrimHttpSchemeIfInputHasScheme) {
  const std::u16string input(u"https://face book");
  const std::u16string url(u"http://www.facebook.com");
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"http://facebook.com", match_inline.contents);
}

// Verifies that "https://" is not trimmed for input that has a (non-matching)
// scheme.
TEST_F(SearchProviderTest, DontTrimHttpsSchemeIfInputHasScheme) {
  const std::u16string input(u"http://face book");
  const std::u16string url(u"https://www.facebook.com");
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"https://facebook.com", match_inline.contents);
}

// Verifies that "https://" is trimmed in the general case.
TEST_F(SearchProviderTest, DoTrimHttpsScheme) {
  const std::u16string input(u"face book");
  const std::u16string url(u"https://www.facebook.com");
  SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(profile_.get()), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      std::u16string(), std::string(), false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE, 0, false, input);

  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(u"facebook.com", match_inline.contents);
}

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

  omnibox::EntityInfo entity_info;
  entity_info.set_name("xy");
  entity_info.set_annotation("A");
  entity_info.set_suggest_search_parameters("p=v");

  struct {
    const std::string input_text;
    const std::string response_json;
    const Match matches[5];
  } cases[] = {
      // A query and an entity suggestion with different search terms.
      {
          "x",
          R"(
      [
        "x",
        [
            "xy", "yy"
        ],
        [
            "", ""
        ],
        [],
        {
        "google:suggestdetail":[
            {},
            {
              "google:entityinfo": ")" +
              SerializeAndEncodeEntityInfo(entity_info) +
              R"("
            }
        ],
        "google:suggesttype":["QUERY","ENTITY"]
      }]
      )",
          {{"x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
           {"xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST},
           {"xy", "A", "p=v", "yy",
            AutocompleteMatchType::SEARCH_SUGGEST_ENTITY},
           kEmptyMatch,
           kEmptyMatch},
      },
      // A query and an entity suggestion with same search terms.
      {
          "x",
          R"(
      [
        "x",
        [
            "xy", "xy"
        ],
        [
            "", ""
        ],
        [],
        {
        "google:suggestdetail":[
            {},
            {
              "google:entityinfo": ")" +
              SerializeAndEncodeEntityInfo(entity_info) +
              R"("
            }
        ],
        "google:suggesttype":["QUERY","ENTITY"]
      }]
      )",
          {{"x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
           {"xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST},
           {"xy", "A", "p=v", "xy",
            AutocompleteMatchType::SEARCH_SUGGEST_ENTITY},
           kEmptyMatch,
           kEmptyMatch},
      },
  };
  for (size_t i = 0; i < std::size(cases); ++i) {
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text), false, cases[i].response_json,
        std::string());

    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());

    SCOPED_TRACE("for input with json = " + cases[i].response_json);

    ASSERT_LE(matches.size(), std::size(cases[i].matches));
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
    for (; j < std::size(cases[i].matches); ++j) {
      SCOPED_TRACE(" and match index: " + base::NumberToString(j));
      EXPECT_EQ(cases[i].matches[j].contents, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].description, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].query_params, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].fill_into_edit, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].type, AutocompleteMatchType::NUM_TYPES);
    }
  }
}

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
           {"c", false, AutocompleteMatchType::SEARCH_SUGGEST, true},
           {"b", false, AutocompleteMatchType::SEARCH_SUGGEST, true},
           {"ab", false, AutocompleteMatchType::SEARCH_SUGGEST, false},
           kEmptyMatch},
      }};

  for (size_t i = 0; i < std::size(cases); ++i) {
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

    ASSERT_LE(matches.size(), std::size(cases[i].matches));
    // Ensure that the returned matches equal the expectations.
    for (size_t j = 0; j < matches.size(); ++j) {
      SCOPED_TRACE(description);
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_prefetched,
                SearchProvider::ShouldPrefetch(matches[j]));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
      EXPECT_EQ(cases[i].matches[j].from_keyword, matches[j].keyword == u"k");
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

  for (size_t i = 0; i < std::size(cases); ++i) {
    ClearAllResults();
    QueryForInputAndWaitForFetcherResponses(
        ASCIIToUTF16(cases[i].input_text), false,
        cases[i].default_provider_response_json, std::string());

    const ACMatches& matches = provider_->matches();
    // The top match must inline and score as highly as calculated verbatim.
    ASSERT_FALSE(matches.empty());
    EXPECT_GE(matches[0].relevance, 1300);

    SCOPED_TRACE("for case: " + base::NumberToString(i));
    ASSERT_LE(matches.size(), std::size(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      SCOPED_TRACE("and match: " + base::NumberToString(j));
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
    }
    for (; j < std::size(cases[i].matches); ++j) {
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

  for (size_t i = 0; i < std::size(cases); ++i) {
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
                matches[j].GetAdditionalInfoForDebugging("deletion_url"));
    }
  }
}

// Tests that all conditions must be met to send the current page URL in the
// suggest requests.
TEST_F(SearchProviderTest, CanSendRequestWithURL) {
  // Benchmark test for HTTPS page URL on different origin as Suggest endpoint.
  auto test_different_origin = [](TemplateURL* template_url,
                                  AutocompleteProviderClient* client,
                                  SearchProvider* provider) {
    // Requires personalized URL data collection to be active.
    return client->IsPersonalizedUrlDataCollectionActive() &&
           provider->CanSendCurrentPageURLInRequest(
               GURL("https://www.example.com?q=foo"),
               metrics::OmniboxEventProto::OTHER, template_url,
               SearchTermsData(), client);
  };

  // Benchmark test for HTTPS page URL on same origin as Suggest endpoint.
  // Uses the same URL as the Suggest endpoint for the current page URL.
  auto test_same_origin = [](TemplateURL* template_url,
                             AutocompleteProviderClient* client,
                             SearchProvider* provider) {
    // Requires personalized URL data collection to be active.
    return client->IsPersonalizedUrlDataCollectionActive() &&
           provider->CanSendCurrentPageURLInRequest(
               template_url->GenerateSuggestionURL(SearchTermsData()),

               metrics::OmniboxEventProto::OTHER, template_url,
               SearchTermsData(), client);
  };

  // Benchmark test for Search Results Page URL.
  auto test_srp = [](TemplateURL* template_url,
                     AutocompleteProviderClient* client,
                     SearchProvider* provider) {
    return provider->CanSendCurrentPageURLInRequest(
        template_url->GenerateSearchURL(SearchTermsData()),
        metrics::OmniboxEventProto::SRP_ZPS_PREFETCH, template_url,
        SearchTermsData(), client);
  };

  // Create an HTTPS Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetShortName(u"https-google");
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  TemplateURL google_template_url(google_template_url_data);

  // Enable personalized URL data collection.
  client_->set_is_personalized_url_data_collection_active(true);

  // Personalized URL data collection is active. Test that we can send the page
  // URL if all of the following hold:
  // 1) Google is the default search provider.
  // 2) The page URL is a valid HTTP(S) URL.
  // 3) The page classification is not NTP.
  // 4) The suggest endpoint URL is a valid HTTPS URL.
  // 5) Suggest is not disabled.
  // 6) The user is not in incognito mode.
  EXPECT_TRUE(test_different_origin(&google_template_url, client_.get(),
                                    provider_.get()));
  EXPECT_TRUE(
      test_same_origin(&google_template_url, client_.get(), provider_.get()));
  EXPECT_TRUE(test_srp(&google_template_url, client_.get(), provider_.get()));

  // Invalid page URL - invalid URL.
  EXPECT_FALSE(provider_->CanSendCurrentPageURLInRequest(
      GURL("badpageurl"), metrics::OmniboxEventProto::OTHER,
      &google_template_url, SearchTermsData(), client_.get()));

  // Invalid page URL - non-HTTP(S) URL.
  EXPECT_FALSE(provider_->CanSendCurrentPageURLInRequest(
      GURL("ftp://www.google.com/search?q=foo"),
      metrics::OmniboxEventProto::OTHER, &google_template_url,
      SearchTermsData(), client_.get()));

  // Invalid page classification - New Tab Page.
  EXPECT_FALSE(provider_->CanSendCurrentPageURLInRequest(
      GURL("https://www.google.com/search?q=foo"),
      metrics::OmniboxEventProto::NTP_REALBOX, &google_template_url,
      SearchTermsData(), client_.get()));

  // Invalid page classification - New Tab Page.
  EXPECT_FALSE(provider_->CanSendCurrentPageURLInRequest(
      GURL("https://www.google.com/search?q=foo"),
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      &google_template_url, SearchTermsData(), client_.get()));

  // Disable Suggest.
  profile_->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);

  // These tests should otherwise succeed.
  EXPECT_FALSE(test_different_origin(&google_template_url, client_.get(),
                                     provider_.get()));
  EXPECT_FALSE(
      test_same_origin(&google_template_url, client_.get(), provider_.get()));
  EXPECT_FALSE(test_srp(&google_template_url, client_.get(), provider_.get()));

  // Re-enable Suggest.
  profile_->GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  // Ensure the state is properly reset.
  EXPECT_TRUE(test_different_origin(&google_template_url, client_.get(),
                                    provider_.get()));
  EXPECT_TRUE(
      test_same_origin(&google_template_url, client_.get(), provider_.get()));
  EXPECT_TRUE(test_srp(&google_template_url, client_.get(), provider_.get()));

  // Disable personalized URL data collection.
  client_->set_is_personalized_url_data_collection_active(false);

  // Personalized URL data collection is not active. Test that we cannot send
  // the page URL unless it is the Search Results Page.
  EXPECT_FALSE(test_different_origin(&google_template_url, client_.get(),
                                     provider_.get()));
  EXPECT_FALSE(
      test_same_origin(&google_template_url, client_.get(), provider_.get()));
  EXPECT_TRUE(test_srp(&google_template_url, client_.get(), provider_.get()));

  // Re-enable personalized URL data collection.
  client_->set_is_personalized_url_data_collection_active(true);

  // Ensure the state is properly reset.
  EXPECT_TRUE(test_different_origin(&google_template_url, client_.get(),
                                    provider_.get()));
  EXPECT_TRUE(
      test_same_origin(&google_template_url, client_.get(), provider_.get()));
  EXPECT_TRUE(test_srp(&google_template_url, client_.get(), provider_.get()));

  // Incognito profile.
  ChromeAutocompleteProviderClient incognito_client(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // These tests should otherwise succeed.
  EXPECT_FALSE(test_different_origin(&google_template_url, &incognito_client,
                                     provider_.get()));
  EXPECT_FALSE(test_same_origin(&google_template_url, &incognito_client,
                                provider_.get()));
  EXPECT_FALSE(
      test_srp(&google_template_url, &incognito_client, provider_.get()));

  // Create a non-Google search provider.
  TemplateURLData non_google_template_url_data;
  non_google_template_url_data.SetShortName(u"non-google");
  non_google_template_url_data.SetURL(
      "https://www.non-google.com/search?q={searchTerms}");
  non_google_template_url_data.suggestions_url =
      "https://www.non-google.com/suggest?q={searchTerms}";
  TemplateURL non_google_template_url(non_google_template_url_data);

  // These tests should otherwise succeed.
  EXPECT_FALSE(test_different_origin(&non_google_template_url, client_.get(),
                                     provider_.get()));
  EXPECT_FALSE(test_same_origin(&non_google_template_url, client_.get(),
                                provider_.get()));
  EXPECT_FALSE(
      test_srp(&non_google_template_url, client_.get(), provider_.get()));

  // Create a non-HTTPS Google search provider.
  TemplateURLData http_google_template_url_data;
  http_google_template_url_data.SetShortName(u"non-https-google");
  http_google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  http_google_template_url_data.suggestions_url =
      "http://www.google.com/suggest?q={searchTerms}";
  TemplateURL http_google_template_url(http_google_template_url_data);

  // These cases should otherwise succeed.
  EXPECT_FALSE(test_different_origin(&http_google_template_url, client_.get(),
                                     provider_.get()));
  EXPECT_FALSE(test_same_origin(&http_google_template_url, client_.get(),
                                provider_.get()));
  EXPECT_FALSE(
      test_srp(&http_google_template_url, client_.get(), provider_.get()));
}

// SearchProviderRequestTest ---------------------------------------------------

class MockSearchProviderForTest
    : public testing::NiceMock<SearchProviderForTest> {
 public:
  MockSearchProviderForTest(AutocompleteProviderClient* client,
                            AutocompleteProviderListener* listener,
                            Profile* profile);
  MockSearchProviderForTest(const MockSearchProviderForTest&) = delete;
  MockSearchProviderForTest& operator=(const MockSearchProviderForTest&) =
      delete;

  // SearchProvider:
  MOCK_METHOD(
      bool,
      CanSendCurrentPageURLInRequest,
      (const GURL& current_page_url,
       metrics::OmniboxEventProto::PageClassification page_classification,
       const TemplateURL* template_url,
       const SearchTermsData& search_terms_data,
       const AutocompleteProviderClient* client),
      (override));

 protected:
  ~MockSearchProviderForTest() override = default;
};

MockSearchProviderForTest::MockSearchProviderForTest(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener,
    Profile* profile)
    : testing::NiceMock<SearchProviderForTest>(client, listener, profile) {}

// Test environment to verify whether the current page URL is sent in the
// suggest requests when all the conditions are met or not.
class SearchProviderRequestTest : public SearchProviderTest {
 public:
  explicit SearchProviderRequestTest(const bool command_line_overrides = false)
      : SearchProviderTest(command_line_overrides) {}

  void SetUp() override {
    CustomizableSetUp(
        /* search_url */ "http://defaultturl/{searchTerms}",
        /* suggestions_url */
        "https://defaultturl2/{searchTerms}&{google:currentPageUrl}");

    provider_ =
        new MockSearchProviderForTest(client_.get(), this, profile_.get());
  }

 protected:
  scoped_refptr<MockSearchProviderForTest> provider_;
};

TEST_F(SearchProviderRequestTest, SendRequestWithoutURL) {
  EXPECT_CALL(*provider_, CanSendCurrentPageURLInRequest(_, _, _, _, _))
      .WillRepeatedly(testing::Return(false));

  // Start a query.
  AutocompleteInput input(u"foo", metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  input.set_current_url(GURL("https://www.example.com"));
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried without the
  // current page URL.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(test_url_loader_factory_.IsPending("https://defaultturl2/foo&"));
}

TEST_F(SearchProviderRequestTest, SendRequestWithURL) {
  EXPECT_CALL(*provider_, CanSendCurrentPageURLInRequest(_, _, _, _, _))
      .WillRepeatedly(testing::Return(true));

  // Start a query.
  AutocompleteInput input(u"foo", metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  input.set_current_url(GURL("https://www.example.com"));
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried with the
  // current page URL.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://defaultturl2/foo&url=https%3A%2F%2Fwww.example.com%2F&"));
}

TEST_F(SearchProviderRequestTest, SendRequestWithoutLensInteractionResponse) {
  // Start a query.
  AutocompleteInput input(u"foo",
                          metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  lens::proto::LensOverlaySuggestInputs lens_overlay_suggest_inputs;
  lens_overlay_suggest_inputs.set_encoded_image_signals("xyz");
  input.set_lens_overlay_suggest_inputs(lens_overlay_suggest_inputs);
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried with the
  // Lens interaction response.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(test_url_loader_factory_.IsPending("https://defaultturl2/foo&"));
}

TEST_F(SearchProviderRequestTest, SendRequestWithLensInteractionResponse) {
  // Set up a Google default search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetShortName(u"t");
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  TemplateURL* template_url =
      turl_model->Add(std::make_unique<TemplateURL>(google_template_url_data));
  turl_model->SetUserSelectedDefaultSearchProvider(template_url);
  ASSERT_NE(0, template_url->id());

  // Start a query.
  AutocompleteInput input(u"foo",
                          metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  lens::proto::LensOverlaySuggestInputs lens_overlay_suggest_inputs;
  lens_overlay_suggest_inputs.set_encoded_image_signals("xyz");
  input.set_lens_overlay_suggest_inputs(lens_overlay_suggest_inputs);
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried with the
  // expected client and Lens Suggest signals.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://www.google.com/suggest?q=foo&client=chrome-multimodal&iil=xyz"));
}

TEST_F(SearchProviderTest, TestDeleteMatch) {
  const char kDeleteUrl[] = "https://www.google.com/complete/deleteitem?q=foo";
  AutocompleteMatch match(
      provider_.get(), 0, true, AutocompleteMatchType::SEARCH_SUGGEST);
  match.RecordAdditionalInfo(SearchProvider::kDeletionUrlKey, kDeleteUrl);

  // Test a successful deletion request.
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_loaders_.empty());
  EXPECT_TRUE(provider_->matches_.empty());

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kDeleteUrl));
  test_url_loader_factory_.AddResponse(kDeleteUrl, "");

  // Need to spin the event loop to let the fetch result go through.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->deletion_loaders_.empty());
  EXPECT_TRUE(provider_->is_success());

  // Test a failing deletion request.
  test_url_loader_factory_.ClearResponses();
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_loaders_.empty());
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kDeleteUrl));

  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 500 Owiee\nContent-type: application/json\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "application/json";
  test_url_loader_factory_.AddResponse(GURL(kDeleteUrl), std::move(head), "",
                                       network::URLLoaderCompletionStatus());

  profile_->BlockUntilHistoryProcessesPendingRequests();
  EXPECT_TRUE(provider_->deletion_loaders_.empty());
  EXPECT_FALSE(provider_->is_success());
}

TEST_F(SearchProviderTest, TestDeleteHistoryQueryMatch) {
  GURL term_url(AddSearchToHistory(default_t_url_, u"flash games", 1));
  profile_->BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch games;
  QueryForInput(u"fla", false, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(u"fla"));
  ASSERT_TRUE(FindMatchWithContents(u"flash games", &games));

  size_t matches_before = provider_->matches().size();
  provider_->DeleteMatch(games);
  EXPECT_EQ(matches_before - 1, provider_->matches().size());

  // Process history deletions.
  profile_->BlockUntilHistoryProcessesPendingRequests();

  // Check that the match is gone.
  test_url_loader_factory_.ClearResponses();
  QueryForInput(u"fla", false, false);
  profile_->BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery(u"fla"));
  EXPECT_FALSE(FindMatchWithContents(u"flash games", &games));
}

// Verifies that duplicates are preserved in AddMatchToMap().
TEST_F(SearchProviderTest, CheckDuplicateMatchesSaved) {
  AddSearchToHistory(default_t_url_, u"a", 1);
  AddSearchToHistory(default_t_url_, u"alpha", 1);
  AddSearchToHistory(default_t_url_, u"avid", 1);

  profile_->BlockUntilHistoryProcessesPendingRequests();
  QueryForInputAndWaitForFetcherResponses(
      u"a", false,
      "[\"a\",[\"a\", \"alpha\", \"avid\", \"apricot\"],[],[],"
      "{\"google:suggestrelevance\":[1450, 1200, 1150, 1100],"
      "\"google:verbatimrelevance\":1350}]",
      std::string());

  AutocompleteMatch verbatim, match_alpha, match_apricot, match_avid;
  EXPECT_TRUE(FindMatchWithContents(u"a", &verbatim));
  EXPECT_TRUE(FindMatchWithContents(u"alpha", &match_alpha));
  EXPECT_TRUE(FindMatchWithContents(u"apricot", &match_apricot));
  EXPECT_TRUE(FindMatchWithContents(u"avid", &match_avid));

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
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  TemplateURLData data;
  data.SetShortName(u"default");
  data.SetKeyword(data.short_name());
  data.SetURL("http://example/{searchTerms}{google:sessionToken}");
  data.suggestions_url =
      "http://suggest/?q={searchTerms}&{google:sessionToken}";
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  std::u16string term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // And the URL matches what we expected.
  TemplateURLRef::SearchTermsArgs search_terms_args(term);
  search_terms_args.session_token =
      provider_->client()->GetTemplateURLService()->GetSessionToken();
  std::string expected_url(
      default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
          search_terms_args, turl_model->search_terms_data()));

  // Make sure the default provider's suggest service was queried.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(expected_url));

  // Complete running the fetcher to clean up.
  test_url_loader_factory_.AddResponse(expected_url, "");
  RunTillProviderDone();
}

TEST_F(SearchProviderTest, AnswersCache) {
  AutocompleteResult result;
  ACMatches matches;
  AutocompleteMatch match1;
  match1.answer = SuggestionAnswer();
  match1.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match1.fill_into_edit = u"weather los angeles";

  AutocompleteMatch non_answer_match1;
  non_answer_match1.fill_into_edit = u"weather laguna beach";

  // Test that an answer in the first slot populates the cache.
  matches.push_back(match1);
  matches.push_back(non_answer_match1);
  result.AppendMatches(matches);
  provider_->RegisterDisplayedAnswers(result);
  ASSERT_FALSE(provider_->answers_cache_.empty());
  AnswersQueryData answer =
      provider_->answers_cache_.GetTopAnswerEntry(u"weather l");
  EXPECT_EQ(u"weather los angeles", answer.full_query_text);

  AutocompleteMatch match2;
  match2.answer_template = omnibox::RichAnswerTemplate();
  match2.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match2.fill_into_edit = u"weather san diego";

  AutocompleteResult result2;
  ACMatches matches2;
  matches2.push_back(match2);
  matches2.push_back(non_answer_match1);
  result2.AppendMatches(matches2);
  provider_->RegisterDisplayedAnswers(result2);
  ASSERT_FALSE(provider_->answers_cache_.empty());
  AnswersQueryData answer2 =
      provider_->answers_cache_.GetTopAnswerEntry(u"weather s");
  EXPECT_EQ(u"weather san diego", answer2.full_query_text);

  // Without scored results, no answers will be retrieved.
  answer = provider_->FindAnswersPrefetchData();
  EXPECT_TRUE(answer.full_query_text.empty());
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, answer.query_type);

  // Inject a scored result, which will trigger answer retrieval.
  std::u16string query = u"weather los angeles";
  SearchSuggestionParser::SuggestResult suggest_result(
      query, AutocompleteMatchType::SEARCH_HISTORY,
      /*suggest_type=*/omnibox::TYPE_NATIVE_CHROME, /*subtypes=*/{},
      /*from_keyword=*/false,
      /*navigational_intent=*/omnibox::NAV_INTENT_NONE,
      /*relevance=*/1200, /*relevance_from_server=*/false,
      /*input_text=*/query);
  QueryForInput(u"weather l", false, false);
  provider_->transformed_default_history_results_.push_back(suggest_result);
  answer = provider_->FindAnswersPrefetchData();
  EXPECT_EQ(u"weather los angeles", answer.full_query_text);
  EXPECT_EQ(omnibox::ANSWER_TYPE_WEATHER, answer.query_type);
}

TEST_F(SearchProviderTest, RemoveExtraAnswers) {
  SuggestionAnswer answer1;
  SuggestionAnswer answer2;

  ACMatches matches;
  AutocompleteMatch match1, match2, match3, match4, match5;
  match1.answer = answer1;
  match1.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match3.answer = answer2;
  match3.answer_type = omnibox::ANSWER_TYPE_TRANSLATION;
  match5.answer_template = omnibox::RichAnswerTemplate();
  match5.answer_type = omnibox::ANSWER_TYPE_FINANCE;

  matches.push_back(match1);
  matches.push_back(match2);
  matches.push_back(match3);
  matches.push_back(match4);
  matches.push_back(match5);

  SearchProvider::RemoveExtraAnswers(&matches);
  EXPECT_EQ(omnibox::ANSWER_TYPE_WEATHER, matches[0].answer_type);
  EXPECT_TRUE(answer1.Equals(*matches[0].answer));
  EXPECT_FALSE(matches[1].answer || matches[1].answer_template);
  EXPECT_FALSE(matches[2].answer || matches[2].answer_template);
  EXPECT_FALSE(matches[3].answer || matches[3].answer_template);
  EXPECT_FALSE(matches[4].answer || matches[4].answer_template);
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, matches[1].answer_type);
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, matches[2].answer_type);
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, matches[3].answer_type);
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, matches[4].answer_type);
}

TEST_F(SearchProviderTest, DuplicateCardAnswer) {
  ACMatches matches;
  AutocompleteMatch match1, match2, match3;
  match1.contents = u"match 1";
  match1.type = AutocompleteMatchType::SEARCH_SUGGEST;
  match1.allowed_to_be_default_match = true;
  match1.answer_template = omnibox::RichAnswerTemplate();
  match1.destination_url = GURL("http://www.google.com/google.com/search?");

  matches.push_back(match1);
  matches.push_back(match2);
  matches.push_back(match3);

  SearchProvider::DuplicateCardAnswer(&matches);

  EXPECT_EQ(4u, matches.size());
  EXPECT_TRUE(matches[0].answer_template);
  EXPECT_FALSE(matches[0].allowed_to_be_default_match);
  EXPECT_FALSE(matches[3].answer_template);
  EXPECT_TRUE(matches[3].allowed_to_be_default_match);
  EXPECT_EQ(matches[3].suggestion_group_id, omnibox::GROUP_SEARCH);
  EXPECT_EQ(matches[0].contents, matches[3].contents);
  EXPECT_EQ(matches[0].type, matches[3].type);
}

TEST_F(SearchProviderTest, CopyAnswerToVerbatim) {
  QueryForInput(u"weather los angeles ", false, false);

  AutocompleteMatch match;
  match.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match.answer_template = omnibox::RichAnswerTemplate();
  match.answer_template->add_answers();
  match.fill_into_edit = u"weather los angeles";
  match.type = AutocompleteMatchType::SEARCH_HISTORY;
  provider_->matches_.push_back(match);
  provider_->ConvertResultsToAutocompleteMatches();

  EXPECT_EQ(1u, provider_->matches().size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  EXPECT_EQ(omnibox::ANSWER_TYPE_WEATHER, provider_->matches()[0].answer_type);
  EXPECT_TRUE(provider_->matches()[0].answer_template);
}

TEST_F(SearchProviderTest, DoesNotProvideOnFocus) {
  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  input.set_prefer_keyword(true);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(InvalidSearchProviderTest, DoesNotSendSuggestRequests) {
  std::u16string query = u"query";
  QueryForInput(query, false, false);

  // Make sure the default provider's suggest service was not queried.
  EXPECT_FALSE(test_url_loader_factory_.IsPending("http://defaulturl/query"));
}

TEST_F(SearchProviderTest, SendsWarmUpRequestOnFocus) {
  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  input.set_prefer_keyword(true);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  provider_->Start(input, false);
  // RunUntilIdle so that SearchProvider create the URLFetcher.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());
  // Make sure the default provider's suggest service was queried with an
  // empty query.
  EXPECT_TRUE(test_url_loader_factory_.IsPending("https://defaultturl2/"));
  // Even if the fetcher returns results, we should still have no suggestions
  // (though the provider should now be done).
  test_url_loader_factory_.AddResponse("https://defaultturl2/",
                                       R"(["",["a", "b"],[],[],{}])");
  RunTillProviderDone();
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());
}

// SearchProviderCommandLineOverrideTest -------------------------------------
//
// Like SearchProviderTest.  The only addition is that it sets additional
// command line flags in SearchProviderFeatureTestComponent.
class SearchProviderCommandLineOverrideTest : public SearchProviderTest {
 public:
  SearchProviderCommandLineOverrideTest() : SearchProviderTest(true) {}

  SearchProviderCommandLineOverrideTest(
      SearchProviderCommandLineOverrideTest const&) = delete;
  SearchProviderCommandLineOverrideTest& operator=(
      SearchProviderCommandLineOverrideTest const&) = delete;
};

TEST_F(SearchProviderCommandLineOverrideTest, CommandLineOverrides) {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  TemplateURLData data;
  data.SetShortName(u"default");
  data.SetKeyword(data.short_name());
  data.SetURL("{google:baseURL}{searchTerms}");
  default_t_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  TestData cases[] = {
      {u"k a",
       1,
       {ResultInfo(GURL("http://keyword/a"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE, true, u"k a")}},
  };

  RunTest(cases, std::size(cases), false);
}
