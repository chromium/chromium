// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/personalization_app/enterprise_policy_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom-shared.h"
#include "ash/webui/personalization_app/search/search.mojom-test-utils.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "ash/webui/personalization_app/search/search_tag_registry.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom-test-utils.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::personalization_app {

namespace {

inline constexpr int kMaxNumResults = 3;

bool HasSearchResult(const std::vector<mojom::SearchResultPtr>& search_results,
                     const std::u16string& text) {
  for (const auto& result : search_results) {
    if (result->text == text) {
      return true;
    }
  }
  return false;
}

std::string SearchConceptIdToString(
    mojom::SearchConceptId search_result_concept) {
  return base::NumberToString(
      static_cast<std::underlying_type_t<mojom::SearchConceptId>>(
          search_result_concept));
}

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

class TestEnterprisePolicyDelegate : public EnterprisePolicyDelegate {
 public:
  TestEnterprisePolicyDelegate() = default;

  TestEnterprisePolicyDelegate(const TestEnterprisePolicyDelegate&) = delete;
  TestEnterprisePolicyDelegate& operator=(const TestEnterprisePolicyDelegate&) =
      delete;

  ~TestEnterprisePolicyDelegate() override = default;

  // EnterprisePolicyDelegate:
  bool IsUserImageEnterpriseManaged() const override {
    return is_user_image_enterprise_managed_;
  }

  bool IsWallpaperEnterpriseManaged() const override {
    return is_wallpaper_enterprise_managed_;
  }

  void AddObserver(EnterprisePolicyDelegate::Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(EnterprisePolicyDelegate::Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void SetIsUserImageEnterpriseManaged(bool is_user_image_enterprise_managed) {
    is_user_image_enterprise_managed_ = is_user_image_enterprise_managed;
    for (auto& observer : observer_list_) {
      observer.OnUserImageIsEnterpriseManagedChanged(
          is_user_image_enterprise_managed_);
    }
  }

  void SetIsWallpaperImageEnterpriseManaged(
      bool is_wallpaper_enterprise_managed) {
    is_wallpaper_enterprise_managed_ = is_wallpaper_enterprise_managed;
    for (auto& observer : observer_list_) {
      observer.OnWallpaperIsEnterpriseManagedChanged(
          is_wallpaper_enterprise_managed_);
    }
  }

 private:
  bool is_user_image_enterprise_managed_ = false;
  bool is_wallpaper_enterprise_managed_ = false;
  base::ObserverList<TestEnterprisePolicyDelegate::Observer> observer_list_;
};

}  // namespace

class PersonalizationAppSearchHandlerTest : public AshTestBase {
 protected:
  PersonalizationAppSearchHandlerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{::chromeos::features::kDarkLightMode,
                              ::ash::features::kAmbientModeFeature},
        /*disabled_features=*/{});
  }

  ~PersonalizationAppSearchHandlerTest() override = default;

  // ash::AshTestBase:
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    local_search_service_proxy_ =
        std::make_unique<local_search_service::LocalSearchServiceProxy>(
            /*for_testing=*/true);
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_->registry()->RegisterBooleanPref(
        ::ash::ambient::prefs::kAmbientModeEnabled, true);
    test_pref_service_->registry()->RegisterBooleanPref(
        ::ash::prefs::kDarkModeEnabled, false);

