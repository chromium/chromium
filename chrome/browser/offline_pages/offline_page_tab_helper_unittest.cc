// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_tab_helper.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

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

  ~OfflinePageTabHelperTest() override = default;

  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  void CreateNavigationSimulator(const GURL& url);

  void SimulateOfflinePageLoad(const GURL& mhtml_url,
                               base::Time mhtml_creation_time,
                               MHTMLLoadResult load_result);

  OfflinePageTabHelper* tab_helper() const { return tab_helper_.get(); }
  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

 private:
  tabs::MockTabInterface mock_tab_;
  std::unique_ptr<OfflinePageTabHelper> tab_helper_;
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  base::WeakPtrFactory<OfflinePageTabHelperTest> weak_ptr_factory_{this};
};

OfflinePageTabHelperTest::OfflinePageTabHelperTest() = default;

void OfflinePageTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  tab_helper_ =
      std::make_unique<OfflinePageTabHelper>(mock_tab_, web_contents());
}

void OfflinePageTabHelperTest::TearDown() {
  tab_helper_.reset();
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

  int process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID();
  int main_frame_id = web_contents()->GetPrimaryMainFrame()->GetRoutingID();

  // Navigate away.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             kTestUrl);
  EXPECT_TRUE(back_forward_cache_tester.IsDisabledForFrameWithReason(
      process_id, main_frame_id,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kOfflinePage)));
}

TEST_F(OfflinePageTabHelperTest, UntrustedMhtmlFileLoadOriginNotSpoofed) {
  const GURL kFileUrl("file:///sdcard/Download/GoogleSignIn.mhtml");
  const GURL kSpoofedUrl("https://accounts.google.com/signin");

  tab_helper()->SetCurrentTargetFrameForTest(
      web_contents()->GetPrimaryMainFrame());

  CreateNavigationSimulator(kFileUrl);
  navigation_simulator()->Start();

  navigation_simulator()->SetContentsMimeType("multipart/related");
  tab_helper()->NotifyMhtmlPageLoadAttempted(
      MHTMLLoadResult::kSuccess, kSpoofedUrl, kTestMhtmlCreationTime);
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(OfflinePageTrustedState::UNTRUSTED, tab_helper()->trusted_state());
  EXPECT_FALSE(tab_helper()->IsShowingTrustedOfflinePage());
  EXPECT_EQ(kTestMhtmlCreationTime,
            tab_helper()->offline_page()->creation_time);
  // Offline page URL must reflect the actual navigated file URL, preventing
  // origin spoofing in Page Info.
  EXPECT_EQ(kFileUrl, tab_helper()->offline_page()->url);
  // The renderer-reported URL is only kept as the target for reloading the
  // page from the network.
  EXPECT_EQ(kSpoofedUrl, tab_helper()->untrusted_online_url());

  // The reported URL must not outlive the page it was reported for.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/page.html"));
  EXPECT_TRUE(tab_helper()->untrusted_online_url().is_empty());
}

TEST_F(OfflinePageTabHelperTest, UntrustedMhtmlFileLoadWithoutReportedUrl) {
  const GURL kFileUrl("file:///sdcard/Download/page.mhtml");

  CreateNavigationSimulator(kFileUrl);
  navigation_simulator()->Start();
  navigation_simulator()->SetContentsMimeType("multipart/related");
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kFileUrl, tab_helper()->offline_page()->url);
  EXPECT_TRUE(tab_helper()->untrusted_online_url().is_empty());
}

TEST_F(OfflinePageTabHelperTest, HttpMhtmlLoadMismatchedUrlRejected) {
  const GURL kNavigatedUrl("https://example.com/page.html");
  const GURL kMismatchedUrl("https://evil.com/phish.html");

  tab_helper()->SetCurrentTargetFrameForTest(
      web_contents()->GetPrimaryMainFrame());

  CreateNavigationSimulator(kNavigatedUrl);
  navigation_simulator()->Start();

  navigation_simulator()->SetContentsMimeType("multipart/related");
  tab_helper()->NotifyMhtmlPageLoadAttempted(
      MHTMLLoadResult::kSuccess, kMismatchedUrl, kTestMhtmlCreationTime);
  navigation_simulator()->Commit();

  // Mismatched renderer-reported URL for HTTP/HTTPS navigation must be
  // rejected.
  EXPECT_EQ(nullptr, tab_helper()->offline_page());
}

TEST_F(OfflinePageTabHelperTest, HttpMhtmlLoadMatchingUrlAccepted) {
  const GURL kNavigatedUrl("https://example.com/page.html");

  tab_helper()->SetCurrentTargetFrameForTest(
      web_contents()->GetPrimaryMainFrame());

  CreateNavigationSimulator(kNavigatedUrl);
  navigation_simulator()->Start();

  navigation_simulator()->SetContentsMimeType("multipart/related");
  tab_helper()->NotifyMhtmlPageLoadAttempted(
      MHTMLLoadResult::kSuccess, kNavigatedUrl, kTestMhtmlCreationTime);
  navigation_simulator()->Commit();

  ASSERT_NE(nullptr, tab_helper()->offline_page());
  EXPECT_EQ(kNavigatedUrl, tab_helper()->offline_page()->url);
}

