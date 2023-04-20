// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
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

// Our main test fixture. Provides `search_producer_` with which tests can
// produce "lacros-side" results, and `search_controller_` with which tests can
// read the output of the `OmniboxLacrosProvider`.
class OmniboxLacrosProviderTest : public testing::Test {
 public:
  OmniboxLacrosProviderTest() = default;
  OmniboxLacrosProviderTest(const OmniboxLacrosProviderTest&) = delete;
  OmniboxLacrosProviderTest& operator=(const OmniboxLacrosProviderTest&) =
      delete;
  ~OmniboxLacrosProviderTest() override = default;

  void SetUp() override {
    // The `CrosapiManager` requires:
    //   - An active profile manager and profile.
    //   - Various services disabled that can't run in unit tests.
    //   - A valid login state.

    // Create the profile manager and an active profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    // The profile needs a template URL service for history Omnibox results.
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        {{TemplateURLServiceFactory::GetInstance(),
          base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}});

    // The idle service has dependencies we can't instantiate in unit tests.
    crosapi::IdleServiceAsh::DisableForTesting();

    // The crosapi manager reads the global login state.
    ash::LoginState::Initialize();

    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();

    // Create fake lacros-side logic.
    search_producer_ = std::make_unique<TestSearchResultProducer>();
    crosapi_manager_->crosapi_ash()
        ->search_provider_ash()
        ->RegisterSearchController(search_producer_->BindToRemote());

    // Create client of our provider.
    search_controller_ = std::make_unique<TestSearchController>();

    // Create the object to actually test.
    omnibox_provider_ = std::make_unique<OmniboxLacrosProvider>(
        profile_, &list_controller_, crosapi_manager_.get());
    omnibox_provider_->set_controller(search_controller_.get());
  }

  void TearDown() override {
    omnibox_provider_.reset();
    search_controller_.reset();
    search_producer_.reset();
    crosapi_manager_.reset();
    ash::LoginState::Shutdown();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  // Tells the producer to produce the given results, then waits for the results
  // to be transmitted over their Mojo pipe.
  void ProduceResults(std::vector<cam::SearchResultPtr> results) {
    search_producer_->ProduceResults(std::move(results));
    base::RunLoop().RunUntilIdle();
  }

  // Starts a search and waits for the query to be sent to "lacros" over a Mojo
  // pipe.
  void StartSearch(const std::u16string& query) {
    omnibox_provider_->Start(query);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<TestSearchResultProducer> search_producer_;
  std::unique_ptr<TestSearchController> search_controller_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestAppListControllerDelegate list_controller_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;

  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;

  std::unique_ptr<OmniboxLacrosProvider> omnibox_provider_;
};

// Test that results sent from lacros each instantiate a Chrome search result.
TEST_F(OmniboxLacrosProviderTest, Basic) {
  StartSearch(u"query");
  EXPECT_EQ(u"query", search_producer_->last_query());

  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult("https://example.com/result"));
  to_produce.emplace_back(NewAnswerResult(
      "https://example.com/answer", cam::SearchResult::AnswerType::kWeather));
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab"));
  ProduceResults(std::move(to_produce));

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
  StartSearch(u"query");

  // Produce one result.
  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_1"));
  ProduceResults(std::move(to_produce));

  StartSearch(u"query2");

  // Then produce another.
  to_produce.clear();
  to_produce.emplace_back(NewOpenTabResult("https://example.com/open_tab_2"));
  ProduceResults(std::move(to_produce));

  // Only newest result should be stored.
  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://example.com/open_tab_2",
            search_controller_->last_results()[0]->id());
}

// Test that invalid URLs aren't accepted.
TEST_F(OmniboxLacrosProviderTest, BadUrls) {
  StartSearch(u"query");

  // All results have bad URLs.
  std::vector<cam::SearchResultPtr> to_produce;
  to_produce.emplace_back(NewOmniboxResult(""));
  to_produce.emplace_back(
      NewAnswerResult("badscheme", cam::SearchResult::AnswerType::kWeather));
  to_produce.emplace_back(NewOpenTabResult("http://?k=v"));
  ProduceResults(std::move(to_produce));

  // None of the results should be accepted.
  EXPECT_TRUE(search_controller_->last_results().empty());
}

// Test that results with the same URL are deduplicated in the correct order.
TEST_F(OmniboxLacrosProviderTest, Deduplicate) {
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

  ProduceResults(std::move(to_produce));

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
  ProduceResults(std::move(to_produce));

  ASSERT_EQ(2u, search_controller_->last_results().size());
  EXPECT_EQ("opentab://https://drive.google.com/doc1",
            search_controller_->last_results()[0]->id());
  EXPECT_EQ("opentab://https://docs.google.com/doc2",
            search_controller_->last_results()[1]->id());
}

// A secondary test fixture which does not have any search producer connected to
// the crosapi manager.
class OmniboxLacrosProviderNoCrosAPITest : public testing::Test {
 public:
  OmniboxLacrosProviderNoCrosAPITest() = default;
  OmniboxLacrosProviderNoCrosAPITest(
      const OmniboxLacrosProviderNoCrosAPITest&) = delete;
  OmniboxLacrosProviderNoCrosAPITest& operator=(
      const OmniboxLacrosProviderNoCrosAPITest&) = delete;
  ~OmniboxLacrosProviderNoCrosAPITest() override = default;

  void SetUp() override {
    // Create the profile manager and an active profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    // The profile needs a template URL service for history Omnibox results.
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        {{TemplateURLServiceFactory::GetInstance(),
          base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}});

    // The idle service has dependencies we can't instantiate in unit tests.
    crosapi::IdleServiceAsh::DisableForTesting();

    // The crosapi manager reads the global login state.
    ash::LoginState::Initialize();

    crosapi_manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();

    // Create fake lacros-side logic.
    crosapi_manager_->crosapi_ash()->search_provider_ash();

    // Create client of our provider.
    search_controller_ = std::make_unique<TestSearchController>();

    // Create the object to actually test.
    omnibox_provider_ = std::make_unique<OmniboxLacrosProvider>(
        profile_, &list_controller_, crosapi_manager_.get());
    omnibox_provider_->set_controller(search_controller_.get());
  }

  void TearDown() override {
    omnibox_provider_.reset();
    search_controller_.reset();
    crosapi_manager_.reset();
    ash::LoginState::Shutdown();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  // Starts a search and waits for the query to be sent to "lacros" over a Mojo
  // pipe.
  void StartSearch(const std::u16string& query) {
    omnibox_provider_->Start(query);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<TestSearchController> search_controller_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestAppListControllerDelegate list_controller_;
  std::unique_ptr<crosapi::CrosapiManager> crosapi_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<OmniboxLacrosProvider> omnibox_provider_;
};

TEST_F(OmniboxLacrosProviderNoCrosAPITest, SystemURLsWorkWithNoSearchProvider) {
  StartSearch(u"os://flags");

  ASSERT_EQ(1u, search_controller_->last_results().size());
  EXPECT_EQ("os://flags", search_controller_->last_results()[0]->id());
  EXPECT_EQ(1.0, search_controller_->last_results()[0]->relevance());
  EXPECT_EQ(u"os://flags", search_controller_->last_results()[0]->title());
  EXPECT_EQ(u"", search_controller_->last_results()[0]->details());
}

}  // namespace app_list::test
