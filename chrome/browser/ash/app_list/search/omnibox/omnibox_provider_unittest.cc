// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"

#include <cstddef>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/answer_type.pb.h"

namespace app_list::test {

// Note that there is necessarily a lot of overlap with unittest in the lacros
// omnibox provider unittest, since this is testing the same behavior.
namespace {

// Helper functions to populate search results.
// Currently only the ones that may affect test results are filled.
AutocompleteMatch NewOmniboxResult(const std::string& url) {
  AutocompleteMatch result;

  result.relevance = 1.0;
  result.destination_url = GURL(url);
  result.stripped_destination_url = GURL(url);
  result.contents = u"contents";
  result.description = u"description";
  result.type = AutocompleteMatchType::BOOKMARK_TITLE;

  return result;
}

AutocompleteMatch NewAnswerResult(const std::string& url,
                                  omnibox::AnswerType answer_type) {
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::SuggestionAnswerMigration>
      scoped_config;
  AutocompleteMatch result;

  result.relevance = 1.0;
  result.destination_url = GURL(url);
  result.stripped_destination_url = GURL(url);
  result.contents = u"contents";
  result.description = u"description";
  if (scoped_config.Get().enabled) {
    omnibox::RichAnswerTemplate answer_template;
    answer_template.add_answers();
    result.answer_template = answer_template;
  } else {
    SuggestionAnswer answer;
    result.answer = answer;
  }
  result.answer_type = answer_type;

  return result;
}

AutocompleteMatch NewOpenTabResult(const std::string& url) {
  AutocompleteMatch result;

  result.relevance = 1.0;
  result.destination_url = GURL(url);
  result.stripped_destination_url = GURL(url);
  result.contents = u"contents";
  result.description = u"description";
  result.type = AutocompleteMatchType::OPEN_TAB;

  return result;
}

// A mock class for the AutoCompleteController.
class MockAutoCompleteController : public AutocompleteController {
 public:
  MockAutoCompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            0) {}
  MockAutoCompleteController(const MockAutoCompleteController&) = delete;
  MockAutoCompleteController& operator=(const MockAutoCompleteController&) =
      delete;
  ~MockAutoCompleteController() override = default;

  // Do nothing when it is called by OmniboxProvider.
  void Start(const AutocompleteInput& input) override {}
};

}  // namespace

class OmniboxProviderTest : public testing::Test {
 public:
  OmniboxProviderTest() = default;
  OmniboxProviderTest(const OmniboxProviderTest&) = delete;
  OmniboxProviderTest& operator=(const OmniboxProviderTest&) = delete;
  ~OmniboxProviderTest() override = default;

  void SetUp() override {
    // Create the profile manager and an active profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    // The profile needs a template URL service for history Omnibox results.
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        {TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(
                &TemplateURLServiceFactory::BuildInstanceFor)}});

    // Create client of our provider.
    search_controller_ = std::make_unique<TestSearchController>();

    // Create the object to actually test.
    list_controller_ =
        std::make_unique<::test::TestAppListControllerDelegate>();
    auto provider = std::make_unique<OmniboxProvider>(
        profile_, list_controller_.get(), /*provider_types=*/0);
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));

    std::unique_ptr<AutocompleteController> controller =
        std::make_unique<MockAutoCompleteController>();
    provider_->set_controller_for_test(std::move(controller));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    search_controller_.reset();
    list_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void ProduceResults(const AutocompleteResult& results) {
    provider_->PopulateFromACResult(std::move(results));
    base::RunLoop().RunUntilIdle();
  }

  // Starts a search and waits for the query to be sent
  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
    base::RunLoop().RunUntilIdle();
  }

  void DisableWebSearch() {
    ScopedDictPrefUpdate pref_update(
        profile_->GetPrefs(), ash::prefs::kLauncherSearchCategoryControlStatus);

    pref_update->Set(ash::GetAppListControlCategoryName(ControlCategory::kWeb),
                     false);
  }

 protected:
  std::unique_ptr<TestSearchController> search_controller_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<AppListControllerDelegate> list_controller_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  raw_ptr<OmniboxProvider> provider_;
};

