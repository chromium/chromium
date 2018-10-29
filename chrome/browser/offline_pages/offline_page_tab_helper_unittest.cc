// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kTestPageUrl("http://mystery.site/foo.html");
const GURL kTestFileUrl("file://foo");

#if defined(OS_ANDROID)
const GURL kTestContentUrl("content://foo");
#endif

const char kTestHeader[] = "reason=download";
}  // namespace

namespace offline_pages {

class TestMetricsCollector : public OfflineMetricsCollector {
 public:
  TestMetricsCollector() = default;
  ~TestMetricsCollector() override = default;

  // OfflineMetricsCollector implementation
  void OnAppStartupOrResume() override { app_startup_count_++; }
  void OnSuccessfulNavigationOnline() override {
    successful_online_navigations_count_++;
  }
  void OnSuccessfulNavigationOffline() override {
    successful_offline_navigations_count_++;
  }
  void OnPrefetchEnabled() override {}
  void OnSuccessfulPagePrefetch() override {}
  void OnPrefetchedPageOpened() override {}
  void ReportAccumulatedStats() override { report_stats_count_++; }

  int app_startup_count_ = 0;
  int successful_offline_navigations_count_ = 0;
  int successful_online_navigations_count_ = 0;
  int report_stats_count_ = 0;
};

// This is used by KeyedServiceFactory::SetTestingFactoryAndUse.
std::unique_ptr<KeyedService> BuildTestPrefetchService(
    content::BrowserContext*) {
  auto taco = std::make_unique<PrefetchServiceTestTaco>();
  taco->SetOfflineMetricsCollector(std::make_unique<TestMetricsCollector>());
  return taco->CreateAndReturnPrefetchService();
}

class OfflinePageTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  OfflinePageTabHelperTest();
  ~OfflinePageTabHelperTest() override {}

  void SetUp() override;
  void TearDown() override;
  content::BrowserContext* CreateBrowserContext() override;

  void CreateNavigationSimulator(const GURL& url);

  OfflinePageTabHelper* tab_helper() const { return tab_helper_; }
  PrefetchService* prefetch_service() const { return prefetch_service_; }
  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }
  TestMetricsCollector* metrics() const {
    return static_cast<TestMetricsCollector*>(
        prefetch_service_->GetOfflineMetricsCollector());
  }

 private:
  OfflinePageTabHelper* tab_helper_;   // Owned by WebContents.
  PrefetchService* prefetch_service_;  // Keyed Service.
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  base::WeakPtrFactory<OfflinePageTabHelperTest> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelperTest);
};

OfflinePageTabHelperTest::OfflinePageTabHelperTest()
    : tab_helper_(nullptr), weak_ptr_factory_(this) {}

void OfflinePageTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  PrefetchServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), base::BindRepeating(&BuildTestPrefetchService));
  prefetch_service_ =
      PrefetchServiceFactory::GetForBrowserContext(browser_context());

  OfflinePageTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = OfflinePageTabHelper::FromWebContents(web_contents());
}

void OfflinePageTabHelperTest::TearDown() {
  content::RenderViewHostTestHarness::TearDown();
}

content::BrowserContext* OfflinePageTabHelperTest::CreateBrowserContext() {
  TestingProfile::Builder builder;
  return builder.Build().release();
}

void OfflinePageTabHelperTest::CreateNavigationSimulator(const GURL& url) {
  navigation_simulator_ =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation_simulator_->SetTransition(ui::PAGE_TRANSITION_LINK);
}

// Checks the test setup.
TEST_F(OfflinePageTabHelperTest, InitialSetup) {
  CreateNavigationSimulator(kTestPageUrl);
  EXPECT_NE(nullptr, tab_helper());
  EXPECT_NE(nullptr, prefetch_service());
  EXPECT_NE(nullptr, prefetch_service()->GetOfflineMetricsCollector());
  EXPECT_EQ(metrics(), prefetch_service()->GetOfflineMetricsCollector());
  EXPECT_EQ(0, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsStartNavigation) {
  CreateNavigationSimulator(kTestPageUrl);
  // This causes WCO::DidStartNavigation()
  navigation_simulator()->Start();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsOnlineNavigation) {
  CreateNavigationSimulator(kTestPageUrl);
  navigation_simulator()->Start();
  navigation_simulator()->Commit();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(1, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  // Since this is online navigation, request to send data should be made.
  EXPECT_EQ(1, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsOfflineNavigation) {
  CreateNavigationSimulator(kTestPageUrl);
  navigation_simulator()->Start();

  // Simulate offline interceptor loading an offline page instead.
  OfflinePageItem offlinePage(kTestPageUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader;
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");

  navigation_simulator()->Commit();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(1, metrics()->successful_offline_navigations_count_);
  // During offline navigation, request to send data should not be made.
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, TrustedInternalOfflinePage) {
  CreateNavigationSimulator(kTestPageUrl);
  navigation_simulator()->Start();

  OfflinePageItem offlinePage(kTestPageUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kTestPageUrl, tab_helper()->offline_page()->url);
  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);
}

TEST_F(OfflinePageTabHelperTest, TrustedPublicOfflinePage) {
  CreateNavigationSimulator(kTestPageUrl);
  navigation_simulator()->Start();

  OfflinePageItem offlinePage(kTestPageUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kTestPageUrl, tab_helper()->offline_page()->url);
  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);
}

TEST_F(OfflinePageTabHelperTest, UntrustedOfflinePageForFileUrl) {
  CreateNavigationSimulator(kTestFileUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);
}

#if defined(OS_ANDROID)
TEST_F(OfflinePageTabHelperTest,
       UntrustedOfflinePageForContentUrlWithMultipartRelatedType) {
  CreateNavigationSimulator(kTestContentUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);
}

TEST_F(OfflinePageTabHelperTest,
       UntrustedOfflinePageForContentUrlWithMessageRfc822Type) {
  CreateNavigationSimulator(kTestContentUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("message/rfc822");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);
}
#endif

TEST_F(OfflinePageTabHelperTest, TestNotifyIsMhtmlPage) {
  GURL mhtml_url("https://www.example.com");
  base::Time mhtml_creation_time = base::Time::FromJsTime(1522339419011L);
  tab_helper()->SetCurrentTargetFrameForTest(web_contents()->GetMainFrame());

  CreateNavigationSimulator(kTestFileUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  tab_helper()->NotifyIsMhtmlPage(mhtml_url, mhtml_creation_time);
  navigation_simulator()->Commit();

  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);

  const OfflinePageItem* offline_page = tab_helper()->offline_page();
  ASSERT_NE(nullptr, offline_page);
  EXPECT_EQ(mhtml_url, offline_page->url);
  EXPECT_EQ(mhtml_creation_time, offline_page->creation_time);
}

TEST_F(OfflinePageTabHelperTest, TestNotifyIsMhtmlPage_BadUrl) {
  GURL mhtml_url("sftp://www.example.com");
  base::Time mhtml_creation_time = base::Time::FromJsTime(1522339419011L);
  tab_helper()->SetCurrentTargetFrameForTest(web_contents()->GetMainFrame());

  CreateNavigationSimulator(kTestFileUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  tab_helper()->NotifyIsMhtmlPage(mhtml_url, mhtml_creation_time);
  navigation_simulator()->Commit();

  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);

  const OfflinePageItem* offline_page = tab_helper()->offline_page();
  EXPECT_EQ(kTestFileUrl, offline_page->url);
  EXPECT_EQ(base::Time(), offline_page->creation_time);
}

}  // namespace offline_pages
