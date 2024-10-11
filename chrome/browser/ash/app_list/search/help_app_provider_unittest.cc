// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/help_app_provider.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using SearchResultPtr = ::ash::help_app::mojom::SearchResultPtr;
using SearchResult = ::ash::help_app::mojom::SearchResult;

SearchResultPtr NewResult(const std::string& url,
                          const std::string& id,
                          const std::u16string& title,
                          double relevance) {
  SearchResultPtr result = SearchResult::New();

  result->id = id;
  result->title = title;
  result->relevance_score = relevance;
  result->url_path_with_parameters = url;
  result->main_category = u"Help";
  result->locale = "us";
  return result;
}

class MockSearchHandler : public ash::help_app::SearchHandler {
 public:
  MockSearchHandler(ash::help_app::SearchTagRegistry* search_tag_registry,
                    ash::local_search_service::LocalSearchServiceProxy*
                        local_search_service_proxy)
      : ash::help_app::SearchHandler(search_tag_registry,
                                     local_search_service_proxy),
        search_tag_registry_(search_tag_registry) {}
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

  raw_ptr<ash::help_app::SearchTagRegistry> search_tag_registry_;
  std::vector<SearchResultPtr> results_;
};
}  // namespace

class HelpAppProviderTest : public AppListTestBase {
 public:
  HelpAppProviderTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()),
        mock_handler_(&search_tag_registry_,
                      local_search_service_proxy_.get()) {}
  ~HelpAppProviderTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();
    proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile());

    apps::StubIconLoader stub_icon_loader;
    proxy_->OverrideInnerIconLoaderForTesting(&stub_icon_loader);

    // Insert dummy map values so that the stub_icon_loader knows of the app.
    stub_icon_loader.update_version_by_app_id_[web_app::kHelpAppId] = 1;

    auto provider = std::make_unique<HelpAppProvider>(profile());
    provider->MaybeInitialize(&mock_handler_);
    provider_ = provider.get();
    search_controller_.AddProvider(std::move(provider));
  }

  // Starts a search and waits for the query to be sent.
  void StartSearch(const std::u16string& query) {
    search_controller_.StartSearch(query);
    task_environment()->RunUntilIdle();
  }

  const app_list::Results& GetLatestResults() {
    return search_controller_.last_results();
  }

  MockSearchHandler* mock_handler() { return &mock_handler_; }

 private:
  session_manager::SessionManager session_manager_;
  TestSearchController search_controller_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::help_app::SearchTagRegistry search_tag_registry_;
  raw_ptr<HelpAppProvider> provider_ = nullptr;
  raw_ptr<apps::AppServiceProxy> proxy_;
  MockSearchHandler mock_handler_;
};

TEST_F(HelpAppProviderTest, Basic) {
  // Manually add in results from the mocked search handler.
  std::vector<SearchResultPtr> search_results;
  search_results.emplace_back(
      NewResult("www.print.com", "test-id-1", u"Printer Help", 0.5));
  search_results.emplace_back(
      NewResult("www.wifi.com", "test-id-2", u"WiFi Help", 0.6));
  mock_handler()->SetNextResults(std::move(search_results));

  // Should not return results if the query is too short.
  StartSearch(u"on");
  EXPECT_TRUE(GetLatestResults().empty());

  StartSearch(u"new");
  ASSERT_EQ(2u, GetLatestResults().size());
  EXPECT_EQ(GetLatestResults()[0]->title(), u"Printer Help");
  EXPECT_EQ(GetLatestResults()[0]->id(),
            ash::kChromeUIHelpAppURL + std::string("www.print.com"));
  EXPECT_EQ(GetLatestResults()[0]->relevance(), 0.5);
  EXPECT_EQ(GetLatestResults()[0]->result_type(), ResultType::kHelpApp);
  EXPECT_EQ(GetLatestResults()[0]->category(), Category::kHelp);
  EXPECT_EQ(GetLatestResults()[0]->display_type(), DisplayType::kList);
  EXPECT_EQ(GetLatestResults()[0]->metrics_type(), ash::HELP_APP_DEFAULT);
  EXPECT_EQ(GetLatestResults()[0]->details(), u"Help");
  EXPECT_EQ(GetLatestResults()[0]->icon().dimension, kAppIconDimension);
}

TEST_F(HelpAppProviderTest, WillFilterResultsBelowTheScoreThreshold) {
  // Manually add in results from the mocked search handler.
  std::vector<SearchResultPtr> search_results;
  search_results.emplace_back(
      NewResult("www.wifiHelp.com", "test-id-4", u"WiFi Helping Hand", 0.7));
  search_results.emplace_back(
      NewResult("www.print.com", "test-id-1", u"Printer Help", 0.3));
  search_results.emplace_back(
      NewResult("www.wifi.com", "test-id-2", u"WiFi Help", 0.6));
  mock_handler()->SetNextResults(std::move(search_results));

  // Should only return the results with a high enough relevance.
  StartSearch(u"query");
  ASSERT_EQ(2u, GetLatestResults().size());
  EXPECT_EQ(GetLatestResults()[0]->title(), u"WiFi Helping Hand");
  EXPECT_EQ(GetLatestResults()[1]->title(), u"WiFi Help");
}

}  // namespace app_list::test
