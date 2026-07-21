// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_tab_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/webapps/twa_launch_navigation_handle_user_data.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"

namespace webapps {

class FakeWebLaunchService : public blink::mojom::WebLaunchService {
 public:
  FakeWebLaunchService() = default;
  ~FakeWebLaunchService() override = default;

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<blink::mojom::WebLaunchService>(
            std::move(handle)));
  }

  void EnqueueLaunchParams(
      const GURL& launch_url,
      base::TimeTicks time_navigation_started_in_browser,
      bool navigation_started,
      std::vector<blink::mojom::FileSystemAccessEntryPtr> files) override {
    launched_url_ = launch_url;
    enqueue_called_ = true;
  }

  bool enqueue_called() const { return enqueue_called_; }
  const GURL& launched_url() const { return launched_url_; }

  void Reset() {
    enqueue_called_ = false;
    launched_url_ = GURL();
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebLaunchService> receiver_{this};
  bool enqueue_called_ = false;
  GURL launched_url_;
};

class TestChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override {
    auto ui_data = std::make_unique<ChromeNavigationUIData>(navigation_handle);
    if (next_twa_launch_token_) {
      ui_data->set_twa_launch_token(next_twa_launch_token_);
      next_twa_launch_token_ = std::nullopt;
    }
    return ui_data;
  }

  void set_next_twa_launch_token(std::optional<int64_t> token) {
    next_twa_launch_token_ = token;
  }

 private:
  std::optional<int64_t> next_twa_launch_token_;
};

class TwaLaunchQueueTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_browser_client_ = std::make_unique<TestChromeContentBrowserClient>();
    old_browser_client_ =
        content::SetBrowserClientForTesting(test_browser_client_.get());

    TwaLaunchQueueTabHelper::CreateForWebContents(web_contents());
    tab_helper_ = TwaLaunchQueueTabHelper::FromWebContents(web_contents());
    InitTestApi(web_contents()->GetPrimaryMainFrame());
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(old_browser_client_);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void InitTestApi(content::RenderFrameHost* rfh) {
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        blink::mojom::WebLaunchService::Name_,
        base::BindRepeating(&FakeWebLaunchService::Bind,
                            base::Unretained(&fake_launch_service_)));
  }

 protected:
  LaunchParams CreateLaunchParams(const GURL& target_url) {
    LaunchParams params;
    params.set_target_url(target_url);
    params.set_started_new_navigation(true);
    params.set_app_id("test_app_id");
    params.set_scope(target_url);
    return params;
  }

  std::unique_ptr<TestChromeContentBrowserClient> test_browser_client_;
  raw_ptr<content::ContentBrowserClient> old_browser_client_;
  raw_ptr<TwaLaunchQueueTabHelper> tab_helper_;
  FakeWebLaunchService fake_launch_service_;
};

TEST_F(TwaLaunchQueueTabHelperTest, SuccessSameOrigin) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();

  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, SuccessSameOriginVerifyAfterCommit) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  // Verification is still pending, so it shouldn't be enqueued yet.
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // Now verify.
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, DiscardOnCrossOriginRedirect) {
  GURL target_url("https://example.com/twa");
  GURL redirect_url("https://malicious.com/hijack");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  tab_helper_->OnLaunchVerified(token, /*success=*/true);

  simulator->Redirect(redirect_url);
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, DiscardOnVerificationFailed) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();

  tab_helper_->OnLaunchVerified(token, /*success=*/false);
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, EnqueueNonNavigatingSameOrigin) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             target_url);

  tab_helper_->EnqueueNonNavigating(params);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, EnqueueNonNavigatingCrossOrigin) {
  GURL target_url("https://example.com/twa");
  GURL current_url("https://malicious.com/hijack");
  LaunchParams params = CreateLaunchParams(target_url);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             current_url);
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  tab_helper_->EnqueueNonNavigating(params);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, SuccessSameOriginInScope) {
  GURL target_url("https://example.com/twa/launch");
  GURL scope_url("https://example.com/twa/");
  LaunchParams params = CreateLaunchParams(target_url);
  params.set_scope(scope_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  tab_helper_->OnLaunchVerified(token, /*success=*/true);

  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, DiscardSameOriginOutOfScope) {
  GURL target_url("https://example.com/twa/launch");
  GURL scope_url("https://example.com/twa/");
  GURL navigated_url("https://example.com/other");
  LaunchParams params = CreateLaunchParams(target_url);
  params.set_scope(scope_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  tab_helper_->OnLaunchVerified(token, /*success=*/true);

  simulator->Redirect(navigated_url);
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, DiscardIfNavigatedAwayBeforeVerification) {
  GURL target_url("https://example.com/twa");
  GURL other_url("https://example.com/other");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  // Navigation 1: Launch navigation.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  // Verification is still pending, so it shouldn't be enqueued yet.
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // Navigation 2: Unrelated navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             other_url);
  // Re-init test API on the new RFH because it might have changed.
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  // Now verify the launch (late verification).
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  // It should NOT be enqueued because we navigated away to an unrelated page.
  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, SpeculativeLaunchSuccess) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  // Simulate speculative navigation committing before WebAppLaunchHandler is
  // ready.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             target_url);
  // Re-init test API because navigation committed and RFH might have changed.
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  // Now Java calls prepareForLaunch and then verifies.
  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/true);
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, SpeculativeLaunchFailed) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             target_url);
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/true);
  tab_helper_->OnLaunchVerified(token, /*success=*/false);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, SpeculativeLaunchURLMismatch) {
  GURL target_url("https://example.com/twa");
  GURL navigated_url("https://example.com/other");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             navigated_url);
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/true);
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, SpeculativeLaunchVerifyAfterCommit) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  // 1. Navigation starts speculatively (no token).
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();

  // 2. Java calls PrepareForLaunch (attaching it to pending_launches_).
  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/true);

  // 3. Navigation commits. DidFinishNavigation should match by URL and move to
  // committed_launches_.
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  // Verification is still pending.
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // 4. Verification completes.
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, VerifiedBeforeStart) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  tab_helper_->OnLaunchVerified(token,
                                /*success=*/true);  // Verified BEFORE start.

  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, VerificationFailedBeforeStart) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  tab_helper_->OnLaunchVerified(token,
                                /*success=*/false);  // Failed BEFORE start.

  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest,
       DiscardIfNavigatedToInScopeBeforeVerification) {
  GURL target_url("https://example.com/twa");
  GURL other_in_scope_url("https://example.com/twa/page2");
  LaunchParams params = CreateLaunchParams(target_url);
  params.set_scope(target_url);  // Scope is https://example.com/twa
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  // Navigation 1: Launch navigation.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();
  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  // Verification is still pending.
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // Navigation 2: Navigate to another IN-SCOPE URL (cross-document).
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), other_in_scope_url);
  InitTestApi(web_contents()->GetPrimaryMainFrame());

  // Now verify the launch.
  tab_helper_->OnLaunchVerified(token, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();

  // It should NOT be enqueued because we navigated away (even if in-scope).
  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, VerifiedAfterStartBeforeCommit) {
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  int64_t token = 123;

  tab_helper_->PrepareForLaunch(token, params,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      target_url, web_contents());
  simulator->Start();  // Navigation started.

  tab_helper_->OnLaunchVerified(
      token,
      /*success=*/true);  // Verified AFTER start, BEFORE commit.

  simulator->Commit();
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url);
}