// Test that results each instantiate a Chrome search result.
TEST_F(OmniboxProviderTest, Basic) {
  StartSearch(u"query");

  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOmniboxResult("https://example.com/result"));
  to_produce.emplace_back(NewAnswerResult(
      "https://example.com/answer", omnibox::AnswerType::ANSWER_TYPE_WEATHER));
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab"));
  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  // Results always appear after answer and open tab entries.
  ASSERT_EQ(3u, search_controller_->last_results().size());
  EXPECT_EQ("omnibox_answer://https://example.com/answer",
            search_controller_->last_results()[0]->id());
  EXPECT_EQ("opentab://https://example.com/open_tab",
            search_controller_->last_results()[1]->id());
  EXPECT_EQ("https://example.com/result",
            search_controller_->last_results()[2]->id());
}

// Test that newly-produced results supersede previous results.
TEST_F(OmniboxProviderTest, NewResults) {
  StartSearch(u"query");

  // Produce one result.
  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_1"));
  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  // Then produce another.
  StartSearch(u"query");

  to_produce.clear();
  AutocompleteResult new_result;
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_2"));
  new_result.AppendMatches(to_produce);
  ProduceResults(std::move(new_result));

  // Only newest result should be stored.
  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://example.com/open_tab_2",
            search_controller_->last_results()[0]->id());
}

// Test that invalid URLs aren't accepted.
TEST_F(OmniboxProviderTest, BadUrls) {
  StartSearch(u"query");

  // All results have bad URLs.
  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOmniboxResult(""));
  to_produce.emplace_back(
      NewAnswerResult("badscheme", omnibox::AnswerType::ANSWER_TYPE_WEATHER));
  to_produce.emplace_back(NewOpenTabResult("http://?k=v"));
  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  // None of the results should be accepted.
  EXPECT_TRUE(search_controller_->last_results().empty());
}

// Test that results with the same URL are deduplicated in the correct order.
TEST_F(OmniboxProviderTest, Deduplicate) {
  StartSearch(u"query");

  // A result that has the same URL as another result, but is a history (i.e.
  // higher-priority) type.
  auto history_result = NewOmniboxResult("https://example.com/result_1");
  history_result.contents = u"history";
  history_result.description = u"history description";
  history_result.type = AutocompleteMatchType::SEARCH_HISTORY;

  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOmniboxResult("https://example.com/result_2"));
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result_1"));
  to_produce.emplace_back(std::move(history_result));

  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  // Only the higher-priority (i.e. history) result for URL 1 should be kept.
  ASSERT_EQ(2u, search_controller_->last_results().size());
  EXPECT_EQ("https://example.com/result_1",
            search_controller_->last_results()[0]->id());
  EXPECT_EQ(u"history", search_controller_->last_results()[0]->title());
  EXPECT_EQ("https://example.com/result_2",
            search_controller_->last_results()[1]->id());
}

// Test that results aren't created for URLs for which there are other
// specialist producers.
TEST_F(OmniboxProviderTest, UnhandledUrls) {
  StartSearch(u"query");

  // Drive URLs aren't handled (_unless_ they are open tabs pointing to the
  // Drive website), and file URLs aren't handled.
  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOmniboxResult("https://drive.google.com/doc1"));
  to_produce.emplace_back(
      NewAnswerResult("https://docs.google.com/doc2",
                      omnibox::AnswerType::ANSWER_TYPE_FINANCE));
  to_produce.emplace_back(NewOpenTabResult("https://drive.google.com/doc1"));
  to_produce.emplace_back(NewOpenTabResult("https://docs.google.com/doc2"));
  to_produce.emplace_back(NewOpenTabResult("file:///docs/doc3"));
  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  ASSERT_EQ(2u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://drive.google.com/doc1",
            search_controller_->last_results()[0]->id());
  EXPECT_EQ("opentab://https://docs.google.com/doc2",
            search_controller_->last_results()[1]->id());
}

// Test that all non-answer results are filtered if web search is disabled in
// search control.
TEST_F(OmniboxProviderTest, WebSearchControl) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kLauncherSearchControl,
       ash::features::kFeatureManagementLocalImageSearch},
      {});
  DisableWebSearch();

  StartSearch(u"query");

  std::vector<AutocompleteMatch> to_produce;
  AutocompleteResult result;

  to_produce.emplace_back(NewOmniboxResult("https://example.com/result"));
  to_produce.emplace_back(NewAnswerResult(
      "https://example.com/answer", omnibox::AnswerType::ANSWER_TYPE_WEATHER));
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab"));
  result.AppendMatches(to_produce);
  ProduceResults(std::move(result));

  // Only answer result is returned.
  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("omnibox_answer://https://example.com/answer",
            search_controller_->last_results()[0]->id());
}

}  // namespace app_list::test
