// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic_pwc_policy_delegate.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/fake_web_contents_manager.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class FakeAutoplayConfigurationClient
    : public blink::mojom::AutoplayConfigurationClient {
 public:
  void OverrideOnFrame(content::RenderFrameHost* frame) {
    frame->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        blink::mojom::AutoplayConfigurationClient::Name_,
        base::BindRepeating(&FakeAutoplayConfigurationClient::Bind,
                            base::Unretained(this)));
  }

  // blink::mojom::AutoplayConfigurationClient:
  void AddAutoplayFlags(const url::Origin& origin, int32_t flags) override {
    granted_calls_.emplace_back(origin, flags);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  std::pair<url::Origin, int32_t> WaitForNextCall() {
    if (consumed_calls_ >= granted_calls_.size()) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return granted_calls_[consumed_calls_++];
  }

  const std::vector<std::pair<url::Origin, int32_t>>& granted_calls() const {
    return granted_calls_;
  }

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<
                       blink::mojom::AutoplayConfigurationClient>(
                       std::move(handle)));
  }

  mojo::AssociatedReceiverSet<blink::mojom::AutoplayConfigurationClient>
      receivers_;
  std::vector<std::pair<url::Origin, int32_t>> granted_calls_;
  size_t consumed_calls_ = 0;
  base::OnceClosure quit_closure_;
};

class GlicGuestObserverTest : public ChromeRenderViewHostTestHarness {
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeAutoplayConfigurationClient autoplay_client_;
};

TEST_F(GlicGuestObserverTest, GrantsAutoplayForGuestOrigin) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  navigation->ReadyToCommit();
  autoplay_client_.OverrideOnFrame(
      navigation->GetNavigationHandle()->GetRenderFrameHost());
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();

  EXPECT_EQ(autoplay_client_.WaitForNextCall(),
            std::make_pair(
                url::Origin::Create(GURL("https://gemini.google.com")),
                blink::mojom::kAutoplayFlagForceAllow));
  EXPECT_EQ(autoplay_client_.granted_calls().size(), 1u);
}

TEST_F(GlicGuestObserverTest, GrantsAutoplayForCorpOrigin) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.corp.google.com/app"), web_contents());
  navigation->ReadyToCommit();
  autoplay_client_.OverrideOnFrame(
      navigation->GetNavigationHandle()->GetRenderFrameHost());
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();

  EXPECT_EQ(autoplay_client_.WaitForNextCall(),
            std::make_pair(
                url::Origin::Create(GURL("https://gemini.corp.google.com")),
                blink::mojom::kAutoplayFlagForceAllow));
  EXPECT_EQ(autoplay_client_.granted_calls().size(), 1u);
}

TEST_F(GlicGuestObserverTest, DoesNotGrantAutoplayForDisallowedOrigin) {
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://evil.com"), web_contents());
  navigation->ReadyToCommit();
  autoplay_client_.OverrideOnFrame(
      navigation->GetNavigationHandle()->GetRenderFrameHost());
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(autoplay_client_.granted_calls().empty());
}

TEST_F(GlicGuestObserverTest, DoesNotGrantAutoplayForSubframe) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://gemini.google.com/subframe"), subframe);
  subframe_simulator->ReadyToCommit();
  autoplay_client_.OverrideOnFrame(
      subframe_simulator->GetNavigationHandle()->GetRenderFrameHost());
  GrantAutoplayPermissions(subframe_simulator->GetNavigationHandle());
  subframe_simulator->Commit();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(autoplay_client_.granted_calls().empty());
}

TEST_F(GlicGuestObserverTest, RecordsMetricsWhenNoWebviewEnabled) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);

  // Subsequent navigation or reload must not record again.
  content::NavigationSimulator::Reload(web_contents());
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsWhenNoWebviewDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsForSubframeNavigation) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://gemini.google.com/subframe"), subframe);
  subframe_simulator->Commit();

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsForSameDocument) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  // Initial cross-document navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);

  // Same-document navigation (e.g. fragment link).
  auto same_doc = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://gemini.google.com/app#hash"), main_rfh());
  same_doc->CommitSameDocument();

  // Counts should remain 1.
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

TEST_F(GlicGuestObserverTest,
       DoesNotRecordMetricsForErrorPageAndRecoversOnSuccessfulReload) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  // Simulate navigation failing and committing an error page.
  auto failed_nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  failed_nav->Fail(net::ERR_CONNECTION_FAILED);
  failed_nav->CommitErrorPage();

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);

  // Subsequent successful navigation on reload should be recorded.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

TEST_F(GlicGuestObserverTest,
       HiddenWebContentsRemainsHiddenAndUncapturedAcrossNavigations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      ::features::kPopulateUnattachedAuraScreenInfo);

  web_contents()->WasHidden();
  ASSERT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  ASSERT_FALSE(web_contents()->IsBeingCaptured());

  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  EXPECT_FALSE(web_contents()->IsBeingCaptured());

  // Cross-document navigation on hidden WebContents.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  EXPECT_FALSE(web_contents()->IsBeingCaptured());

  // Same-document navigation on hidden WebContents.
  auto same_doc = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://gemini.google.com/app#hash"), main_rfh());
  same_doc->CommitSameDocument();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  EXPECT_FALSE(web_contents()->IsBeingCaptured());
}

