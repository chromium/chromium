// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Tests that SubresourceFilterHistoryObserver is operating as expected in the
// context of //chrome-level setup of SubresourceFilterProfileContext. More of
// an integration test than a unittest in spirit, but requires unittest
// constructs such as TestingProfile and HistoryService-related unittest
// utilities to be able to perform the needed operations on history.
class SubresourceFilterHistoryObserverTest : public testing::Test {
 public:
  SubresourceFilterHistoryObserverTest() = default;

  SubresourceFilterHistoryObserverTest(
      const SubresourceFilterHistoryObserverTest&) = delete;
  SubresourceFilterHistoryObserverTest& operator=(
      const SubresourceFilterHistoryObserverTest&) = delete;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    testing_profile_ = profile_builder.Build();

    settings_manager_ = SubresourceFilterProfileContextFactory::GetForProfile(
                            testing_profile_.get())
                            ->settings_manager();
    settings_manager_->set_should_use_smart_ui_for_testing(true);
  }

  subresource_filter::SubresourceFilterContentSettingsManager*
  settings_manager() {
    return settings_manager_;
  }

  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> testing_profile_;

  // Owned by the testing_profile_.
  raw_ptr<subresource_filter::SubresourceFilterContentSettingsManager>
      settings_manager_ = nullptr;
};

// Tests that SubresourceFilterHistoryObserver observes deletions of URLs from
// history and instructs SubresourceFilterContentSettingsManager to delete the
// appropriate site metadata from content settings (if any).
TEST_F(SubresourceFilterHistoryObserverTest,
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

// Tests that SubresourceFilterHistoryObserver observes deletions of all URLs
// from history and instructs SubresourceFilterContentSettingsManager to delete
// all site metadata from content settings.
TEST_F(SubresourceFilterHistoryObserverTest,
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
  history_service->ExpireHistoryBetween(
      std::set<GURL>(), history::kNoAppIdFilter, base::Time(), base::Time(),
      /*user_initiated*/ true, run_loop.QuitClosure(), &task_tracker);
  run_loop.Run();

  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url1));
  EXPECT_TRUE(settings_manager()->ShouldShowUIForSite(url2));
}
