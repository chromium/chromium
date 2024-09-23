// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/reauth_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace signin {

class ReauthTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  ReauthTabHelperTest()
      : reauth_url_("https://my-identity_provider.com/reauth") {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ReauthTabHelper::CreateForWebContents(web_contents(), reauth_url(),
                                          mock_callback_.Get());
    tab_helper_ = ReauthTabHelper::FromWebContents(web_contents());
  }

  ReauthTabHelper* tab_helper() { return tab_helper_; }

  base::MockOnceCallback<void(signin::ReauthResult)>* mock_callback() {
    return &mock_callback_;
  }

  const GURL& reauth_url() { return reauth_url_; }

 private:
  raw_ptr<ReauthTabHelper, DanglingUntriaged> tab_helper_ = nullptr;
  base::MockOnceCallback<void(signin::ReauthResult)> mock_callback_;
  const GURL reauth_url_;
};

// Tests a direct call to CompleteReauth().
TEST_F(ReauthTabHelperTest, CompleteReauth) {
  signin::ReauthResult result = signin::ReauthResult::kSuccess;
  EXPECT_CALL(*mock_callback(), Run(result));
  tab_helper()->CompleteReauth(result);
}

// Tests a successful navigation to the reauth URL.
TEST_F(ReauthTabHelperTest, NavigateToReauthURL) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url(), web_contents());
  simulator->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator->Commit();
}

// Tests the reauth flow when the reauth URL has query parameters.
TEST_F(ReauthTabHelperTest, NavigateToReauthURLWithQuery) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url().Resolve("?rapt=35be36ae"), web_contents());
  simulator->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator->Commit();
}

// Tests the reauth flow with multiple navigations within the same origin.
TEST_F(ReauthTabHelperTest, MultipleNavigationReauth) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url(), web_contents());
  simulator->Start();
  simulator->Redirect(
      reauth_url().DeprecatedGetOriginAsURL().Resolve("/login"));
  simulator->Commit();

  auto simulator2 = content::NavigationSimulator::CreateRendererInitiated(
      reauth_url(), main_rfh());
  simulator2->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator2->Commit();
}

// Tests the reauth flow with multiple navigations across two different origins.
// TODO(crbug.com/40116065): update this test once navigations outside of
// reauth_url() are blocked.
TEST_F(ReauthTabHelperTest, MultipleNavigationReauthThroughExternalOrigin) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url(), web_contents());
  simulator->Start();
  simulator->Redirect(GURL("https://other-identity-provider.com/login"));
  simulator->Commit();

  auto simulator2 = content::NavigationSimulator::CreateRendererInitiated(
      reauth_url(), main_rfh());
  simulator2->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator2->Commit();
}

// Tests a failed navigation to the reauth URL, followed by a successful
// navigation.
TEST_F(ReauthTabHelperTest, NavigationToReauthURLFailed) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url(), web_contents());
  simulator->Start();
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();
  EXPECT_TRUE(tab_helper()->has_last_committed_error_page());
  // Check that the navigation still counts as within the same origin.
  EXPECT_TRUE(tab_helper()->is_within_reauth_origin());

  auto simulator2 = content::NavigationSimulator::CreateRendererInitiated(
      reauth_url(), main_rfh());
  simulator2->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator2->Commit();
  EXPECT_FALSE(tab_helper()->has_last_committed_error_page());
}

// Tests a failed navigation redirecting to an external origin, followed by a
// successful navigation.
TEST_F(ReauthTabHelperTest, NavigationToExternalOriginFailed) {
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      reauth_url(), web_contents());
  simulator->Start();
  simulator->Redirect(GURL("https://other-identity-provider.com/login"));
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();
  EXPECT_TRUE(tab_helper()->has_last_committed_error_page());
  // Check that the navigation doesn't count as within the same origin.
  EXPECT_FALSE(tab_helper()->is_within_reauth_origin());

  auto simulator2 = content::NavigationSimulator::CreateRendererInitiated(
      reauth_url(), main_rfh());
  simulator2->Start();
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kSuccess));
  simulator2->Commit();
  EXPECT_FALSE(tab_helper()->has_last_committed_error_page());
  EXPECT_FALSE(tab_helper()->is_within_reauth_origin());
}

// Tests the WebContents deletion.
TEST_F(ReauthTabHelperTest, WebContentsDestroyed) {
  EXPECT_CALL(*mock_callback(), Run(signin::ReauthResult::kDismissedByUser));
  DeleteContents();
}

class ReauthTabHelperPrerenderTest : public ReauthTabHelperTest {
 public:
  ReauthTabHelperPrerenderTest() = default;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(ReauthTabHelperPrerenderTest,
       PrerenderDoesNotAffectLastCommittedErrorPage) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             reauth_url());
  EXPECT_FALSE(tab_helper()->has_last_committed_error_page());

  // Fail prerendering navigation.
  const GURL prerender_url = reauth_url().Resolve("?prerendering");
  auto simulator = content::WebContentsTester::For(web_contents())
                       ->AddPrerenderAndStartNavigation(prerender_url);
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();

  // has_last_committed_error_page_ is not updated by preredering.
  EXPECT_FALSE(tab_helper()->has_last_committed_error_page());
}

}  // namespace signin
