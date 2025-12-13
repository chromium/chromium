// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_top_site_provider_bridge.h"

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
                                    /*enterprise_shortcuts*/ nullptr,
                                    /*icon_cacher*/ nullptr,
                                    /*is_default_chrome_app_migrated*/ true) {}

  MockMostVisitedSites(const MockMostVisitedSites&) = delete;
  MockMostVisitedSites& operator=(const MockMostVisitedSites&) = delete;

  ~MockMostVisitedSites() override = default;

  MOCK_METHOD2(AddMostVisitedURLsObserver,
               void(Observer* observer, size_t max_num_sites));
  MOCK_METHOD1(RemoveMostVisitedURLsObserver, void(Observer* observer));
};

// Unit tests for AuxiliarySearchTopSiteProviderBridge.
class AuxiliarySearchTopSiteProviderBridgeTest : public ::testing::Test {
 public:
  AuxiliarySearchTopSiteProviderBridgeTest() {
    auto mock_most_visited_sites =
        std::make_unique<MockMostVisitedSites>(&testing_prefs_);
    auxiliary_search_top_site_provider_bridge_ =
        std::make_unique<AuxiliarySearchTopSiteProviderBridge>(
            std::move(mock_most_visited_sites));
  }

  AuxiliarySearchTopSiteProviderBridgeTest(
      const AuxiliarySearchTopSiteProviderBridgeTest&) = delete;
  AuxiliarySearchTopSiteProviderBridgeTest& operator=(
      const AuxiliarySearchTopSiteProviderBridgeTest&) = delete;

 protected:
  TestingPrefServiceSimple testing_prefs_;
  std::unique_ptr<AuxiliarySearchTopSiteProviderBridge>
      auxiliary_search_top_site_provider_bridge_;
};

TEST_F(AuxiliarySearchTopSiteProviderBridgeTest, AddAndRemoveObservers) {
  auto j_ref = base::android::JavaRef<jobject>();
  MockMostVisitedSites* mock_most_visited_sites =
      static_cast<MockMostVisitedSites*>(
          auxiliary_search_top_site_provider_bridge_->most_visited_sites_
              .get());

  // Verifies to start observing most visited sites when the first observer is
  // added.
  EXPECT_CALL(*mock_most_visited_sites,
              AddMostVisitedURLsObserver(testing::_, kMaxNumMostVisitedSites))
      .Times(1);
  auxiliary_search_top_site_provider_bridge_->SetObserverAndTrigger(nullptr,
                                                                    j_ref);

  // Verifies stop observing most visited sites after destroy.
  EXPECT_CALL(*mock_most_visited_sites,
              RemoveMostVisitedURLsObserver(testing::_))
      .Times(1);
  auxiliary_search_top_site_provider_bridge_->RemoveObserver();
}
