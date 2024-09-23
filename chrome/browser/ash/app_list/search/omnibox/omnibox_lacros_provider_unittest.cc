// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list::test {
namespace {

namespace cam = ::crosapi::mojom;
using ::test::TestAppListControllerDelegate;

// Helper functions to populate search results.

cam::SearchResultPtr NewOmniboxResult(const std::string& url) {
  auto result = cam::SearchResult::New();

  result->type = cam::SearchResultType::kOmniboxResult;
  result->relevance = 1.0;
  result->destination_url = GURL(url);
  result->stripped_destination_url = GURL(url);
  result->is_answer = cam::SearchResult::OptionalBool::kFalse;
  result->contents = u"contents";
  result->contents_type = cam::SearchResult::TextType::kUnset;
  result->description = u"description";
  result->description_type = cam::SearchResult::TextType::kUnset;
  result->is_omnibox_search = cam::SearchResult::OptionalBool::kFalse;
  result->omnibox_type = cam::SearchResult::OmniboxType::kDomain;
  result->metrics_type = cam::SearchResult::MetricsType::kWhatYouTyped;

  return result;
}

cam::SearchResultPtr NewAnswerResult(
    const std::string& url,
    cam::SearchResult::AnswerType answer_type) {
  auto result = cam::SearchResult::New();

  result->type = cam::SearchResultType::kOmniboxResult;
  result->relevance = 1.0;
  result->destination_url = GURL(url);
  result->stripped_destination_url = GURL(url);
  result->is_answer = cam::SearchResult::OptionalBool::kTrue;
  result->contents = u"contents";
  result->contents_type = cam::SearchResult::TextType::kUnset;
  result->description = u"description";
  result->description_type = cam::SearchResult::TextType::kUnset;
  result->is_omnibox_search = cam::SearchResult::OptionalBool::kFalse;
  result->answer_type = answer_type;

  return result;
}

cam::SearchResultPtr NewOpenTabResult(const std::string& url) {
  auto result = cam::SearchResult::New();

  result->type = cam::SearchResultType::kOmniboxResult;
  result->relevance = 1.0;
  result->destination_url = GURL(url);
  result->stripped_destination_url = GURL(url);
  result->is_answer = cam::SearchResult::OptionalBool::kFalse;
  result->contents = u"contents";
  result->contents_type = cam::SearchResult::TextType::kUnset;
  result->description = u"description";
  result->description_type = cam::SearchResult::TextType::kUnset;
  result->is_omnibox_search = cam::SearchResult::OptionalBool::kFalse;
  result->omnibox_type = cam::SearchResult::OmniboxType::kOpenTab;

  return result;
}

// A class that emulates lacros-side logic by producing and transmitting search
// results. Named "producer" because "controller", "provider" and "publisher"
// are all taken (sometimes more than once!).
class TestSearchResultProducer : public cam::SearchController {
 public:
  TestSearchResultProducer() = default;
  TestSearchResultProducer(const TestSearchResultProducer&) = delete;
  TestSearchResultProducer& operator=(const TestSearchResultProducer&) = delete;
  ~TestSearchResultProducer() override = default;

  mojo::PendingRemote<cam::SearchController> BindToRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ProduceResults(std::vector<cam::SearchResultPtr> results) {
    // Bad search statuses aren't tested because the `OmniboxLacrosProvider`
    // isn't responsible for handling them.
    publisher_->OnSearchResultsReceived(cam::SearchStatus::kDone,
                                        std::move(results));
  }

  const std::u16string& last_query() { return last_query_; }

 private:
  // cam::SearchController overrides:
  void Search(const std::u16string& query, SearchCallback callback) override {
    last_query_ = query;

    // Reset the remote and send a new pending receiver to ash.
    publisher_.reset();
    std::move(callback).Run(publisher_.BindNewEndpointAndPassReceiver());
  }

  mojo::Receiver<cam::SearchController> receiver_{this};
  mojo::AssociatedRemote<cam::SearchResultsPublisher> publisher_;
  std::u16string last_query_;
};

}  // namespace

// Our main test fixture. Provides `search_controller_` with which tests can
// read the output of the `OmniboxLacrosProvider`.
class OmniboxLacrosProviderTest : public testing::Test {
 public:
  OmniboxLacrosProviderTest() = default;
  OmniboxLacrosProviderTest(const OmniboxLacrosProviderTest&) = delete;
  OmniboxLacrosProviderTest& operator=(const OmniboxLacrosProviderTest&) =
      delete;
  ~OmniboxLacrosProviderTest() override = default;

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
    auto omnibox_provider = std::make_unique<OmniboxLacrosProvider>(
        profile_, &list_controller_, base::BindLambdaForTesting([this] {
          return crosapi_search_controller_ash_.get();
        }));
    omnibox_provider_ = omnibox_provider.get();
    search_controller_->AddProvider(std::move(omnibox_provider));
  }

  void TearDown() override {
    omnibox_provider_ = nullptr;
    search_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void RegisterSearchController(
      mojo::PendingRemote<cam::SearchController> search_controller) {
    crosapi_search_controller_ash_ =
        std::make_unique<crosapi::SearchControllerAsh>(
            std::move(search_controller));
  }

  // Starts a search and waits for the query to be sent to "lacros" over a Mojo
  // pipe.
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
  TestAppListControllerDelegate list_controller_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<crosapi::SearchControllerAsh> crosapi_search_controller_ash_;

  raw_ptr<OmniboxLacrosProvider> omnibox_provider_;
};

