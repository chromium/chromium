// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicGeminiEnterpriseManagerBrowserTest : public PlatformBrowserTest {
 public:
  GlicGeminiEnterpriseManagerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic,
          {{"enabled", "true"},
           {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
        {});
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    manager_ = std::make_unique<GlicGeminiEnterpriseManager>(GetProfile());
    manager_->Bind(handler_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDownOnMainThread() override {
    manager_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<GlicGeminiEnterpriseManager> manager_;
  mojo::Remote<mojom::GeminiEnterpriseHandler> handler_remote_;
};

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       OpenAndCloseSignInTabLifecycle) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  // Open sign-in tab.
  GURL signin_url("https://accounts.google.com/signin");
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = signin_url;
  handler_remote_->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kSuccess);

  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveIndex(), 1);
  EXPECT_EQ(tab_list->GetActiveTab()->GetContents()->GetVisibleURL(),
            signin_url);

  // Close sign-in tab.
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);

  // Verify sign-in tab is closed and focus is restored to the original tab.
  EXPECT_EQ(tab_list->GetTabCount(), 1);
  EXPECT_EQ(tab_list->GetActiveIndex(), 0);
  EXPECT_EQ(tab_list->GetActiveTab(), initial_tab);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       OpenSignInTabReusesExistingSignInTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  GURL signin_url("https://accounts.google.com/signin");
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = signin_url;
  handler_remote_->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kSuccess);

  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveIndex(), 1);

  // Switch back to tab 0:
  tab_list->ActivateTab(initial_tab->GetHandle());
  EXPECT_EQ(tab_list->GetActiveIndex(), 0);

  // Calling OpenSignInTab again should reactivate tab 1 without opening another
  // tab:
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future2;
  auto options2 = mojom::OpenSignInTabOptions::New();
  options2->signin_url = signin_url;
  handler_remote_->OpenSignInTab(std::move(options2),
                                 open_future2.GetCallback());
  EXPECT_EQ(open_future2.Take(), mojom::OpenSignInTabResult::kSuccess);

  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveIndex(), 1);

  // Close the sign-in tab.
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);
  EXPECT_EQ(tab_list->GetTabCount(), 1);
}

IN_PROC_BROWSER_TEST_F(
    GlicGeminiEnterpriseManagerBrowserTest,
    CloseSignInTabReturnsAlreadyClosedWhenTabWasClosedExternally) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  // Open allowed sign-in tab.
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = GURL("https://accounts.google.com/signin");
  handler_remote_->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kSuccess);
  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveIndex(), 1);

  // Close the tab externally.
  tabs::TabInterface* signin_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(signin_tab);
  signin_tab->Close();
  EXPECT_EQ(tab_list->GetTabCount(), 1);

  // Calling CloseSignInTab should now return kAlreadyClosed.
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kAlreadyClosed);
}

class GlicGeminiEnterpriseManagerCustomGuestUrlBrowserTest
    : public GlicGeminiEnterpriseManagerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicGeminiEnterpriseManagerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        geic::kGeicGuestURLSwitch,
        "https://localhost.corp.google.com:10443/side-panel");
  }
};

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerCustomGuestUrlBrowserTest,
                       AllowsConfiguredGuestOrigin) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  GURL guest_signin_url("https://localhost.corp.google.com:10443/auth/signin");
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = guest_signin_url;
  handler_remote_->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(), mojom::OpenSignInTabResult::kSuccess);

  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveTab()->GetContents()->GetVisibleURL(),
            guest_signin_url);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);
  EXPECT_EQ(tab_list->GetTabCount(), 1);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       DisallowedUrlDoesNotCaptureActiveTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  // Attempt to open a disallowed sign-in URL with embedded credentials:
  GURL disallowed_url("https://user:pass@accounts.google.com/signin");
  base::test::TestFuture<mojom::OpenSignInTabResult> open_future;
  auto options = mojom::OpenSignInTabOptions::New();
  options->signin_url = disallowed_url;
  handler_remote_->OpenSignInTab(std::move(options), open_future.GetCallback());
  EXPECT_EQ(open_future.Take(),
            mojom::OpenSignInTabResult::kErrorDisallowedUrl);

  // Tab count should remain 1 and initial tab should still be active:
  EXPECT_EQ(tab_list->GetTabCount(), 1);
  EXPECT_EQ(tab_list->GetActiveTab(), initial_tab);

  // CloseSignInTab should not close the user's initial tab:
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  handler_remote_->CloseSignInTab(nullptr, close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kNoSignInTab);
  EXPECT_EQ(tab_list->GetTabCount(), 1);
  EXPECT_EQ(tab_list->GetActiveTab(), initial_tab);
}

