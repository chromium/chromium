// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_manager.h"

#include <memory>
#include <optional>

#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicWebClientManagerNavigationTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicURLConfig,
          {{features::kGlicGuestURL.name, "https://gemini.google.com"}}},
         {features::kGlicCSPConfig,
          {{features::kGlicAllowedOriginsOverride.name, ""},
           {features::kGlicApiAllowedOrigins.name, ""}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DidFinishNavigationWaiter : public content::WebContentsObserver {
 public:
  DidFinishNavigationWaiter(content::WebContents* web_contents,
                            const GURL& target_url)
      : content::WebContentsObserver(web_contents), target_url_(target_url) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == target_url_) {
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  const GURL target_url_;
  base::RunLoop run_loop_;
};

}  // namespace

TEST(GlicWebClientManagerTest, ReasonForGuestProcessGoneMapsTerminationStatus) {
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_CRASHED),
            ClientLoadErrorReason::kGuestProcessCrashed);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_CRASHED),
            mojom::ClientLoadErrorReason::kGuestProcessCrashed);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED),
            ClientLoadErrorReason::kGuestProcessKilled);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED),
            mojom::ClientLoadErrorReason::kGuestProcessKilled);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_EVICTED_FOR_MEMORY),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_EVICTED_FOR_MEMORY),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
#endif
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM_PROTECTED),
            ClientLoadErrorReason::kGuestProcessGone);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM_PROTECTED),
            mojom::ClientLoadErrorReason::kGuestProcessGone);
#endif
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_LAUNCH_FAILED),
            ClientLoadErrorReason::kGuestProcessLaunchFailed);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_LAUNCH_FAILED),
            mojom::ClientLoadErrorReason::kGuestProcessLaunchFailed);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_INTEGRITY_FAILURE),
            ClientLoadErrorReason::kGuestProcessIntegrityFailure);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_INTEGRITY_FAILURE),
            mojom::ClientLoadErrorReason::kGuestProcessIntegrityFailure);
#endif
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_NORMAL_TERMINATION),
            ClientLoadErrorReason::kGuestProcessGone);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_ABNORMAL_TERMINATION),
            mojom::ClientLoadErrorReason::kGuestProcessGone);
}

TEST_F(GlicWebClientManagerNavigationTest,
       ReasonForClientLoadTimeoutTracksStages) {
  GlicWebClientManager manager;
  manager.AttachGuestContents(web_contents());

  // 1. Before any primary-main-frame navigation starts.
  EXPECT_FALSE(manager.HasInFlightNavigation());
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeout);

  // 2. Initial primary-main-frame navigation finishes without committing.
  {
    auto uncommitted = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/204"), web_contents());
    uncommitted->Start();
    EXPECT_TRUE(manager.HasInFlightNavigation());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress);
    uncommitted->AbortCommit();
    EXPECT_FALSE(manager.HasInFlightNavigation());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationNotCommitted);
  }

  // 3. Overlapping in-flight navigations keep the stage in-progress when an
  // earlier pending-commit navigation aborts while the newer one continues.
  {
    auto nav_a = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/a"), web_contents());
    nav_a->Start();
    nav_a->ReadyToCommit();
    auto nav_b = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/b"), web_contents());
    nav_b->Start();
    nav_a->AbortCommit();
    EXPECT_TRUE(manager.HasInFlightNavigation());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress);

    // 4. Committed regular API-allowed page before and after onload.
    nav_b->SetKeepLoading(true);
    nav_b->Commit();
    EXPECT_FALSE(manager.HasInFlightNavigation());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutBeforeOnload);
    nav_b->StopLoading();
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutAfterOnload);

    // Same-document navigations are ignored by
    // `PrimaryMainFrameNavigationTracker`.
    auto same_doc = content::NavigationSimulator::CreateRendererInitiated(
        GURL("https://gemini.google.com/b#section"), main_rfh());
    same_doc->CommitSameDocument();
    EXPECT_FALSE(manager.HasInFlightNavigation());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutAfterOnload);
  }

  // 5. Subsequent uncommitted navigation after a committed regular page.
  {
    auto uncommitted = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/204"), web_contents());
    uncommitted->Start();
    uncommitted->AbortCommit();
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationNotCommitted);
  }

  // 6. Non-regular (login) and disallowed-origin commits fall back to generic
  // kClientLoadTimeout, and AttachGuestContents() resets prior load state.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://accounts.google.com/ServiceLogin"));
  EXPECT_EQ(manager.last_committed_page_type(), mojom::GuestPageType::kLogin);
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeout);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/"));
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeout);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeoutAfterOnload);
  manager.AttachGuestContents(web_contents());
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeout);
}

