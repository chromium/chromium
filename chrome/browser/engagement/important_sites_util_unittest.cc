// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_util.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sample_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/40666874): Disabled all tests because they lead the flakiness
// dashboard. The root cause is documented in the bug.
#if !BUILDFLAG(IS_ANDROID)

namespace site_engagement {

namespace {
using BookmarkModel = bookmarks::BookmarkModel;
using ImportantDomainInfo = ImportantSitesUtil::ImportantDomainInfo;

const size_t kNumImportantSites = 5;

// We only need to reproduce the values that we are testing. The values here
// need to match the values in important_sites_util.
enum ImportantReasonForTesting {
  ENGAGEMENT = 0,
  BOOKMARKS = 2,
  NOTIFICATIONS = 4
};

}  // namespace

class ImportantSitesUtilTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SiteEngagementScore::SetParamValuesForTesting();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                HistoryServiceFactory::GetDefaultFactory()}};
  }

  void AddContentSetting(ContentSettingsType type,
                         ContentSetting setting,
                         const GURL& origin) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromURLNoWildcard(origin),
            ContentSettingsPattern::Wildcard(), type, setting);
    EXPECT_EQ(setting, HostContentSettingsMapFactory::GetForProfile(profile())
                           ->GetContentSetting(origin, GURL(), type));
  }

  void AddBookmark(const GURL& origin) {
    if (!model_) {
      model_ = BookmarkModelFactory::GetForBrowserContext(profile());
      bookmarks::test::WaitForBookmarkModelToLoad(model_);
    }

    model_->AddURL(model_->bookmark_bar_node(), 0,
                   base::ASCIIToUTF16(origin.spec()), origin);
  }

  void ExpectImportantResultsEq(
      const std::vector<std::string>& domains,
      const std::vector<GURL>& expected_sorted_origins,
      const std::vector<ImportantDomainInfo>& important_sites) {
    ASSERT_EQ(domains.size(), important_sites.size());
    ASSERT_EQ(expected_sorted_origins.size(), important_sites.size());
    for (size_t i = 0; i < important_sites.size(); i++) {
      EXPECT_EQ(domains[i], important_sites[i].registerable_domain);
      EXPECT_EQ(expected_sorted_origins[i], important_sites[i].example_origin);
    }
  }

  void ExpectImportantResultsEqualUnordered(
      const std::vector<std::string>& domains,
      const std::vector<GURL>& expected_sorted_origins,
      const std::vector<ImportantDomainInfo>& important_sites) {
    ASSERT_EQ(domains.size(), important_sites.size());
    ASSERT_EQ(expected_sorted_origins.size(), important_sites.size());

    std::vector<std::string> actual_domains;
    std::vector<GURL> actual_origins;
    for (size_t i = 0; i < important_sites.size(); i++) {
      actual_domains.push_back(important_sites[i].registerable_domain);
      actual_origins.push_back(important_sites[i].example_origin);
    }
    EXPECT_THAT(actual_domains, testing::UnorderedElementsAreArray(domains));
    EXPECT_THAT(actual_origins,
                testing::UnorderedElementsAreArray(expected_sorted_origins));
  }

 private:
  raw_ptr<BookmarkModel, DanglingUntriaged> model_ = nullptr;
};

TEST_F(ImportantSitesUtilTest, TestNoImportantSites) {
  EXPECT_TRUE(ImportantSitesUtil::GetImportantRegisterableDomains(
                  profile(), kNumImportantSites)
                  .empty());
}