TEST_F(OfflinePageTabHelperTest, RemoveOfflineHeaderOnRedirect) {
  testing::NiceMock<content::MockNavigationHandle> main_frame_handle(
      GURL("https://example.com/redirected"),
      web_contents()->GetPrimaryMainFrame());
  main_frame_handle.set_is_in_primary_main_frame(true);
  EXPECT_CALL(main_frame_handle,
              RemoveRequestHeader(std::string_view(kOfflinePageHeader)))
      .Times(1);
  static_cast<content::WebContentsObserver*>(tab_helper())
      ->DidRedirectNavigation(&main_frame_handle);

  testing::NiceMock<content::MockNavigationHandle> subframe_handle(
      GURL("https://example.com/subframe"),
      web_contents()->GetPrimaryMainFrame());
  subframe_handle.set_is_in_primary_main_frame(false);
  EXPECT_CALL(subframe_handle, RemoveRequestHeader(testing::_)).Times(0);
  static_cast<content::WebContentsObserver*>(tab_helper())
      ->DidRedirectNavigation(&subframe_handle);
}

TEST_F(OfflinePageTabHelperTest,
       SelectPagesForURLDoneBindsOfflineIdAndCrossOriginHeaderRemoval) {
  const GURL kSavedUrl("https://example.com/saved");
  constexpr int64_t kOfflineId = 987654;
  OfflinePageItem saved_item(kSavedUrl, kOfflineId,
                             ClientId(kLastNNamespace, "1234"),
                             base::FilePath(), 100);
  tab_helper()->SelectPagesForURLDoneForTesting({saved_item});
  content::NavigationEntry* pending_entry =
      web_contents()->GetController().GetPendingEntry();
  ASSERT_NE(nullptr, pending_entry);
  EXPECT_TRUE(pending_entry->GetRemoveExtraHeadersOnCrossOriginRedirect());

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(pending_entry->GetExtraHeaders());
  std::optional<std::string> header_value =
      headers.GetHeader(kOfflinePageHeader);
  ASSERT_TRUE(header_value.has_value());
  OfflinePageHeader parsed(*header_value);
  EXPECT_EQ(OfflinePageHeader::Reason::NET_ERROR, parsed.reason);
  EXPECT_EQ(base::NumberToString(kOfflineId), parsed.id);
  web_contents()->GetController().DiscardNonCommittedEntries();
}

TEST_F(OfflinePageTabHelperTest, NetErrorReloadFailureDoesNotLoop) {
  constexpr int kTabId = 42;
  constexpr int64_t kOfflineId = 987654;
  const GURL kSavedUrl("https://example.com/saved");

  Profile* profile = Profile::FromBrowserContext(browser_context());
  OfflinePageModel* model = static_cast<OfflinePageModel*>(
      OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
          profile->GetProfileKey(),
          base::BindRepeating(&BuildTestOfflinePageModel)));
  tab_helper()->SetTabIdGetterForTesting(
      base::BindRepeating([](content::WebContents*, int* tab_id) {
        *tab_id = kTabId;
        return true;
      }));

  OfflinePageItem saved_item(
      kSavedUrl, kOfflineId,
      ClientId(kLastNNamespace, base::NumberToString(kTabId)), base::FilePath(),
      100);
  base::test::TestFuture<AddPageResult, int64_t> add_future;
  model->AddPage(saved_item, add_future.GetCallback());
  ASSERT_EQ(AddPageResult::SUCCESS, add_future.Get<0>());

  // Initial navigation fails with ERR_INTERNET_DISCONNECTED -> triggers a
  // single NET_ERROR reload carrying `id=kOfflineId`.
  auto initial_nav = content::NavigationSimulator::CreateBrowserInitiated(
      kSavedUrl, web_contents());
  initial_nav->Fail(net::ERR_INTERNET_DISCONNECTED);
  initial_nav->CommitErrorPage();
  task_environment()->RunUntilIdle();

  content::NavigationEntry* reload_entry =
      web_contents()->GetController().GetPendingEntry();
  ASSERT_NE(nullptr, reload_entry);
  EXPECT_EQ(kSavedUrl, reload_entry->GetURL());

  // When that NET_ERROR reload itself fails (e.g. evicted last_n archive file)
  // and commits an error page, DidFinishNavigation must NOT schedule a second
  // reload, even though the last_n metadata row remains in the store.
  auto failed_reload = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  failed_reload->Fail(net::ERR_INTERNET_DISCONNECTED);
  failed_reload->CommitErrorPage();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(nullptr, web_contents()->GetController().GetPendingEntry());

  // A subsequent user-initiated navigation that fails offline still gets a
  // fresh single reload attempt (and aborting that reload resets the guard).
  auto retry_nav = content::NavigationSimulator::CreateBrowserInitiated(
      kSavedUrl, web_contents());
  retry_nav->Fail(net::ERR_INTERNET_DISCONNECTED);
  retry_nav->CommitErrorPage();
  task_environment()->RunUntilIdle();
  ASSERT_NE(nullptr, web_contents()->GetController().GetPendingEntry());

  auto aborted_reload = content::NavigationSimulator::CreateFromPending(
      web_contents()->GetController());
  aborted_reload->AbortCommit();
  task_environment()->RunUntilIdle();

  auto after_abort_nav = content::NavigationSimulator::CreateBrowserInitiated(
      kSavedUrl, web_contents());
  after_abort_nav->Fail(net::ERR_INTERNET_DISCONNECTED);
  after_abort_nav->CommitErrorPage();
  task_environment()->RunUntilIdle();
  EXPECT_NE(nullptr, web_contents()->GetController().GetPendingEntry());
  web_contents()->GetController().DiscardNonCommittedEntries();
}

}  // namespace
}  // namespace offline_pages