constexpr char kConnectorOauthUrl[] =
    "https://vertexaisearch.cloud.google.com/oauth-redirect"
    "?continue_uri=https%3A%2F%2Flogin.example.com%2Fauthorize%3Fstate%3D1";
constexpr char kConnectorOauthUrl2[] =
    "https://vertexaisearch.cloud.google.com/oauth-redirect"
    "?continue_uri=https%3A%2F%2Flogin.example.com%2Fauthorize%3Fstate%3D2";

mojom::OpenAuthTabOptionsPtr MakeOpenOptions(mojom::AuthTabPurpose purpose,
                                             const GURL& url) {
  auto options = mojom::OpenAuthTabOptions::New();
  options->purpose = purpose;
  options->url = url;
  return options;
}

mojom::CloseAuthTabOptionsPtr MakeCloseOptions(mojom::AuthTabPurpose purpose) {
  auto options = mojom::CloseAuthTabOptions::New();
  options->purpose = purpose;
  return options;
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       OpenAndCloseConnectorOauthTabLifecycle) {
  base::HistogramTester histogram_tester;
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);

  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveIndex(), 1);
  EXPECT_EQ(tab_list->GetActiveTab()->GetContents()->GetVisibleURL(),
            GURL(kConnectorOauthUrl));

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kClosedActive);

  EXPECT_EQ(tab_list->GetTabCount(), 1);
  EXPECT_EQ(tab_list->GetActiveTab(), initial_tab);

  histogram_tester.ExpectUniqueSample("Geic.AuthTab.OpenResult.ConnectorOAuth",
                                      mojom::OpenAuthTabResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.CloseResult.ConnectorOAuth",
                                      mojom::CloseAuthTabResult::kClosedActive,
                                      1);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       OpenConnectorOauthTabNavigatesExistingTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  tabs::TabInterface* auth_tab = tab_list->GetActiveTab();
  content::WaitForLoadStop(auth_tab->GetContents());

  // User switches back to the original tab, then starts another flow.
  tab_list->ActivateTab(initial_tab->GetHandle());
  ASSERT_EQ(tab_list->GetActiveIndex(), 0);

  content::TestNavigationObserver navigation_observer(auth_tab->GetContents());
  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future2;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl2)),
      open_future2.GetCallback());
  EXPECT_EQ(open_future2.Take()->result, mojom::OpenAuthTabResult::kSuccess);

  // The same tab is reused, reactivated and navigated to the new URL.
  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveTab(), auth_tab);
  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(),
            GURL(kConnectorOauthUrl2));

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kClosedActive);
  EXPECT_EQ(tab_list->GetTabCount(), 1);
  EXPECT_EQ(tab_list->GetActiveTab(), initial_tab);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       CloseConnectorOauthTabReturnsAlreadyClosed) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);

  // The user closes the tab themselves.
  tab_list->GetActiveTab()->Close();
  EXPECT_EQ(tab_list->GetTabCount(), 1);

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kAlreadyClosed);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       AuthTabsAreScopedPerPurpose) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  // Open a sign-in tab through the purpose-keyed API.
  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> signin_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kSignIn,
                      GURL("https://accounts.google.com/signin")),
      signin_future.GetCallback());
  EXPECT_EQ(signin_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);

  // Opening a connector OAuth tab creates a separate tab.
  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> oauth_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      oauth_future.GetCallback());
  EXPECT_EQ(oauth_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 3);

  // Closing the connector OAuth tab must not touch the sign-in tab.
  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_oauth_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_oauth_future.GetCallback());
  EXPECT_EQ(close_oauth_future.Take()->result,
            mojom::CloseAuthTabResult::kClosedActive);
  EXPECT_EQ(tab_list->GetTabCount(), 2);

  // The deprecated CloseSignInTab shares the kSignIn tab.
  base::test::TestFuture<mojom::CloseSignInTabResult> close_signin_future;
  handler_remote_->CloseSignInTab(nullptr, close_signin_future.GetCallback());
  EXPECT_EQ(close_signin_future.Take(), mojom::CloseSignInTabResult::kSuccess);
  EXPECT_EQ(tab_list->GetTabCount(), 1);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       OpenConnectorOauthTabDoesNotReuseNavigatedAwayTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  tabs::TabInterface* abandoned_tab = tab_list->GetActiveTab();
  content::WaitForLoadStop(abandoned_tab->GetContents());

  // The user abandons the flow and browses somewhere else in that tab.
  const GURL unrelated_url("https://example.com/");
  content::NavigateToURLBlockUntilNavigationsComplete(
      abandoned_tab->GetContents(), unrelated_url, 1);

  // Starting another flow opens a fresh tab instead of hijacking that one.
  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future2;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl2)),
      open_future2.GetCallback());
  EXPECT_EQ(open_future2.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 3);
  EXPECT_NE(tab_list->GetActiveTab(), abandoned_tab);
  EXPECT_EQ(abandoned_tab->GetContents()->GetLastCommittedURL(), unrelated_url);

  // Closing affects only the new tab.
  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kClosedActive);
  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_NE(tab_list->GetIndexOfTab(abandoned_tab->GetHandle()), -1);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       CloseConnectorOauthTabLeavesNavigatedAwayTabOpen) {
  base::HistogramTester histogram_tester;
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);
  tabs::TabInterface* auth_tab = tab_list->GetActiveTab();
  content::WaitForLoadStop(auth_tab->GetContents());

  content::NavigateToURLBlockUntilNavigationsComplete(
      auth_tab->GetContents(), GURL("https://example.com/"), 1);

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kNavigatedAway);

  // The tab stays open and focused, and is no longer tracked.
  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveTab(), auth_tab);
  histogram_tester.ExpectUniqueSample("Geic.AuthTab.CloseResult.ConnectorOAuth",
                                      mojom::CloseAuthTabResult::kNavigatedAway,
                                      1);

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future2;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future2.GetCallback());
  EXPECT_EQ(close_future2.Take()->result,
            mojom::CloseAuthTabResult::kNoAuthTab);
  EXPECT_EQ(tab_list->GetTabCount(), 2);
}