TEST_F(TwaLaunchQueueTabHelperTest, EnqueueNonNavigatingInScope) {
  GURL current_url("https://example.com/twa/page");
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  params.set_scope(target_url);

  // Set current URL.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             current_url);

  tab_helper_->EnqueueNonNavigating(params);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_TRUE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, EnqueueNonNavigatingOutOfScope) {
  GURL current_url("https://example.com/out-of-scope");
  GURL target_url("https://example.com/twa");
  LaunchParams params = CreateLaunchParams(target_url);
  params.set_scope(target_url);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             current_url);

  tab_helper_->EnqueueNonNavigating(params);
  tab_helper_->FlushLaunchQueueForTesting();

  EXPECT_FALSE(fake_launch_service_.enqueue_called());
}

TEST_F(TwaLaunchQueueTabHelperTest, AbortedNavigationClearsActiveLaunch) {
  GURL target_url1("https://example.com/twa1");
  GURL target_url2("https://example.com/twa2");
  LaunchParams params1 = CreateLaunchParams(target_url1);
  LaunchParams params2 = CreateLaunchParams(target_url2);
  int64_t token1 = 123;
  int64_t token2 = 456;

  // Start Navigation 1.
  tab_helper_->PrepareForLaunch(token1, params1,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token1);
  auto simulator1 = content::NavigationSimulator::CreateBrowserInitiated(
      target_url1, web_contents());
  simulator1->Start();

  // Start Navigation 2 (aborts 1).
  tab_helper_->PrepareForLaunch(token2, params2,
                                /*has_speculative_navigation=*/false);
  test_browser_client_->set_next_twa_launch_token(token2);
  auto simulator2 = content::NavigationSimulator::CreateBrowserInitiated(
      target_url2, web_contents());
  simulator2->Start();

  // Verify token 1.
  tab_helper_->OnLaunchVerified(token1, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // Verify token 2 and commit.
  simulator2->Commit();
  tab_helper_->OnLaunchVerified(token2, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();
  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url2);
}

TEST_F(TwaLaunchQueueTabHelperTest, PrepareForLaunchClearsPending) {
  GURL target_url1("https://example.com/twa1");
  GURL target_url2("https://example.com/twa2");
  LaunchParams params1 = CreateLaunchParams(target_url1);
  LaunchParams params2 = CreateLaunchParams(target_url2);
  int64_t token1 = 123;
  int64_t token2 = 456;

  // Prepare launch 1 (speculative).
  tab_helper_->PrepareForLaunch(token1, params1,
                                /*has_speculative_navigation=*/true);

  // Prepare launch 2 (supersedes 1).
  tab_helper_->PrepareForLaunch(token2, params2,
                                /*has_speculative_navigation=*/false);

  // Try to start navigation with token 1. It should NOT match because it was
  // cleared.
  test_browser_client_->set_next_twa_launch_token(token1);
  auto simulator1 = content::NavigationSimulator::CreateBrowserInitiated(
      target_url1, web_contents());
  simulator1->Start();
  simulator1->Commit();
  tab_helper_->OnLaunchVerified(token1, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();
  EXPECT_FALSE(fake_launch_service_.enqueue_called());

  // Start navigation with token 2. It should match and succeed.
  test_browser_client_->set_next_twa_launch_token(token2);
  auto simulator2 = content::NavigationSimulator::CreateBrowserInitiated(
      target_url2, web_contents());
  simulator2->Start();
  simulator2->Commit();
  InitTestApi(web_contents()->GetPrimaryMainFrame());
  tab_helper_->OnLaunchVerified(token2, /*success=*/true);
  tab_helper_->FlushLaunchQueueForTesting();
  EXPECT_TRUE(fake_launch_service_.enqueue_called());
  EXPECT_EQ(fake_launch_service_.launched_url(), target_url2);
}

}  // namespace webapps