// Test that results sent from lacros each instantiate a Chrome search result.
TEST_F(OmniboxLacrosProviderTest, Basic) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  StartSearch(u"query");
  EXPECT_EQ(u"query", search_producer->last_query());

  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result"));
  to_produce.emplace_back(NewAnswerResult(
      "https://example.com/answer", cam::SearchResult::AnswerType::kWeather));
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

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
TEST_F(OmniboxLacrosProviderTest, NewResults) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  StartSearch(u"query");

  // Produce one result.
  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_1"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

  StartSearch(u"query2");

  // Then produce another.
  to_produce.clear();
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_2"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

  // Only newest result should be stored.
  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://example.com/open_tab_2",
            search_controller_->last_results()[0]->id());
}

// Test that invalid URLs aren't accepted.
TEST_F(OmniboxLacrosProviderTest, BadUrls) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  StartSearch(u"query");

  // All results have bad URLs.
  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult(""));
  to_produce.emplace_back(
      NewAnswerResult("badscheme", cam::SearchResult::AnswerType::kWeather));
  to_produce.emplace_back(NewOpenTabResult("http://?k=v"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

  // None of the results should be accepted.
  EXPECT_TRUE(search_controller_->last_results().empty());
}

// Test that results with the same URL are deduplicated in the correct order.
TEST_F(OmniboxLacrosProviderTest, Deduplicate) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  StartSearch(u"query");

  // A result that has the same URL as another result, but is a history (i.e.
  // higher-priority) type.
  auto history_result = NewOmniboxResult("https://example.com/result_1");
  history_result->contents = u"history";
  history_result->is_omnibox_search = cam::SearchResult::OptionalBool::kTrue;
  history_result->omnibox_type = cam::SearchResult::OmniboxType::kHistory;
  history_result->metrics_type = cam::SearchResult::MetricsType::kSearchHistory;

  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result_2"));
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result_1"));
  to_produce.emplace_back(std::move(history_result));

  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

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
TEST_F(OmniboxLacrosProviderTest, UnhandledUrls) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  StartSearch(u"query");

  // Drive URLs aren't handled (_unless_ they are open tabs pointing to the
  // Drive website), and file URLs aren't handled.
  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult("https://drive.google.com/doc1"));
  to_produce.emplace_back(NewAnswerResult(
      "https://docs.google.com/doc2", cam::SearchResult::AnswerType::kFinance));
  to_produce.emplace_back(NewOpenTabResult("https://drive.google.com/doc1"));
  to_produce.emplace_back(NewOpenTabResult("https://docs.google.com/doc2"));
  to_produce.emplace_back(NewOpenTabResult("file:///docs/doc3"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://drive.google.com/doc1",
            search_controller_->last_results()[0]->id());
  EXPECT_EQ("opentab://https://docs.google.com/doc2",
            search_controller_->last_results()[1]->id());
}

// Test that all non-answer results are filtered if web search is disabled in
// search control.
TEST_F(OmniboxLacrosProviderTest, WebSearchControl) {
  auto search_producer = std::make_unique<TestSearchResultProducer>();
  RegisterSearchController(search_producer->BindToRemote());
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kLauncherSearchControl,
       ash::features::kFeatureManagementLocalImageSearch},
      {});
  DisableWebSearch();

  StartSearch(u"query");
  EXPECT_EQ(u"query", search_producer->last_query());

  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result"));
  to_produce.emplace_back(NewAnswerResult(
      "https://example.com/answer", cam::SearchResult::AnswerType::kWeather));
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab"));
  search_producer->ProduceResults(std::move(to_produce));
  base::RunLoop().RunUntilIdle();

  // Results always appear after answer and open tab entries.
  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("omnibox_answer://https://example.com/answer",
            search_controller_->last_results()[0]->id());
}

TEST_F(OmniboxLacrosProviderTest, SystemURLsWorkWithNoSearchProvider) {
  StartSearch(u"os://flags");

  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("os://flags", search_controller_->last_results()[0]->id());
  EXPECT_EQ(1.0, search_controller_->last_results()[0]->relevance());
  EXPECT_EQ(u"os://flags", search_controller_->last_results()[0]->title());
  EXPECT_EQ(u"", search_controller_->last_results()[0]->details());
}

}  // namespace app_list::test