IN_PROC_BROWSER_TEST_F(GlicGeminiEnterpriseManagerBrowserTest,
                       CloseInactiveAuthTabDoesNotMoveFocus) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);
  tabs::TabInterface* initial_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  base::test::TestFuture<mojom::OpenAuthTabResponsePtr> open_future;
  handler_remote_->OpenAuthTab(
      MakeOpenOptions(mojom::AuthTabPurpose::kConnectorOauth,
                      GURL(kConnectorOauthUrl)),
      open_future.GetCallback());
  EXPECT_EQ(open_future.Take()->result, mojom::OpenAuthTabResult::kSuccess);
  ASSERT_EQ(tab_list->GetTabCount(), 2);

  // The user moves on to a different tab before the flow finishes.
  tabs::TabInterface* other_tab =
      tab_list->OpenTab(GURL("about:blank"), -1, /*foreground=*/true);
  ASSERT_TRUE(other_tab);
  ASSERT_EQ(tab_list->GetActiveTab(), other_tab);

  base::test::TestFuture<mojom::CloseAuthTabResponsePtr> close_future;
  handler_remote_->CloseAuthTab(
      MakeCloseOptions(mojom::AuthTabPurpose::kConnectorOauth),
      close_future.GetCallback());
  EXPECT_EQ(close_future.Take()->result,
            mojom::CloseAuthTabResult::kClosedInactive);

  // The auth tab is closed, but focus stays where the user put it rather than
  // jumping back to the tab that was active when the flow started.
  EXPECT_EQ(tab_list->GetTabCount(), 2);
  EXPECT_EQ(tab_list->GetActiveTab(), other_tab);
  EXPECT_NE(tab_list->GetActiveTab(), initial_tab);
}

}  // namespace
}  // namespace glic
