// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/personalization_provider.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using SearchResultPtr = ::ash::personalization_app::mojom::SearchResultPtr;
using SearchResult = ::ash::personalization_app::mojom::SearchResult;
using SearchConceptId = ::ash::personalization_app::mojom::SearchConceptId;

SearchResultPtr NewResult(const std::string& url,
                          const std::u16string& text,
                          double relevance,
                          SearchConceptId id) {
  SearchResultPtr result = SearchResult::New();

  result->search_concept_id = id;
  result->relevance_score = relevance;
  result->relative_url = url;
  result->text = text;
  return result;
}

class MockSearchHandler : public ash::personalization_app::SearchHandler {
 public:
  MockSearchHandler() = default;
  ~MockSearchHandler() override = default;

  MockSearchHandler(const MockSearchHandler& other) = delete;
  MockSearchHandler& operator=(const MockSearchHandler& other) = delete;

  void Search(const std::u16string& query,
              uint32_t max_num_results,
              SearchCallback callback) override {
    std::move(callback).Run(std::move(results_));
  }

  // Manually add in results which will be returned by the Search function.
  void SetNextResults(std::vector<SearchResultPtr> results) {
    results_ = std::move(results);
  }

  std::vector<SearchResultPtr> results_;
};

}  // namespace

class PersonalizationProviderTest : public testing::Test {
 public:
  PersonalizationProviderTest() = default;
  PersonalizationProviderTest(const PersonalizationProviderTest&) = delete;
  PersonalizationProviderTest& operator=(const PersonalizationProviderTest&) =
      delete;
  ~PersonalizationProviderTest() override = default;

  void SetUp() override {
    search_controller_ = std::make_unique<TestSearchController>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("name");

    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
    apps::StubIconLoader stub_icon_loader;
    app_service_proxy_->OverrideInnerIconLoaderForTesting(&stub_icon_loader);

    // Insert dummy map values so that the stub_icon_loader knows of the app.
    stub_icon_loader.update_version_by_app_id_[web_app::kPersonalizationAppId] =
        1;

    mock_handler_ = std::make_unique<MockSearchHandler>();
    auto provider = std::make_unique<PersonalizationProvider>(profile_);
    provider->MaybeInitialize(mock_handler_.get());
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    search_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  const SearchProvider::Results& results() {
    return search_controller_->last_results();
  }

  MockSearchHandler* mock_handler() { return mock_handler_.get(); }

  // Starts a search and waits for the query to be sent.
  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<MockSearchHandler> mock_handler_;
  session_manager::SessionManager session_manager_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<::apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_;
  raw_ptr<PersonalizationProvider> provider_;
};

TEST_F(PersonalizationProviderTest, Basic) {
  // Manually add in results from the mocked search handler.
  std::vector<SearchResultPtr> personalization_results;
  personalization_results.emplace_back(NewResult(
      "www.ambient.com", u"Ambient Mode", 0.6, SearchConceptId::kAmbientMode));
  personalization_results.emplace_back(
      NewResult("www.me.com", u"Personalization", 0.7,
                SearchConceptId::kPersonalization));
  mock_handler()->SetNextResults(std::move(personalization_results));

  // Should not return results if the query is too short.
  StartSearch(u"on");
  EXPECT_TRUE(results().empty());

  StartSearch(u"query");
  ASSERT_EQ(2u, results().size());
  EXPECT_EQ(results()[0]->title(), u"Ambient Mode");
  EXPECT_EQ(results()[0]->id(),
            ::ash::personalization_app::kChromeUIPersonalizationAppURL +
                std::string("www.ambient.com"));
  EXPECT_EQ(results()[0]->relevance(), 0.6);
  EXPECT_EQ(results()[0]->result_type(), ResultType::kPersonalization);
  EXPECT_EQ(results()[0]->category(), Category::kSettings);
  EXPECT_EQ(results()[0]->display_type(), DisplayType::kList);
  EXPECT_EQ(results()[0]->metrics_type(), ash::PERSONALIZATION);
  EXPECT_EQ(results()[0]->icon().dimension, kAppIconDimension);
  EXPECT_EQ(results()[1]->title(), u"Personalization");
}
}  // namespace app_list::test