    search_handler_ = std::make_unique<SearchHandler>(
        *local_search_service_proxy_, test_pref_service_.get(),
        std::make_unique<TestEnterprisePolicyDelegate>());
    search_handler_->BindInterface(
        search_handler_remote_.BindNewPipeAndPassReceiver());
  }

  std::vector<mojom::SearchResultPtr> SimulateSearchCompleted(
      uint32_t max_num_results,
      local_search_service::ResponseStatus response_status,
      const absl::optional<std::vector<local_search_service::Result>>&
          local_search_service_results) {
    std::vector<mojom::SearchResultPtr> result;
    base::RunLoop loop;
    search_handler_->OnLocalSearchDone(
        base::BindLambdaForTesting(
            [&result, done = loop.QuitClosure()](
                std::vector<mojom::SearchResultPtr> search_results) {
              result = std::move(search_results);
              std::move(done).Run();
            }),
        max_num_results, response_status, local_search_service_results);
    return result;
  }

  SearchHandler* search_handler() { return search_handler_.get(); }

  SearchTagRegistry* search_tag_registry() {
    return search_handler_->search_tag_registry_.get();
  }

  TestEnterprisePolicyDelegate* test_search_delegate() {
    return static_cast<TestEnterprisePolicyDelegate*>(
        search_tag_registry()->enterprise_policy_delegate_.get());
  }

  mojo::Remote<mojom::SearchHandler>* search_handler_remote() {
    return &search_handler_remote_;
  }

  void SetDarkModeEnabled(bool enabled) {
    test_pref_service_->SetBoolean(::ash::prefs::kDarkModeEnabled, enabled);
  }

  std::vector<mojom::SearchResultPtr> RunSearch(int message_id) {
    std::vector<mojom::SearchResultPtr> search_results;
    std::u16string query = l10n_util::GetStringUTF16(message_id);
    // Search results match better if one character is subtracted.
    query.pop_back();
    mojom::SearchHandlerAsyncWaiter(search_handler_remote()->get())
        .Search(query, /*max_num_results=*/kMaxNumResults, &search_results);
    return search_results;
  }

  // Remove all existing search concepts saved in the registry.
  void ClearSearchTagRegistry() {
    local_search_service::mojom::IndexAsyncWaiter(
        search_tag_registry()->index_remote_.get())
        .ClearIndex();
    search_tag_registry()->result_id_to_search_concept_.clear();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
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
  ClearSearchTagRegistry();
  TestSearchResultsObserver test_observer;
  search_handler_remote()->get()->AddObserver(test_observer.GetRemote());
  SearchConcept search_concept = {
      .id = mojom::SearchConceptId::kChangeWallpaper,
      .message_id = IDS_PERSONALIZATION_APP_WALLPAPER_LABEL,
      .relative_url = "testing",
  };

  // Add a search concept.
  search_tag_registry()->UpdateSearchConcepts(
      {{&search_concept, /*add=*/true}});
  test_observer.WaitForSearchResultsChanged();

  EXPECT_EQ(&search_concept, search_tag_registry()->GetSearchConceptById(
                                 SearchConceptIdToString(search_concept.id)))
      << "Search concept was added";

  // Remove the search concept.
  search_tag_registry()->UpdateSearchConcepts({{&search_concept, false}});
  test_observer.WaitForSearchResultsChanged();

  EXPECT_EQ(nullptr,
            search_tag_registry()->GetSearchConceptById(
                base::NumberToString(IDS_PERSONALIZATION_APP_WALLPAPER_LABEL)))
      << "Search concept was removed";
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

TEST_F(PersonalizationAppSearchHandlerTest, HasBasicPersonalizationConcepts) {
  // Message id to expected relative url.
  std::unordered_map<int, std::string> message_ids_to_search = {
      {IDS_PERSONALIZATION_APP_SEARCH_RESULT_TITLE_ALT2, std::string()},
      {IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_WALLPAPER_ALT2,
       kWallpaperSubpageRelativeUrl},
      {IDS_PERSONALIZATION_APP_SEARCH_RESULT_CHANGE_DEVICE_ACCOUNT_IMAGE_ALT4,
       kUserSubpageRelativeUrl},
  };

  for (const auto& [message_id, expected_url] : message_ids_to_search) {
    std::vector<mojom::SearchResultPtr> search_results = RunSearch(message_id);
    EXPECT_EQ(1u, search_results.size());
    EXPECT_EQ(expected_url, search_results.front()->relative_url);
  }
}

TEST_F(PersonalizationAppSearchHandlerTest, RemovesAvatarForEnterprise) {
  EXPECT_TRUE(
      search_tag_registry()->GetSearchConceptById(SearchConceptIdToString(
          mojom::SearchConceptId::kChangeDeviceAccountImage)));

  TestSearchResultsObserver test_observer;
  search_handler_remote()->get()->AddObserver(test_observer.GetRemote());

  test_search_delegate()->SetIsUserImageEnterpriseManaged(true);

  test_observer.WaitForSearchResultsChanged();

  EXPECT_FALSE(
      search_tag_registry()->GetSearchConceptById(SearchConceptIdToString(
          mojom::SearchConceptId::kChangeDeviceAccountImage)));
}

TEST_F(PersonalizationAppSearchHandlerTest, RemovesWallpaperForEnterprise) {
  EXPECT_TRUE(search_tag_registry()->GetSearchConceptById(
      SearchConceptIdToString(mojom::SearchConceptId::kChangeWallpaper)));

  TestSearchResultsObserver test_observer;
  search_handler_remote()->get()->AddObserver(test_observer.GetRemote());

  test_search_delegate()->SetIsWallpaperImageEnterpriseManaged(true);

  test_observer.WaitForSearchResultsChanged();

  EXPECT_FALSE(search_tag_registry()->GetSearchConceptById(
      SearchConceptIdToString(mojom::SearchConceptId::kChangeWallpaper)));
}

TEST_F(PersonalizationAppSearchHandlerTest, HasDarkModeSearchResults) {
  {
    // Search one of the basic dark mode tags.
    std::vector<mojom::SearchResultPtr> dark_mode_results =
        RunSearch(IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_ALT2);

    EXPECT_EQ(dark_mode_results.front()->text,
              l10n_util::GetStringUTF16(
                  IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_ALT2));

    for (const auto& search_result : dark_mode_results) {
      // All dark mode results link to main page.
      EXPECT_EQ(std::string(), search_result->relative_url);
    }
  }

  // Terms to search when dark mode is on.
  std::vector<int> dark_mode_on_tags = {
      IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF,
      IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_OFF_ALT1,
  };
  // Terms to search when dark mode is off.
  std::vector<int> dark_mode_off_tags = {
      IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON,
      IDS_PERSONALIZATION_APP_SEARCH_RESULT_DARK_MODE_TURN_ON_ALT1,
  };

  {
    SetDarkModeEnabled(true);
    for (auto message_id : dark_mode_on_tags) {
      // Has expected search result because dark mode is on.
      auto expected_result = l10n_util::GetStringUTF16(message_id);
      EXPECT_TRUE(HasSearchResult(RunSearch(message_id), expected_result))
          << "Search result should be present: " << expected_result;
    }

    for (auto message_id : dark_mode_off_tags) {
      // Does not have dark mode off search result because dark mode is on.
      auto unexpected_result = l10n_util::GetStringUTF16(message_id);
      EXPECT_FALSE(HasSearchResult(RunSearch(message_id), unexpected_result))
          << "Search result should not be present: " << unexpected_result;
    }
  }

  {
    SetDarkModeEnabled(false);
    for (auto message_id : dark_mode_on_tags) {
      // Does not have dark mode on search result because dark mode is off.
      auto unexpected_result = l10n_util::GetStringUTF16(message_id);
      EXPECT_FALSE(HasSearchResult(RunSearch(message_id), unexpected_result))
          << "Search result should not be present: " << unexpected_result;
    }

    for (auto message_id : dark_mode_off_tags) {
      // Has expected search result because dark mode is off.
      auto expected_result = l10n_util::GetStringUTF16(message_id);
      EXPECT_TRUE(HasSearchResult(RunSearch(message_id), expected_result))
          << "Search result should be present: " << expected_result;
    }
  }
}

TEST_F(PersonalizationAppSearchHandlerTest, SortsAndTruncatesResults) {
  ClearSearchTagRegistry();
  // Test search concepts.
  std::vector<const SearchConcept> test_search_concepts = {
      {
          .id = mojom::SearchConceptId::kChangeWallpaper,
          .message_id = IDS_PERSONALIZATION_APP_WALLPAPER_LABEL,
      },
      {
          .id = mojom::SearchConceptId::kPersonalization,
          .message_id = IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE,
      },
      {
          .id = mojom::SearchConceptId::kAmbientMode,
          .message_id = IDS_PERSONALIZATION_APP_SCREENSAVER_LABEL,
      },
      {
          .id = mojom::SearchConceptId::kChangeDeviceAccountImage,
          .message_id = IDS_PERSONALIZATION_APP_AVATAR_LABEL,
      },
  };
  SearchTagRegistry::SearchConceptUpdates updates;
  for (const auto& search_concept : test_search_concepts) {
    updates.insert(std::make_pair(&search_concept, true));
  }
  search_tag_registry()->UpdateSearchConcepts(updates);

  // Scores that correspond to each of the |test_search_concepts|.
  std::vector<double> scores = {0.33, 0.5, 0.1, 0.99};
  std::vector<local_search_service::Result> fake_local_results;
  for (size_t i = 0; i < scores.size(); i++) {
    std::vector<local_search_service::Position> positions;
    positions.emplace_back(/*content_id=*/base::NumberToString(
                               test_search_concepts.at(i).message_id),
                           /*start=*/0, /*length=*/0);
    fake_local_results.emplace_back(
        /*id=*/SearchConceptIdToString(test_search_concepts.at(i).id),
        /*score=*/scores.at(i), std::move(positions));
  }

  constexpr size_t maxNumResults = 2;
  auto results = SimulateSearchCompleted(
      /*max_num_results=*/maxNumResults,
      local_search_service::ResponseStatus::kSuccess,
      absl::make_optional(fake_local_results));

  // Capped at |maxNumResults|.
  EXPECT_EQ(maxNumResults, results.size());

  // First result is top scoring result.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PERSONALIZATION_APP_AVATAR_LABEL),
            results.at(0)->text);
  EXPECT_EQ(0.99, results.at(0)->relevance_score);

  // Next result is second best score.
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE),
            results.at(1)->text);
  EXPECT_EQ(0.5, results.at(1)->relevance_score);
}

}  // namespace ash::personalization_app
