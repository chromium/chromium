// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/search/search.mojom-test-utils.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace personalization_app {

namespace {

inline constexpr int kMaxNumResults = 3;

class TestSearchResultsObserver : public mojom::SearchResultsObserver {
 public:
  TestSearchResultsObserver() = default;

  TestSearchResultsObserver(const TestSearchResultsObserver&) = delete;
  TestSearchResultsObserver& operator=(const TestSearchResultsObserver&) =
      delete;

  ~TestSearchResultsObserver() override = default;

  void OnSearchResultsChanged() override {
    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForSearchResultsChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::PendingRemote<mojom::SearchResultsObserver> GetRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  base::OnceClosure quit_callback_;
  mojo::Receiver<mojom::SearchResultsObserver> receiver_{this};
};

}  // namespace

class PersonalizationAppSearchHandlerTest : public testing::Test {
 protected:
  PersonalizationAppSearchHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::ash::features::kPersonalizationHub);
  }

  ~PersonalizationAppSearchHandlerTest() override = default;

  void SetUp() override {
    local_search_service_proxy_ = std::make_unique<
        ::chromeos::local_search_service::LocalSearchServiceProxy>(
        /*for_testing=*/true);
    search_handler_ =
        std::make_unique<SearchHandler>(*local_search_service_proxy_);
    search_handler_->BindInterface(
        search_handler_remote_.BindNewPipeAndPassReceiver());
  }

  SearchHandler* search_handler() { return search_handler_.get(); }

  SearchTagRegistry* search_tag_registry() {
    return search_handler_->search_tag_registry_.get();
  }

  mojo::Remote<mojom::SearchHandler>* search_handler_remote() {
    return &search_handler_remote_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<::chromeos::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<SearchHandler> search_handler_;
  mojo::Remote<mojom::SearchHandler> search_handler_remote_;
};

TEST_F(PersonalizationAppSearchHandlerTest, AnswersPersonalizationQuery) {
  std::vector<mojom::SearchResultPtr> search_results;
  mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
      .Search(u"testing", /*max_num_results=*/kMaxNumResults, &search_results);
  EXPECT_TRUE(search_results.empty());

  std::u16string title =
      l10n_util::GetStringUTF16(IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE);
  mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
      .Search(title, /*max_num_results=*/kMaxNumResults, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results.front()->text, title);
  EXPECT_GT(search_results.front()->relevance_score, 0.9);
}

TEST_F(PersonalizationAppSearchHandlerTest, ObserverFiresWhenResultsUpdated) {
  TestSearchResultsObserver test_observer;
  search_handler_remote()->get()->AddObserver(test_observer.GetRemote());
  std::vector<const SearchConcept> test_search_concepts = {
      {.message_id = IDS_PERSONALIZATION_APP_WALLPAPER_LABEL,
       .relative_url = "testing"}};
  search_tag_registry()->AddSearchConcepts(test_search_concepts);
  test_observer.WaitForSearchResultsChanged();
}

TEST_F(PersonalizationAppSearchHandlerTest, RespondsToAltQuery) {
  std::vector<mojom::SearchResultPtr> search_results;
  std::u16string search_query = l10n_util::GetStringUTF16(
      IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT1);

  mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
      .Search(search_query, /*max_num_results=*/kMaxNumResults,
              &search_results);

  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results.front()->text, search_query);
  EXPECT_GT(search_results.front()->relevance_score, 0.9);
}

}  // namespace personalization_app
}  // namespace ash
