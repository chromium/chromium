// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_tab_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

const base::Time kTestMhtmlCreationTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1522339419011L);

const char kTestHeader[] = "reason=download";

}  // namespace

namespace offline_pages {
namespace {

using blink::mojom::MHTMLLoadResult;

class OfflinePageTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  OfflinePageTabHelperTest();

  OfflinePageTabHelperTest(const OfflinePageTabHelperTest&) = delete;
  OfflinePageTabHelperTest& operator=(const OfflinePageTabHelperTest&) = delete;

  ~OfflinePageTabHelperTest() override {}

  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  void CreateNavigationSimulator(const GURL& url);

  void SimulateOfflinePageLoad(const GURL& mhtml_url,
                               base::Time mhtml_creation_time,
                               MHTMLLoadResult load_result);

  OfflinePageTabHelper* tab_helper() const { return tab_helper_; }
  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

 private:
  raw_ptr<OfflinePageTabHelper> tab_helper_;  // Owned by WebContents.
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  base::WeakPtrFactory<OfflinePageTabHelperTest> weak_ptr_factory_{this};
};

OfflinePageTabHelperTest::OfflinePageTabHelperTest() : tab_helper_(nullptr) {}

void OfflinePageTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  OfflinePageTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = OfflinePageTabHelper::FromWebContents(web_contents());
}

void OfflinePageTabHelperTest::TearDown() {
  content::RenderViewHostTestHarness::TearDown();
}

std::unique_ptr<content::BrowserContext>
OfflinePageTabHelperTest::CreateBrowserContext() {
  return TestingProfile::Builder().Build();
}

void OfflinePageTabHelperTest::CreateNavigationSimulator(const GURL& url) {
  navigation_simulator_ =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents());
  navigation_simulator_->SetTransition(ui::PAGE_TRANSITION_LINK);
}

void OfflinePageTabHelperTest::SimulateOfflinePageLoad(
    const GURL& mhtml_url,
    base::Time mhtml_creation_time,
    MHTMLLoadResult load_result) {
  tab_helper()->SetCurrentTargetFrameForTest(
      web_contents()->GetPrimaryMainFrame());

  // Simulate navigation
  CreateNavigationSimulator(GURL("file://foo"));
  navigation_simulator()->Start();

  OfflinePageItem offlinePage(mhtml_url, 0, ClientId("async_loading", "1234"),
                              base::FilePath(), 0, mhtml_creation_time);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR, false);

  navigation_simulator()->SetContentsMimeType("multipart/related");

  tab_helper()->NotifyMhtmlPageLoadAttempted(load_result, mhtml_url,
                                             mhtml_creation_time);
  navigation_simulator()->Commit();
}

// Checks the test setup.
TEST_F(OfflinePageTabHelperTest, InitialSetup) {
  CreateNavigationSimulator(GURL("http://mystery.site/foo.html"));
  EXPECT_NE(nullptr, tab_helper());
}

TEST_F(OfflinePageTabHelperTest, MetricsStartNavigation) {
  CreateNavigationSimulator(GURL("http://mystery.site/foo.html"));
  // This causes WCO::DidStartNavigation()
  navigation_simulator()->Start();
}

TEST_F(OfflinePageTabHelperTest, MetricsOnlineNavigation) {
  CreateNavigationSimulator(GURL("http://mystery.site/foo.html"));
  navigation_simulator()->Start();
  navigation_simulator()->Commit();
}

TEST_F(OfflinePageTabHelperTest, MetricsOfflineNavigation) {
  const GURL kTestUrl("http://mystery.site/foo.html");
  CreateNavigationSimulator(kTestUrl);
  navigation_simulator()->Start();

  // Simulate offline interceptor loading an offline page instead.
  OfflinePageItem offlinePage(kTestUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader;
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");

  navigation_simulator()->Commit();
}

TEST_F(OfflinePageTabHelperTest, TrustedInternalOfflinePage) {
  const GURL kTestUrl("http://mystery.site/foo.html");
  CreateNavigationSimulator(kTestUrl);
  navigation_simulator()->Start();

  OfflinePageItem offlinePage(kTestUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kTestUrl, tab_helper()->offline_page()->url);
  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);
}

TEST_F(OfflinePageTabHelperTest, TrustedPublicOfflinePage) {
  const GURL kTestUrl("http://mystery.site/foo.html");
  CreateNavigationSimulator(kTestUrl);
  navigation_simulator()->Start();

  OfflinePageItem offlinePage(kTestUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR, false);
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kTestUrl, tab_helper()->offline_page()->url);
  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_UNMODIFIED_AND_IN_PUBLIC_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);
}

TEST_F(OfflinePageTabHelperTest, UntrustedOfflinePageForFileUrl) {
  CreateNavigationSimulator(GURL("file://foo"));
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::NONE,
            tab_helper()->offline_header().reason);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(OfflinePageTabHelperTest,
       UntrustedOfflinePageForContentUrlWithMultipartRelatedType) {
  CreateNavigationSimulator(GURL("content://foo"));
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
  CreateNavigationSimulator(GURL("content://foo"));
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

TEST_F(OfflinePageTabHelperTest, TestNotifyMhtmlPageLoadAttempted_Success) {
  GURL mhtml_url("https://www.example.com");

  // Simulate navigation
  SimulateOfflinePageLoad(mhtml_url, kTestMhtmlCreationTime,
                          MHTMLLoadResult::kSuccess);

  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);

  const OfflinePageItem* offline_page = tab_helper()->offline_page();
  ASSERT_NE(nullptr, offline_page);
  EXPECT_EQ(mhtml_url, offline_page->url);
  EXPECT_EQ(kTestMhtmlCreationTime, offline_page->creation_time);
}

