// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicGeminiEnterpriseManagerBrowserTest : public PlatformBrowserTest {
 public:
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
        ::switches::kGlicGuestURL,
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

}  // namespace
}  // namespace glic