TEST_F(ImportantSitesUtilTest, SourceOrdering) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://www.chrome.com/");
  GURL url5("https://www.example.com/");
  GURL url6("https://youtube.com/");
  GURL url7("https://foo.bar/");

  service->ResetBaseScoreForURL(url1, 5);
  service->ResetBaseScoreForURL(url2, 2);  // Below medium engagement (5).
  service->ResetBaseScoreForURL(url3, 7);
  service->ResetBaseScoreForURL(url4, 8);
  service->ResetBaseScoreForURL(url5, 9);
  service->ResetBaseScoreForURL(url6, 1);  // Below the medium engagement (5).
  service->ResetBaseScoreForURL(url7, 11);

  // Here we should have:
  // 1: removed domains below minimum engagement,
  // 2: combined the google.com entries, and
  // 3: sorted by the score.
  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);
  std::vector<std::string> expected_sorted_domains = {
      "foo.bar", "example.com", "chrome.com", "google.com"};
  std::vector<GURL> expected_sorted_origins = {url7, url5, url4, url3};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);

  // Test that notifications get moved to the front.
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url6);
  // BLOCK'ed sites don't count. We want to make sure we only bump sites that
  // were granted the permsion.
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_BLOCK,
                    url1);

  // Same as above, but the site with notifications should be at the front.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  expected_sorted_domains = {"youtube.com", "foo.bar", "example.com",
                             "chrome.com", "google.com"};
  expected_sorted_origins = {url6, url7, url5, url4, url3};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);

  // Test that bookmarks move above engagements and below notifications.
  AddBookmark(url1);
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  expected_sorted_domains = {"youtube.com", "google.com", "foo.bar",
                             "example.com", "chrome.com"};
  expected_sorted_origins = {url6, url3, url7, url5, url4};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
}

TEST_F(ImportantSitesUtilTest, TooManyBookmarks) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://www.chrome.com/");
  GURL url5("https://www.example.com/");
  GURL url6("https://youtube.com/");
  GURL url7("https://foo.bar/");

  // Add some as bookmarks.
  AddBookmark(url1);
  AddBookmark(url2);
  AddBookmark(url3);
  AddBookmark(url4);
  AddBookmark(url5);

  // We have just below our limit, so all sites are important (the first three
  // origins collapse, so we end up with 3).
  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);
  EXPECT_EQ(3u, important_sites.size());

  // Add the rest, which should put us over the limit.
  AddBookmark(url6);
  AddBookmark(url7);
  // Too many bookmarks! Nothing shows up now.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  EXPECT_EQ(0u, important_sites.size());

  // If we add some site engagement, they should show up (even though the site
  // engagement score is too low for a signal by itself).
  service->ResetBaseScoreForURL(url1, 2);
  service->ResetBaseScoreForURL(url4, 3);
  service->ResetBaseScoreForURL(url7, 0);

  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  ASSERT_EQ(2u, important_sites.size());
  std::vector<std::string> expected_sorted_domains = {"google.com",
                                                      "chrome.com"};
  std::vector<GURL> expected_sorted_origins = {url1, url4};
  ExpectImportantResultsEqualUnordered(
      expected_sorted_domains, expected_sorted_origins, important_sites);
}

TEST_F(ImportantSitesUtilTest, Suppressing) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("http://www.gmail.com/");

  // Set a bunch of positive signals.
  service->ResetBaseScoreForURL(url1, 5);
  AddBookmark(url2);
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url1);

  // Important fetch 1.
  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);
  std::vector<std::string> expected_sorted_domains = {"google.com",
                                                      "gmail.com"};
  std::vector<GURL> expected_sorted_origins = {url1, url2};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
  ASSERT_EQ(2u, important_sites.size());
  // Record ignore twice.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});

  // Important fetch 2.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
  // We shouldn't suppress after first two times.
  ASSERT_EQ(2u, important_sites.size());

  // Record ignore 3rd time.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});

  // Important fetch 3. Google.com should be suppressed now.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);

  ASSERT_EQ(1u, important_sites.size());
  expected_sorted_domains = {"gmail.com"};
  expected_sorted_origins = {url2};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
}

TEST_F(ImportantSitesUtilTest, SuppressingReset) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("http://www.gmail.com/");

  // Set a bunch of positive signals.
  service->ResetBaseScoreForURL(url1, 5);
  AddBookmark(url2);
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url1);

  // Important fetch 1.
  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);
  ASSERT_EQ(2u, important_sites.size());
  // Record ignore twice.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});

  // Important fetch, we should still be there.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  std::vector<std::string> expected_sorted_domains = {"google.com",
                                                      "gmail.com"};
  std::vector<GURL> expected_sorted_origins = {url1, url2};
  ASSERT_EQ(2u, important_sites.size());
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);

  // Record NOT ignored.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"google.com", "gmail.com"},
      {important_sites[0].reason_bitfield, important_sites[1].reason_bitfield},
      std::vector<std::string>(), std::vector<int32_t>());

  // Record ignored twice again
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});

  // Important fetch, we should still be there.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);

  // Record ignored 3rd time in a row.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"gmail.com"}, {important_sites[1].reason_bitfield},
      {"google.com"}, {important_sites[0].reason_bitfield});

  // Suppressed now.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  ASSERT_EQ(1u, important_sites.size());
  expected_sorted_domains = {"gmail.com"};
  expected_sorted_origins = {url2};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
}

