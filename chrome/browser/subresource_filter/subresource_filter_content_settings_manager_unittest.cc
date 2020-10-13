// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"

#include <set>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class SubresourceFilterContentSettingsManagerTest : public testing::Test {
 public:
  SubresourceFilterContentSettingsManagerTest() {}

  void SetUp() override {
    settings_manager_ =
        SubresourceFilterProfileContextFactory::GetForProfile(&testing_profile_)
            ->settings_manager();
    settings_manager_->set_should_use_smart_ui_for_testing(true);
  }

  HostContentSettingsMap* GetSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  SubresourceFilterContentSettingsManager* settings_manager() {
    return settings_manager_;
  }

  TestingProfile* profile() { return &testing_profile_; }

  ContentSetting GetContentSettingMatchingUrlWithEmptyPath(const GURL& url) {
    ContentSettingsForOneType host_settings;
    GetSettingsMap()->GetSettingsForOneType(ContentSettingsType::ADS,
                                            std::string(), &host_settings);
    GURL url_with_empty_path = url.GetWithEmptyPath();
    for (const auto& it : host_settings) {
      // Need GURL conversion to get rid of unnecessary default ports.
      if (GURL(it.primary_pattern.ToString()) == url_with_empty_path)
        return it.GetContentSetting();
    }
    return CONTENT_SETTING_DEFAULT;
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  TestingProfile testing_profile_;

  // Owned by the testing_profile_.
  SubresourceFilterContentSettingsManager* settings_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManagerTest);
};

// It isn't very cheap to initialize the history service. Tests that need it can
// use this harness.
class SubresourceFilterContentSettingsManagerHistoryTest
    : public SubresourceFilterContentSettingsManagerTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile()->CreateHistoryService());
    SubresourceFilterContentSettingsManagerTest::SetUp();
  }
};

TEST_F(SubresourceFilterContentSettingsManagerTest, LogDefaultSetting) {
  const char kDefaultContentSetting[] =
      "ContentSettings.DefaultSubresourceFilterSetting";
  // The histogram should be logged at profile creation.
  histogram_tester().ExpectTotalCount(kDefaultContentSetting, 1);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       SetSiteMetadataBasedOnActivation) {
  GURL url("https://example.test/");
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       NoSiteMetadata_SiteActivationFalse) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataForTesting(url, nullptr);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       MetadataExpiryFollowingActivation) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Advance the clock, metadata is cleared.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, nullptr);

  // Verify once metadata has expired we revert to metadata V1 and do not set
  // activation using the metadata activation key.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, nullptr);
}

// TODO(https://crbug.com/1113967): Remove test once ability to persist metadata
// is removed from the subresource filter content settings manager.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       MetadataExpiryFavorsAdsIntervention) {
  GURL url("https://example.test/");

  // Sets metadata expiry at kMaxPersistMetadataDuration from Time::Now().
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention);

  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration -
      base::TimeDelta::FromMinutes(1));

  // Setting metadata in safe browsing does not overwrite the existing
  // expiration set by the ads intervention.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);

  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_NE(dict, nullptr);

  // Advance the clock, metadata should be cleared.
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(1));

  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, nullptr);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdsInterventionMetadata_ExpiresAfterDuration) {
  GURL url("https://example.test/");
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention);
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Advance the clock, metadata is cleared.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kMaxPersistMetadataDuration);
  dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_EQ(dict, nullptr);
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdditionalMetadata_SetInMetadata) {
  GURL url("https://example.test/");
  const char kTestKey[] = "Test";
  auto additional_metadata = std::make_unique<base::DictionaryValue>();
  additional_metadata->SetBoolKey(kTestKey, true);

  // Set activation with additional metadata.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing,
      std::move(additional_metadata));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Verify metadata was actually persisted on site activation false.
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(dict->HasKey(kTestKey));
}

// TODO(https://crbug.com/1113967): Remove test once ability to persist metadata
// is removed from the subresource filter content settings manager.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       AdditionalMetadata_PersistedWithAdsIntervention) {
  GURL url("https://example.test/");
  const char kTestKey[] = "Test";
  auto additional_metadata = std::make_unique<base::DictionaryValue>();
  additional_metadata->SetBoolKey(kTestKey, true);

  // Set activation with additional metadata.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, true /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention,
      std::move(additional_metadata));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));

  // Verify metadata was actually persisted if another activation source
  // sets site activation false.
  settings_manager()->SetSiteMetadataBasedOnActivation(
      url, false /* is_activated */,
      SubresourceFilterContentSettingsManager::ActivationSource::kSafeBrowsing);
  EXPECT_FALSE(settings_manager()->GetSiteActivationFromMetadata(url));
  auto dict = settings_manager()->GetSiteMetadata(url);
  EXPECT_TRUE(dict->HasKey(kTestKey));
}

