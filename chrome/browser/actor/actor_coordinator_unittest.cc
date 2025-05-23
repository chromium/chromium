// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_coordinator.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/gurl.h"

namespace actor {

namespace {

constexpr int kFakeContentNodeId = 123;

class FakeChromeRenderFrame : public chrome::mojom::ChromeRenderFrame {
 public:
  FakeChromeRenderFrame() = default;
  ~FakeChromeRenderFrame() override = default;

  void OverrideBinder(content::RenderFrameHost* rfh) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        rfh->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        chrome::mojom::ChromeRenderFrame::Name_,
        base::BindRepeating(&FakeChromeRenderFrame::Bind,
                            base::Unretained(this)));
  }

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override {}
  void RequestReloadImageForContextNode() override {}
  void RequestBitmapForContextNode(
      RequestBitmapForContextNodeCallback callback) override {}
  void RequestBitmapForContextNodeWithBoundsHint(
      RequestBitmapForContextNodeWithBoundsHintCallback callback) override {}
  void RequestBoundsHintForAllImages(
      RequestBoundsHintForAllImagesCallback callback) override {}
  void RequestImageForContextNode(
      int32_t image_min_area_pixels,
      const gfx::Size& image_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      int32_t quality,
      RequestImageForContextNodeCallback callback) override {}
  void ExecuteWebUIJavaScript(const std::u16string& javascript) override {}
  void GetMediaFeedURL(GetMediaFeedURLCallback callback) override {}
  void LoadBlockedPlugins(const std::string& identifier) override {}
  void SetSupportsDraggableRegions(bool supports_draggable_regions) override {}
  void SetShouldDeferMediaLoad(bool should_defer) override {}

  void InvokeTool(actor::mojom::ToolInvocationPtr request,
                  InvokeToolCallback callback) override {
    std::move(callback).Run(MakeOkResult());
  }

 private:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
            std::move(handle)));
  }

  mojo::AssociatedReceiver<chrome::mojom::ChromeRenderFrame> receiver_{this};
};

class ActorCoordinatorTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorCoordinatorTest() = default;
  ~ActorCoordinatorTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);

    ChromeRenderViewHostTestHarness::SetUp();

    AssociateTabInterface();
  }

  void TearDown() override {
    ClearTabInterface();

    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  bool Act(const GURL& url,
           const optimization_guide::proto::BrowserAction& action) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);

    FakeChromeRenderFrame fake_chrome_render_frame;
    fake_chrome_render_frame.OverrideBinder(main_rfh());

    base::test::TestFuture<mojom::ActionResultPtr> success;
    ActorCoordinator coordinator(profile());
    coordinator.StartTaskForTesting(GetTab());
    coordinator.Act(action, success.GetCallback());
    return IsOk(*success.Get());
  }

  tabs::MockTabInterface* GetTab() {
    return tab_state_ ? &tab_state_->tab : nullptr;
  }

  void AssociateTabInterface() { tab_state_.emplace(web_contents()); }
  void ClearTabInterface() { tab_state_.reset(); }

 private:
  struct TabState {
    explicit TabState(content::WebContents* web_contents) : weak_factory(&tab) {
      ON_CALL(tab, GetWeakPtr)
          .WillByDefault(::testing::Return(weak_factory.GetWeakPtr()));
      ON_CALL(tab, GetContents).WillByDefault(::testing::Return(web_contents));
    }

    tabs::MockTabInterface tab;
    base::WeakPtrFactory<tabs::MockTabInterface> weak_factory;
  };
  std::optional<TabState> tab_state_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ActorCoordinatorTest, ActSucceedsOnSupportedUrl) {
  EXPECT_TRUE(Act(GURL("http://localhost/"),
                  MakeClick(*main_rfh(), kFakeContentNodeId)));
}

TEST_F(ActorCoordinatorTest, ActFailsOnUnsupportedUrl) {
  EXPECT_FALSE(Act(GURL(chrome::kChromeUIVersionURL),
                   MakeClick(*main_rfh(), kFakeContentNodeId)));
}

TEST_F(ActorCoordinatorTest, ActFailsWhenTabDestroyed) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  base::test::TestFuture<mojom::ActionResultPtr> result;
  ActorCoordinator coordinator(profile());
  coordinator.StartTaskForTesting(GetTab());

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  coordinator.Act(MakeClick(*main_rfh(), kFakeContentNodeId),
                  result.GetCallback());

  ClearTabInterface();
  DeleteContents();

  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

TEST_F(ActorCoordinatorTest, CrossOriginNavigationBeforeAction) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost/"));

  FakeChromeRenderFrame fake_chrome_render_frame;
  fake_chrome_render_frame.OverrideBinder(main_rfh());

  base::test::TestFuture<mojom::ActionResultPtr> result;
  ActorCoordinator coordinator(profile());
  coordinator.StartTaskForTesting(GetTab());
  coordinator.Act(MakeClick(*main_rfh(), kFakeContentNodeId),
                  result.GetCallback());

  // Before the action happens, commit a cross-origin navigation.
  ASSERT_FALSE(result.IsReady());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://localhost:8000/"));

  // TODO(mcnee): We currently just fail, but this should do something more
  // graceful.
  ExpectErrorResult(result, mojom::ActionResultCode::kCrossOriginNavigation);
}

}  // namespace

}  // namespace actor
