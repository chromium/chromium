// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/site_isolation/features.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

class TrustedVaultEncryptionKeysTabHelperTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TrustedVaultEncryptionKeysTabHelperTest() {
    // Avoid the disabling of site isolation due to memory constraints, required
    // on Android so that ApplyGlobalIsolatedOrigins() takes effect regardless
    // of available memory when running the test (otherwise low-memory bots may
    // run into test failures).
    // TODO(crbug.com/362466866): Instead of disabling the
    // `kSafetyHubAbusiveNotificationRevocation` feature, find a stable
    // fix such that the tests still pass when the feature is enabled.
    feature_list_.InitWithFeaturesAndParameters(
        {{site_isolation::features::kSiteIsolationMemoryThresholds,
          {{site_isolation::features::
                kStrictSiteIsolationMemoryThresholdParamName,
            "0"},
           {site_isolation::features::
                kPartialSiteIsolationMemoryThresholdParamName,
            "0"}}}},
        {safe_browsing::kSafetyHubAbusiveNotificationRevocation});
  }

  ~TrustedVaultEncryptionKeysTabHelperTest() override = default;

  TrustedVaultEncryptionKeysTabHelperTest(
      const TrustedVaultEncryptionKeysTabHelperTest&) = delete;
  TrustedVaultEncryptionKeysTabHelperTest& operator=(
      const TrustedVaultEncryptionKeysTabHelperTest&) = delete;

 protected:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();
    ChromeRenderViewHostTestHarness::SetUp();
    TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    // Undo content::SiteIsolationPolicy::ApplyGlobalIsolatedOrigins().
    content::ChildProcessSecurityPolicy::GetInstance()
        ->ClearIsolatedOriginsForTesting();
  }

  bool HasEncryptionKeysApi(content::RenderFrameHost* rfh) {
    auto* tab_helper =
        TrustedVaultEncryptionKeysTabHelper::FromWebContents(web_contents());
    return tab_helper->HasEncryptionKeysApiForTesting(rfh);
  }

  bool HasEncryptionKeysApiInMainFrame() {
    return HasEncryptionKeysApi(main_rfh());
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                TrustedVaultServiceFactory::GetInstance(),
                TrustedVaultServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                ChromeSigninClientFactory::GetInstance(),
                base::BindRepeating(&signin::BuildTestSigninClient)}};
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       ShouldExposeMojoApiToAllowedOrigin) {
  ASSERT_FALSE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiToUnallowedOrigin) {
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigatedAway) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       ShouldExposeMojoApiEvenIfSubframeNavigatedAway) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  content::NavigationSimulator::CreateRendererInitiated(GURL("http://page.com"),
                                                        subframe)
      ->Commit();
  // For the receiver set to be fully updated, a mainframe navigation is needed.
  // Otherwise the test passes regardless of whether the logic is buggy.
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigationFailed) {
  auto* render_frame_host =
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents(), GaiaUrls::GetInstance()->gaia_url(),
          net::ERR_ABORTED);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

// TODO(crbug.com/40881433): flaky on android bots.
TEST_F(TrustedVaultEncryptionKeysTabHelperTest,
       DISABLED_ShouldNotExposeMojoApiIfNavigatedAwayToErrorPage) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  auto* render_frame_host =
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents(), GURL("http://page.com"), net::ERR_ABORTED);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  // `net::ERR_ABORTED` doesn't update the main frame and the previous main
  // frame is still available. So, EncryptionKeysApi is still valid on the main
  // frame.
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  render_frame_host = content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL("http://page.com"), net::ERR_TIMED_OUT);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  // `net::ERR_TIMED_OUT` commits the error page that is the main frame now.
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

class TrustedVaultEncryptionKeysTabHelperPrerenderingTest
    : public TrustedVaultEncryptionKeysTabHelperTest {
 public:
  TrustedVaultEncryptionKeysTabHelperPrerenderingTest() = default;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

// Tests that EncryptionKeys works based on a main frame. A prerendered page
// also creates EncryptionKeys as it's a main frame. But, if EncryptionKeys
// is accessed thrugh Mojo in prerendering, it causes canceling prerendering.
// See the browser test, 'ShouldNotBindEncryptionKeysApiInPrerendering', from
// 'trusted_vault_keys_tab_helper_browsertest.cc' for the details about
// canceling prerendering.
TEST_F(TrustedVaultEncryptionKeysTabHelperPrerenderingTest,
       CreateEncryptionKeysInPrerendering) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  // Load a page.
  ASSERT_FALSE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  // If prerendering happens in the gaia url, EncryptionKeys is created for
  // the prerendering and the current EncryptionKeys for a primary page also
  // exists.
  const GURL kPrerenderingUrl(GaiaUrls::GetInstance()->gaia_url().spec() +
                              "?prerendering");
  auto* prerender_rfh = content::WebContentsTester::For(web_contents())
                            ->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  DCHECK_NE(prerender_rfh, nullptr);
  EXPECT_TRUE(HasEncryptionKeysApi(prerender_rfh));
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     kPrerenderingUrl);

  // Activate the prerendered page.
  auto* activated_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          kPrerenderingUrl, web_contents()->GetPrimaryMainFrame());
  host_observer.WaitForActivation();
  EXPECT_TRUE(host_observer.was_activated());
  // The EncryptionKeys exists for the activated page.
  EXPECT_TRUE(HasEncryptionKeysApi(activated_rfh));

  // If the prerendering happens to the cross origin, the prerendering would be
  // canceled.
  const GURL kCrossOriginPrerenderingUrl(GURL("http://page.com"));
  content::FrameTreeNodeId frame_tree_node_id =
      content::WebContentsTester::For(web_contents())
          ->AddPrerender(kCrossOriginPrerenderingUrl);
  ASSERT_TRUE(frame_tree_node_id.is_null());
  // EncryptionKeys is still valid in a primary page.
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  // Load a page that doesn't allow EncryptionKeys.
  auto* rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      kCrossOriginPrerenderingUrl, web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(HasEncryptionKeysApi(rfh));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}
}  // namespace