TEST_F(ImportantSitesUtilTest, Metrics) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);
  base::HistogramTester histogram_tester;

  GURL url1("http://www.google.com/");
  service->ResetBaseScoreForURL(url1, 5);
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url1);

  GURL url2("http://www.youtube.com/");
  AddBookmark(url2);

  GURL url3("http://www.bad.com/");
  AddBookmark(url3);

  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);

  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), {"google.com", "youtube.com"},
      {important_sites[0].reason_bitfield, important_sites[1].reason_bitfield},
      {"bad.com"}, {important_sites[2].reason_bitfield});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Storage.ImportantSites.CBDChosenReason"),
      testing::ElementsAre(base::Bucket(ENGAGEMENT, 1),
                           base::Bucket(BOOKMARKS, 1),
                           base::Bucket(NOTIFICATIONS, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Storage.ImportantSites.CBDIgnoredReason"),
      testing::ElementsAre(base::Bucket(BOOKMARKS, 1)));
}

TEST_F(ImportantSitesUtilTest, DialogExcluding) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("http://www.yahoo.com/");

  // Set a bunch of positive signals.
  service->ResetBaseScoreForURL(url2, 5);
  AddBookmark(url1);
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url1);

  // Start off not disabled.
  EXPECT_FALSE(ImportantSitesUtil::IsDialogDisabled(profile()));

  // Important fetch 1.
  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);
  std::vector<std::string> expected_sorted_domains = {"google.com",
                                                      "yahoo.com"};
  std::vector<GURL> expected_sorted_origins = {url1, url2};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
  ASSERT_EQ(2u, important_sites.size());
  // Ignore all sites 2 times.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), std::vector<std::string>(), std::vector<int32_t>(),
      {"google.com", "yahoo.com"},
      {important_sites[0].reason_bitfield, important_sites[1].reason_bitfield});
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), std::vector<std::string>(), std::vector<int32_t>(),
      {"google.com", "yahoo.com"},
      {important_sites[0].reason_bitfield, important_sites[1].reason_bitfield});

  // Still not disabled...
  EXPECT_FALSE(ImportantSitesUtil::IsDialogDisabled(profile()));

  // Ignore 3rd time.
  ImportantSitesUtil::RecordExcludedAndIgnoredImportantSites(
      profile(), std::vector<std::string>(), std::vector<int32_t>(),
      {"google.com", "yahoo.com"},
      {important_sites[0].reason_bitfield, important_sites[1].reason_bitfield});

  // Items should still be present.
  important_sites = ImportantSitesUtil::GetImportantRegisterableDomains(
      profile(), kNumImportantSites);
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);

  // Dialog should be disabled.
  EXPECT_TRUE(ImportantSitesUtil::IsDialogDisabled(profile()));
}

TEST_F(ImportantSitesUtilTest, ExcludeNonRegisterableDomains) {
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("chrome://newtab/");
  GURL url3("chrome://settings/");
  GURL url4("http://localhost/");

  // Set a bunch of positive signals.
  service->ResetBaseScoreForURL(url1, 8);
  service->ResetBaseScoreForURL(url2, 9);
  AddBookmark(url3);
  AddContentSetting(ContentSettingsType::NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url4);

  std::vector<ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kNumImportantSites);

  ASSERT_EQ(1u, important_sites.size());
  std::vector<std::string> expected_sorted_domains = {"google.com"};
  std::vector<GURL> expected_sorted_origins = {url1};
  ExpectImportantResultsEq(expected_sorted_domains, expected_sorted_origins,
                           important_sites);
}

}  // namespace site_engagement

#endif  // !BUILDFLAG(IS_ANDROID)