TEST_F(GlicWebClientManagerNavigationTest,
       DidStartNavigationResetsLastCommittedPageTypeAndIgnoresLateOnload) {
  base::test::ScopedFeatureList ssr_loader;
  ssr_loader.InitAndEnableFeatureWithParameters(
      features::kGlicSsr,
      {{features::kGlicShowLoaderUntilMainPageOnload.name, "true"}});

  GlicWebClientManager manager;
  manager.AttachGuestContents(web_contents());

  // Navigating away from a login page clears `last_committed_page_type()`
  // immediately so `Host::ComputeIsShowingNativeLoading()` is not suppressed.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://accounts.google.com/ServiceLogin"));
  ASSERT_EQ(manager.last_committed_page_type(), mojom::GuestPageType::kLogin);

  auto regular_nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  regular_nav->SetKeepLoading(true);
  regular_nav->Start();
  EXPECT_EQ(manager.last_committed_page_type(), std::nullopt);

  regular_nav->Commit();
  ASSERT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeoutBeforeOnload);
  ASSERT_FALSE(manager.guest_main_page_loaded());

  // Starting a new cross-document navigation prevents a late onload from the
  // outgoing document from marking the main page loaded or changing the stage.
  auto next_nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://accounts.google.com/ServiceLogin"), web_contents());
  next_nav->Start();
  EXPECT_EQ(manager.last_committed_page_type(), std::nullopt);
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress);

  manager.DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_FALSE(manager.guest_main_page_loaded());
  EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
            ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress);
  next_nav->AbortCommit();
}