// Verifies that the site activation status is True when there is
// metadata without an explicit site activation status key value
// pair in the metadata.
TEST_F(SubresourceFilterContentSettingsManagerTest,
       SiteMetadataWithoutActivationStatus_SiteActivationTrue) {
  GURL url("https://example.test/");
  auto dict = std::make_unique<base::DictionaryValue>();
  settings_manager()->SetSiteMetadataForTesting(url, std::move(dict));
  EXPECT_TRUE(settings_manager()->GetSiteActivationFromMetadata(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest, SmartUI) {
  GURL url("https://example.test/");
  GURL url2("https://example.test/path");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingMatchingUrlWithEmptyPath(url));
  settings_manager()->OnDidShowUI(url);

  // Subsequent same-origin navigations should not show UI.
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url2));

  // Fast forward the clock.
  task_environment()->FastForwardBy(
      SubresourceFilterContentSettingsManager::kDelayBeforeShowingInfobarAgain);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
}

TEST_F(SubresourceFilterContentSettingsManagerTest, NoSmartUI) {
  settings_manager()->set_should_use_smart_ui_for_testing(false);

  GURL url("https://example.test/");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingMatchingUrlWithEmptyPath(url));
  settings_manager()->OnDidShowUI(url);

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url));
}

TEST_F(SubresourceFilterContentSettingsManagerTest,
       DefaultSettingsChange_NoWebsiteMetadata) {
  GURL url("https://example.test/");
  EXPECT_FALSE(settings_manager()->GetSiteMetadata(url));

  // Set the setting to the default, should not populate the metadata.
  GetSettingsMap()->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::ADS, std::string(),
      CONTENT_SETTING_DEFAULT);

  EXPECT_FALSE(settings_manager()->GetSiteMetadata(url));
}

// Tests that ClearSiteMetadata(origin) will result in clearing metadata for all
// sites whose origin is |origin|, but will not clear metadata for sites with
// different origins.
TEST_F(SubresourceFilterContentSettingsManagerTest, ClearSiteMetadata) {
  GURL initial_url("https://example.test/1");
  GURL same_origin_url("https://example.test/2");
  GURL different_origin_url("https://second_example.test/");

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(initial_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(different_origin_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearSiteMetadata(initial_url);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearSiteMetadata(different_origin_url);
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));
}

// Tests that ClearMetadataForAllSites() does indeed clear metadata for all
// sites.
TEST_F(SubresourceFilterContentSettingsManagerTest, ClearMetadataForAllSites) {
  GURL initial_url("https://example.test/1");
  GURL same_origin_url("https://example.test/2");
  GURL different_origin_url("https://second_example.test/");

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->OnDidShowUI(initial_url);
  settings_manager()->OnDidShowUI(different_origin_url);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(different_origin_url));

  settings_manager()->ClearMetadataForAllSites();
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(initial_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(same_origin_url));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(different_origin_url));
}

TEST_F(SubresourceFilterContentSettingsManagerHistoryTest,
       HistoryUrlDeleted_ClearsWebsiteSetting) {
  // Simulate a history already populated with a URL.
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  history_service->AddPage(GURL("https://already-browsed.com/"),
                           base::Time::Now(), history::SOURCE_BROWSED);

  // Ensure the website setting is set.
  GURL url1("https://example.test/1");
  GURL url2("https://example.test/2");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
  settings_manager()->OnDidShowUI(url1);

  // Simulate adding two page to the history for example.test.
  history_service->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
  history_service->AddPage(url2, base::Time::Now(), history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url2));

  // Deleting a URL from history while there are still other urls for the
  // same origin should not delete the setting.
  history_service->DeleteURLs({url1});
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url2));

  // Deleting all URLs of an origin from history should clear the setting for
  // this URL. Note that since there is another URL in the history this won't
  // clear all items.
  history_service->DeleteURLs({url2});
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
}

TEST_F(SubresourceFilterContentSettingsManagerHistoryTest,
       AllHistoryUrlDeleted_ClearsWebsiteSetting) {
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);

  GURL url1("https://example.test");
  GURL url2("https://example.test");
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
  settings_manager()->OnDidShowUI(url1);
  settings_manager()->OnDidShowUI(url2);

  // Simulate adding the pages to the history.
  history_service->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
  history_service->AddPage(url2, base::Time::Now(), history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_FALSE(settings_manager()->ShouldShowUIForSite(url2));

  // Deleting all the URLs should clear everything.
  base::RunLoop run_loop;
  base::CancelableTaskTracker task_tracker;
  history_service->ExpireHistoryBetween(std::set<GURL>(), base::Time(),
                                        base::Time(), /*user_initiated*/ true,
                                        run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
}

}  // namespace