TEST_F(GlicGuestObserverTest,
       DoesNotTagUnnavigatedPrivilegedPlaceholderAndTagsSpeculativeMainFrame) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  auto pwc = pwc::PrivilegedWebContents::Create(
      pwc::PrivilegedComponent::kGlic, profile(),
      std::make_unique<GlicPwcPolicyDelegate>(profile()));
  content::WebContents* guest_contents = pwc->web_contents();
  content::RenderProcessHost* placeholder_rph =
      guest_contents->GetPrimaryMainFrame()->GetProcess();
  ASSERT_TRUE(
      guest_contents->GetPrimaryMainFrame()->GetSiteInstance()->HasProcess());
  ASSERT_FALSE(guest_contents->GetPrimaryMainFrame()->IsRenderFrameLive());

  FakeWebContentsManager contents_manager;
  PrepareGlicGuestWebContents(*guest_contents, contents_manager);
  EXPECT_FALSE(IsProcessHostForGlic(placeholder_rph));

  class SpeculativeFrameCheckObserver : public content::WebContentsObserver {
   public:
    explicit SpeculativeFrameCheckObserver(content::WebContents* contents)
        : content::WebContentsObserver(contents) {}

    void RenderFrameCreated(content::RenderFrameHost* rfh) override {
      if (!rfh->GetParentOrOuterDocument()) {
        saw_main_frame_created_ = true;
        was_active_primary_main_frame_ = rfh->IsInPrimaryMainFrame();
        was_marked_in_render_frame_created_ =
            IsProcessHostForGlic(rfh->GetProcess());
      }
    }

    bool saw_main_frame_created() const { return saw_main_frame_created_; }
    bool was_active_primary_main_frame() const {
      return was_active_primary_main_frame_;
    }
    bool was_marked_in_render_frame_created() const {
      return was_marked_in_render_frame_created_;
    }

   private:
    bool saw_main_frame_created_ = false;
    bool was_active_primary_main_frame_ = true;
    bool was_marked_in_render_frame_created_ = false;
  };

  SpeculativeFrameCheckObserver frame_observer(guest_contents);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      guest_contents, GURL("https://gemini.google.com/app"));

  EXPECT_TRUE(frame_observer.saw_main_frame_created());
  EXPECT_FALSE(frame_observer.was_active_primary_main_frame());
  EXPECT_TRUE(frame_observer.was_marked_in_render_frame_created());
  EXPECT_TRUE(IsProcessHostForGlic(
      guest_contents->GetPrimaryMainFrame()->GetProcess()));
  EXPECT_FALSE(IsProcessHostForGlic(placeholder_rph));

  // Cross-site subframes in a distinct RenderProcessHost must not be tagged.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(guest_contents->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.example.com/subframe"), subframe);
  ASSERT_NE(subframe->GetProcess(),
            guest_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(IsProcessHostForGlic(subframe->GetProcess()));
}

TEST_F(GlicGuestObserverTest,
       PrepareGlicGuestWebContentsTagsPrivilegedOrGuestProcessByMode) {
  FakeWebContentsManager contents_manager;

  // In NoWebview mode, an already-navigated privileged guest process is tagged
  // immediately by PrepareGlicGuestWebContents().
  {
    base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
    auto pwc = pwc::PrivilegedWebContents::Create(
        pwc::PrivilegedComponent::kGlic, profile(),
        std::make_unique<GlicPwcPolicyDelegate>(profile()));
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        pwc->web_contents(), GURL("https://gemini.google.com/app"));
    ASSERT_TRUE(pwc->web_contents()
                    ->GetPrimaryMainFrame()
                    ->GetProcess()
                    ->IsPrivileged());

    PrepareGlicGuestWebContents(*pwc->web_contents(), contents_manager);
    EXPECT_TRUE(IsProcessHostForGlic(
        pwc->web_contents()->GetPrimaryMainFrame()->GetProcess()));
  }

  // In <webview> mode, only IsForGuestsOnly() processes are tagged by
  // PrepareGlicGuestWebContents().
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicNoWebview);

    PrepareGlicGuestWebContents(*web_contents(), contents_manager);
    EXPECT_FALSE(IsProcessHostForGlic(main_rfh()->GetProcess()));

    auto guest_site_instance = content::SiteInstance::CreateForGuest(
        profile(), GetGlicStoragePartitionConfig(profile()));
    auto guest_wc = content::WebContents::Create(
        content::WebContents::CreateParams(profile(), guest_site_instance));
    ASSERT_TRUE(
        guest_wc->GetPrimaryMainFrame()->GetProcess()->IsForGuestsOnly());

    PrepareGlicGuestWebContents(*guest_wc, contents_manager);
    EXPECT_TRUE(
        IsProcessHostForGlic(guest_wc->GetPrimaryMainFrame()->GetProcess()));
  }
}

}  // namespace
}  // namespace glic
