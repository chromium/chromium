// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_browser_host_impl.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace geic {
namespace {

using GetContextResult =
    base::expected<mojom::TabContextDataPtr, mojom::GetTabContextError>;

class FakeGeicClient : public mojom::GeicClient {
 public:
  void OnFocusedTabChanged(mojom::FocusedTabDataPtr data) override {
    last_data_ = std::move(data);
  }

  mojom::FocusedTabDataPtr last_data_;
};

class GeicBrowserHostImplTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        pwc::mojom::features::kPrivilegedWebContents);
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), GURL("https://example.com/initial"));
    tab_ = browser()->GetActiveTabInterface();
    ASSERT_TRUE(tab_);

    host_impl_ = std::make_unique<GeicBrowserHostImpl>(tab_);
    host_impl_->BindBrowserHost(host_remote_.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<mojom::GeicClient> client_remote =
        client_receiver_.BindNewPipeAndPassRemote();
    base::test::TestFuture<mojom::GeicInitialStatePtr> initial_state_future;
    host_remote_->RegisterClient(std::move(client_remote),
                                 initial_state_future.GetCallback());
    auto initial_state = initial_state_future.Take();
    ASSERT_TRUE(initial_state);
    ASSERT_TRUE(initial_state->focused_tab_data);
    EXPECT_TRUE(initial_state->focused_tab_data->is_focused_tab());
  }

  void TearDown() override {
    host_impl_.reset();
    tab_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  FakeGeicClient client_;
  mojo::Receiver<mojom::GeicClient> client_receiver_{&client_};
  std::unique_ptr<GeicBrowserHostImpl> host_impl_;
  mojo::Remote<mojom::GeicBrowserHost> host_remote_;
};

TEST_F(GeicBrowserHostImplTest, GetFocusedTabReturnsFocusedTabDataWhenActive) {
  base::test::TestFuture<mojom::FocusedTabDataPtr> future;
  host_remote_->GetFocusedTab(future.GetCallback());
  auto data = future.Take();
  ASSERT_TRUE(data);
  ASSERT_TRUE(data->is_focused_tab());
  EXPECT_EQ(data->get_focused_tab()->url, GURL("https://example.com/initial"));
}

TEST_F(GeicBrowserHostImplTest,
       RegisterClientInitialStateMatchesGetFocusedTab) {
  // Create a separate host instance to test initial handshake agreement.
  mojo::Remote<mojom::GeicBrowserHost> fresh_remote;
  GeicBrowserHostImpl fresh_host(tab_);
  fresh_host.BindBrowserHost(fresh_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<mojom::FocusedTabDataPtr> tab_future;
  fresh_remote->GetFocusedTab(tab_future.GetCallback());
  auto direct_tab_data = tab_future.Take();
  ASSERT_TRUE(direct_tab_data);

  FakeGeicClient fresh_client;
  mojo::Receiver<mojom::GeicClient> fresh_receiver{&fresh_client};
  base::test::TestFuture<mojom::GeicInitialStatePtr> register_future;
  fresh_remote->RegisterClient(fresh_receiver.BindNewPipeAndPassRemote(),
                               register_future.GetCallback());
  auto initial_state = register_future.Take();
  ASSERT_TRUE(initial_state);
  ASSERT_TRUE(initial_state->focused_tab_data);

  // Both paths must return matching state structure and URL.
  ASSERT_EQ(initial_state->focused_tab_data->which(), direct_tab_data->which());
  ASSERT_TRUE(initial_state->focused_tab_data->is_focused_tab());
  EXPECT_EQ(initial_state->focused_tab_data->get_focused_tab()->url,
            direct_tab_data->get_focused_tab()->url);
}

TEST_F(GeicBrowserHostImplTest,
       GetContextFromFocusedTabReturnsErrorWhenTabClosed) {
  tab_ = nullptr;
  browser()->tab_strip_model()->CloseAllTabs();
  base::test::TestFuture<GetContextResult> future;
  host_remote_->GetContextFromFocusedTab(mojom::TabContextOptions::New(),
                                         future.GetCallback());
  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kTabClosed);
}

TEST_F(GeicBrowserHostImplTest,
       GetValidatedActiveTabReturnsNoActiveTabWhenTabNull) {
  GeicBrowserHostImpl null_host(nullptr);
  auto validated = null_host.GetValidatedActiveTab();
  EXPECT_FALSE(validated.contents);
  EXPECT_FALSE(validated.metadata);
  EXPECT_EQ(validated.rejection, RejectionKind::kNoActiveTab);
}