TEST_F(GlicWebClientManagerNavigationTest,
       BlockedNavigationPreservesOverlappingAndPreOnloadStages) {
  base::test::ScopedFeatureList ssr_loader;
  ssr_loader.InitAndEnableFeatureWithParameters(
      features::kGlicSsr,
      {{features::kGlicShowLoaderUntilMainPageOnload.name, "true"}});

  class MainPageLoadedCountingDelegate : public GlicWebClientManager::Delegate {
   public:
    void OnGuestMainPageLoaded() override { ++main_page_loaded_count_; }
    int main_page_loaded_count() const { return main_page_loaded_count_; }

   private:
    int main_page_loaded_count_ = 0;
  } delegate;

  GlicWebClientManager manager;
  manager.SetDelegate(&delegate);
  manager.AttachGuestContents(web_contents());

  content::TestNavigationThrottleInserter throttle_inserter(
      web_contents(),
      base::BindLambdaForTesting(
          [&](content::NavigationThrottleRegistry& registry) {
            content::NavigationHandle& handle = registry.GetNavigationHandle();
            const GURL url = handle.GetURL();
            std::optional<mojom::GuestPageType> blocked_type;
            if (url.path().starts_with("/blocked")) {
              blocked_type = mojom::GuestPageType::kDisabledByAdmin;
            } else if (url.host() == "blocked.example.com") {
              blocked_type = mojom::GuestPageType::kOriginBlocked;
            }
            if (!blocked_type) {
              return;
            }
            auto throttle =
                std::make_unique<content::TestNavigationThrottle>(registry);
            const int64_t nav_id = handle.GetNavigationId();
            throttle->SetCallback(
                content::TestNavigationThrottle::WILL_START_REQUEST,
                base::BindLambdaForTesting([&, nav_id, blocked_type, url]() {
                  manager.OnGuestNavigationBlocked(nav_id, *blocked_type, url);
                }));
            throttle->SetResponse(
                content::TestNavigationThrottle::WILL_START_REQUEST,
                content::TestNavigationThrottle::SYNCHRONOUS,
                content::NavigationThrottle::CANCEL_AND_IGNORE);
            registry.AddThrottle(std::move(throttle));
          }));

  // Commit an initial `gemini.google.com` document so same-site navigations
  // commit in the active `RenderFrameHost` rather than a speculative one that
  // `OnRequestFailedInternal` tears down when another navigation fails.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/initial"));

  // 1. A blocked navigation cancelled while an earlier pending-commit
  // navigation is still in flight leaves the stage in-progress and does not
  // carry its blocked status over if the earlier navigation later aborts.
  {
    auto nav_a = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/pending"), web_contents());
    nav_a->Start();
    nav_a->ReadyToCommit();

    const GURL blocked_url("https://gemini.google.com/blocked");
    DidFinishNavigationWaiter finish_waiter(web_contents(), blocked_url);
    auto blocked_b = content::NavigationSimulator::CreateBrowserInitiated(
        blocked_url, web_contents());
    blocked_b->Start();
    // `WillStartRequest` cancellation posts `OnRequestFailedInternal` to
    // dispatch `DidFinishNavigation` asynchronously.
    finish_waiter.Wait();
    EXPECT_TRUE(blocked_b->HasFailed());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress);

    nav_a->AbortCommit();
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutNavigationNotCommitted);
  }

  // 2. A standalone blocked navigation with no active client falls back to
  // generic `kClientLoadTimeout`.
  {
    const GURL blocked_url("https://gemini.google.com/blocked_initial");
    DidFinishNavigationWaiter finish_waiter(web_contents(), blocked_url);
    auto blocked = content::NavigationSimulator::CreateBrowserInitiated(
        blocked_url, web_contents());
    blocked->Start();
    finish_waiter.Wait();
    EXPECT_TRUE(blocked->HasFailed());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeout);
  }

  // 3. When a warmed client binds before `onload` and a disallowed navigation
  // is cancelled, the pre-onload stage is preserved so `onload` still
  // completes normally afterward.
  {
    const int baseline_loaded_count = delegate.main_page_loaded_count();
    auto regular_nav = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://gemini.google.com/app"), web_contents());
    regular_nav->SetKeepLoading(true);
    regular_nav->Start();
    regular_nav->Commit();
    ASSERT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutBeforeOnload);

    mojo::PendingRemote<mojom::WebClientHandler> client_remote;
    manager.SetPendingWebClientReceiver(
        client_remote.InitWithNewPipeAndPassReceiver());

    const GURL blocked_url("https://blocked.example.com/subframe_top");
    DidFinishNavigationWaiter finish_waiter(web_contents(), blocked_url);
    auto blocked = content::NavigationSimulator::CreateRendererInitiated(
        blocked_url, main_rfh());
    blocked->Start();
    finish_waiter.Wait();
    EXPECT_TRUE(blocked->HasFailed());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutBeforeOnload);
    EXPECT_FALSE(manager.guest_main_page_loaded());
    EXPECT_EQ(delegate.main_page_loaded_count(), baseline_loaded_count);

    regular_nav->StopLoading();
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutAfterOnload);
    EXPECT_TRUE(manager.guest_main_page_loaded());
    EXPECT_EQ(delegate.main_page_loaded_count(), baseline_loaded_count + 1);

    // 4. Cancelling a disallowed navigation after `onload` restores both
    // `kClientLoadTimeoutAfterOnload` and `guest_main_page_loaded()` (and
    // re-notifies `OnGuestMainPageLoaded()` after `DidStartNavigation()` reset
    // `guest_main_page_loaded_`).
    const GURL post_onload_blocked_url(
        "https://blocked.example.com/after_onload");
    DidFinishNavigationWaiter post_onload_waiter(web_contents(),
                                                 post_onload_blocked_url);
    auto blocked_after_onload =
        content::NavigationSimulator::CreateRendererInitiated(
            post_onload_blocked_url, main_rfh());
    blocked_after_onload->Start();
    post_onload_waiter.Wait();
    EXPECT_TRUE(blocked_after_onload->HasFailed());
    EXPECT_EQ(manager.ReasonForClientLoadTimeout(),
              ClientLoadErrorReason::kClientLoadTimeoutAfterOnload);
    EXPECT_TRUE(manager.guest_main_page_loaded());
    EXPECT_EQ(delegate.main_page_loaded_count(), baseline_loaded_count + 2);
  }
}

}  // namespace glic