TEST_F(OfflinePageTabHelperTest,
       TestNotifyMhtmlPageLoadAttempted_BadUrlScheme) {
  GURL mhtml_url("sftp://www.example.com");

  base::HistogramTester histogram_tester;
  SimulateOfflinePageLoad(mhtml_url, kTestMhtmlCreationTime,
                          MHTMLLoadResult::kUrlSchemeNotAllowed);

  EXPECT_EQ(OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
            tab_helper()->trusted_state());
  EXPECT_TRUE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD,
            tab_helper()->offline_header().reason);

  const OfflinePageItem* offline_page = tab_helper()->offline_page();
  EXPECT_EQ(mhtml_url, offline_page->url);
  EXPECT_EQ(kTestMhtmlCreationTime, offline_page->creation_time);
}

TEST_F(OfflinePageTabHelperTest, TestNotifyMhtmlPageLoadAttempted_Untrusted) {
  GURL mhtml_url("https://www.example.com");

  tab_helper()->SetCurrentTargetFrameForTest(
      web_contents()->GetPrimaryMainFrame());

  // Simulate navigation
  CreateNavigationSimulator(GURL("file://foo"));
  navigation_simulator()->Start();

  // We force use of the untrusted page histogram by using an empty namespace.
  OfflinePageItem offlinePage(mhtml_url, 0, ClientId("", "1234"),
                              base::FilePath(), 0, kTestMhtmlCreationTime);
  OfflinePageHeader offlineHeader(kTestHeader);
  tab_helper()->SetOfflinePage(offlinePage, offlineHeader,
                               OfflinePageTrustedState::UNTRUSTED, false);

  navigation_simulator()->SetContentsMimeType("multipart/related");

  tab_helper()->NotifyMhtmlPageLoadAttempted(MHTMLLoadResult::kSuccess,
                                             mhtml_url, kTestMhtmlCreationTime);
  navigation_simulator()->Commit();
}

TEST_F(OfflinePageTabHelperTest, AbortedNavigationDoesNotResetOfflineInfo) {
  GURL mhtml_url("https://www.example.com");
  SimulateOfflinePageLoad(mhtml_url, kTestMhtmlCreationTime,
                          MHTMLLoadResult::kUrlSchemeNotAllowed);
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://mystery.site/foo.html"), web_contents());
  navigation->Start();
  navigation->AbortCommit();
  EXPECT_TRUE(tab_helper()->offline_page());
}

TEST_F(OfflinePageTabHelperTest, OfflinePageIsNotStoredInBackForwardCache) {
  content::BackForwardCacheDisabledTester back_forward_cache_tester;

  const GURL kTestUrl("http://mystery.site/foo.html");
  CreateNavigationSimulator(kTestUrl);
  navigation_simulator()->Start();

  SimulateOfflinePageLoad(kTestUrl, kTestMhtmlCreationTime,
                          MHTMLLoadResult::kSuccess);

  int process_id = web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  int main_frame_id = web_contents()->GetPrimaryMainFrame()->GetRoutingID();

  // Navigate away.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kTestUrl);
  EXPECT_TRUE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      process_id, main_frame_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kOfflinePage)));
}

class OfflinePageTabHelperFencedFrameTest : public OfflinePageTabHelperTest {
 public:
  OfflinePageTabHelperFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~OfflinePageTabHelperFencedFrameTest() override = default;

  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* fenced_frame =
        content::RenderFrameHostTester::For(parent)->AppendFencedFrame();
    return fenced_frame;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OfflinePageTabHelperFencedFrameTest, DoNotRecordMetricsInFencedFrame) {
  const GURL kTestUrl("http://mystery.site/foo.html");
  CreateNavigationSimulator(kTestUrl);
  navigation_simulator()->Start();

  // Simulate offline interceptor loading an offline page instead.
  OfflinePageItem offlinePage(kTestUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader;
  tab_helper()->SetOfflinePage(
      offlinePage, offlineHeader,
      OfflinePageTrustedState::TRUSTED_AS_IN_INTERNAL_DIR,
      true /* is_offline_preview */);
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  // Ensure that the offline page exists via an offline preview item.
  const OfflinePageItem* offline_page_item =
      tab_helper()->GetOfflinePreviewItem();
  EXPECT_NE(offline_page_item, nullptr);

  // Create a fenced frame.
  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* fenced_frame_rfh = CreateFencedFrame(main_rfh());
  GURL kFencedFrameUrl("https://fencedframe.com");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(kFencedFrameUrl,
                                                            fenced_frame_rfh);
  navigation_simulator->Commit();
  EXPECT_TRUE(fenced_frame_rfh->IsFencedFrameRoot());

  // The offline preview item should not be cleared by the fenced frame's
  // navigation and should be same as |offline_page_item|.
  EXPECT_EQ(tab_helper()->GetOfflinePreviewItem(), offline_page_item);
}

}  // namespace
}  // namespace offline_pages