TEST_F(GeicBrowserHostImplTest, ActivityInSecondWindowDoesNotAffectHost) {
  std::unique_ptr<Browser> window2 =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, /*hosted_app=*/false);
  AddTab(window2.get(), GURL("https://example.com/window2_tab"));

  // host_impl_ is attached to tab_ in browser() (Window 1).
  // Activity in Window 2 should not change host_impl_'s focused tab data.
  {
    auto validated = host_impl_->GetValidatedActiveTab();
    ASSERT_TRUE(validated.contents);
    EXPECT_EQ(validated.metadata->url, GURL("https://example.com/initial"));
    EXPECT_EQ(validated.metadata->window_id, browser()->GetSessionID().id());
  }

  window2->tab_strip_model()->CloseAllTabs();
}

TEST_F(GeicBrowserHostImplTest, TabMovedBetweenWindowsFollowsNewWindow) {
  std::unique_ptr<Browser> window2 =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, /*hosted_app=*/false);

  // Move tab_ from browser() to window2:
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(0);
  ASSERT_EQ(detached_tab.get(), tab_);
  window2->tab_strip_model()->InsertDetachedTabAt(0, std::move(detached_tab),
                                                  AddTabTypes::ADD_ACTIVE);

  // Verify host_impl_ follows tab_ to window2 dynamically:
  EXPECT_EQ(tab_->GetBrowserWindowInterface(), window2.get());
  {
    auto validated = host_impl_->GetValidatedActiveTab();
    ASSERT_TRUE(validated.contents);
    EXPECT_EQ(validated.metadata->url, GURL("https://example.com/initial"));
    EXPECT_EQ(validated.metadata->window_id, window2->GetSessionID().id());
  }

  tab_ = nullptr;
  window2->tab_strip_model()->CloseAllTabs();
}

TEST_F(GeicBrowserHostImplTest, NavigationDuringExtractionReturnsError) {

  auto validated = host_impl_->GetValidatedActiveTab();
  ASSERT_TRUE(validated.contents);
  ASSERT_EQ(validated.rejection, RejectionKind::kNone);

  auto options = mojom::TabContextOptions::New();
  options->include_inner_text = true;
  base::test::TestFuture<GetContextResult> future;
  host_impl_->GetContextFromFocusedTab(std::move(options),
                                       future.GetCallback());

  // Simulate a page navigation before async extraction callback completes.
  NavigateAndCommitActiveTab(GURL("https://example.com/navigated"));

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kNavigationInProgress);
}

TEST_F(GeicBrowserHostImplTest,
       SubframeNavigationDuringExtractionDoesNotReturnError) {
  AddTab(browser(), GURL("https://example.com/initial"));
  NavigateAndCommitActiveTab(GURL("https://example.com/initial"));

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);

  // Append a subframe to the primary main frame.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(
          active_contents->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  ASSERT_TRUE(subframe);

  auto options = mojom::TabContextOptions::New();
  options->include_inner_text = true;
  base::test::TestFuture<GetContextResult> future;
  host_impl_->GetContextFromFocusedTab(std::move(options),
                                       future.GetCallback());

  // Simulate a subframe navigation while extraction is in flight.
  auto subframe_nav = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/subframe_nav"), subframe);
  subframe_nav->Commit();

  auto result = future.Take();
  // Primary main frame document has not changed, so subframe navigation
  // is deliberately ignored and does not produce an error.
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
}

TEST_F(GeicBrowserHostImplTest, IsTabValidForSharingAllowsHttpAndHttpsOnly) {
  EXPECT_FALSE(IsTabValidForSharing(nullptr));

  // Regular web contents with HTTPS URL:
  std::unique_ptr<content::WebContents> test_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      test_contents.get(), GURL("https://example.com/page"));
  EXPECT_TRUE(IsTabValidForSharing(test_contents.get()));

  // Regular web contents with HTTP URL:
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      test_contents.get(), GURL("http://example.com/page"));
  EXPECT_TRUE(IsTabValidForSharing(test_contents.get()));

  // about:blank URL:
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      test_contents.get(), GURL("about:blank"));
  EXPECT_FALSE(IsTabValidForSharing(test_contents.get()));

  // data: URL:
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      test_contents.get(), GURL("data:text/html,<h1>test</h1>"));
  EXPECT_FALSE(IsTabValidForSharing(test_contents.get()));

  // Another PrivilegedWebContents (PWC) serving over HTTPS:
  GURL pwc_url("https://localhost.corp.google.com:10443/side-panel");
  url::Origin origin = url::Origin::Create(pwc_url);
  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{origin}, std::vector<url::Origin>{origin});
  std::unique_ptr<pwc::PrivilegedWebContents> pwc_contents =
      pwc::PrivilegedWebContents::Create(pwc::PrivilegedComponent::kGeic,
                                         profile(), std::move(policy_delegate));
  ASSERT_TRUE(pwc_contents);
  ASSERT_TRUE(pwc_contents->web_contents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      pwc_contents->web_contents(), pwc_url);
  EXPECT_FALSE(IsTabValidForSharing(pwc_contents->web_contents()));
}

}  // namespace
}  // namespace geic
