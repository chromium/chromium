// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"

#include <memory>

#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/prefs/testing_pref_service.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr size_t kMaxNumMostVisitedSites = 4;
}  // namespace

class MockMostVisitedSites : public ntp_tiles::MostVisitedSites {
 public:
  explicit MockMostVisitedSites(PrefService* prefs)
      : ntp_tiles::MostVisitedSites(prefs,
                                    /*identity_manager*/ nullptr,
                                    /*supervised_user_service*/ nullptr,
                                    /*top_sites*/ nullptr,
                                    /*popular_sites*/ nullptr,
                                    /*custom_links*/ nullptr,
                                    /*icon_cacher*/ nullptr,
                                    /*is_default_chrome_app_migrated*/ true,
                                    /*is_custom_links_mixable*/ false) {}

  MockMostVisitedSites(const MockMostVisitedSites&) = delete;
  MockMostVisitedSites& operator=(const MockMostVisitedSites&) = delete;

  ~MockMostVisitedSites() override = default;

  MOCK_METHOD2(AddMostVisitedURLsObserver,
               void(Observer* observer, size_t max_num_sites));
  MOCK_METHOD1(RemoveMostVisitedURLsObserver, void(Observer* observer));
};

// Unit tests for AuxiliarySearchProvider.
class AuxiliarySearchProviderTest : public ::testing::Test {
 public:
  AuxiliarySearchProviderTest() {
    auto mock_most_visited_sites =
        std::make_unique<MockMostVisitedSites>(&testing_prefs_);
    auxiliary_search_provider_ = std::make_unique<AuxiliarySearchProvider>(
        &mock_visited_url_ranking_service_, std::move(mock_most_visited_sites));
  }

  AuxiliarySearchProviderTest(const AuxiliarySearchProviderTest&) = delete;
  AuxiliarySearchProviderTest& operator=(const AuxiliarySearchProviderTest&) =
      delete;

 protected:
  TestingPrefServiceSimple testing_prefs_;
  visited_url_ranking::MockVisitedURLRankingService
      mock_visited_url_ranking_service_;
  std::unique_ptr<AuxiliarySearchProvider> auxiliary_search_provider_;
};

TEST_F(AuxiliarySearchProviderTest, AddAndRemoveObservers) {
  auto j_ref = base::android::JavaRef<jobject>();
  MockMostVisitedSites* mock_most_visited_sites =
      static_cast<MockMostVisitedSites*>(
          auxiliary_search_provider_->most_visited_sites_.get());

  // Verifies to start observing most visited sites when the first observer is
  // added.
  EXPECT_CALL(*mock_most_visited_sites,
              AddMostVisitedURLsObserver(testing::_, kMaxNumMostVisitedSites))
      .Times(1);
  int id1 = auxiliary_search_provider_->SetObserverAndTrigger(nullptr, j_ref);
  EXPECT_EQ(0, id1);
  EXPECT_EQ(1u, auxiliary_search_provider_->observers_map_.size());

  // Verifies not to call AddMostVisitedURLsObserver() again when more observer
  // is added.
  EXPECT_CALL(*mock_most_visited_sites,
              AddMostVisitedURLsObserver(testing::_, kMaxNumMostVisitedSites))
      .Times(0);
  auto j_ref_1 = base::android::JavaRef<jobject>();
  int id2 = auxiliary_search_provider_->SetObserverAndTrigger(nullptr, j_ref_1);
  EXPECT_EQ(1, id2);
  EXPECT_EQ(2u, auxiliary_search_provider_->observers_map_.size());

  // Verifies still observing the most visited sites when an observer is
  // removed.
  EXPECT_CALL(*mock_most_visited_sites,
              RemoveMostVisitedURLsObserver(testing::_))
      .Times(0);
  auxiliary_search_provider_->RemoveObserver(nullptr, id2);
  EXPECT_EQ(1u, auxiliary_search_provider_->observers_map_.size());

  // Verifies not to observe most visited sites after the last observer is
  // removed.
  EXPECT_CALL(*mock_most_visited_sites,
              RemoveMostVisitedURLsObserver(testing::_))
      .Times(1);
  auxiliary_search_provider_->RemoveObserver(nullptr, id1);
  EXPECT_TRUE(auxiliary_search_provider_->observers_map_.empty());
}
