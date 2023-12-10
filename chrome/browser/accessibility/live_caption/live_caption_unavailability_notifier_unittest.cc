// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_unavailability_notifier.h"

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleSiteSameOrigin[] = "https://example.com/test";
constexpr char kExampleSiteDifferentOrigin[] = "https://test.com";

namespace captions {

class LiveCaptionUnavailabilityNotifierTest
    : public ChromeRenderViewHostTestHarness {
 public:
  LiveCaptionUnavailabilityNotifierTest() = default;
  ~LiveCaptionUnavailabilityNotifierTest() override = default;
  LiveCaptionUnavailabilityNotifierTest(
      const LiveCaptionUnavailabilityNotifierTest&) = delete;
  LiveCaptionUnavailabilityNotifierTest& operator=(
      const LiveCaptionUnavailabilityNotifierTest&) = delete;

  bool ErrorSilencedForOrigin() { return notifier_->ErrorSilencedForOrigin(); }

  void OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(
      bool checked) {
    notifier_->OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(
        CaptionBubbleErrorType::kMediaFoundationRendererUnsupported, checked);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kExampleSite));

    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver;
    remote_.Bind(receiver.InitWithNewPipeAndPassRemote());

    // The LiveCaptionUnavailabilityNotifier is self-owned and is reset upon the
    // destruction of the mojo connection.
    notifier_ =
        new LiveCaptionUnavailabilityNotifier(*main_rfh(), std::move(receiver));
  }

  void TearDown() override {
    notifier_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  mojo::Remote<media::mojom::MediaFoundationRendererNotifier> remote_;
  raw_ptr<LiveCaptionUnavailabilityNotifier> notifier_ = nullptr;
};

TEST_F(LiveCaptionUnavailabilityNotifierTest, MediaFoundationRendererCreated) {
  ASSERT_FALSE(ErrorSilencedForOrigin());

  OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(true);
  ASSERT_TRUE(ErrorSilencedForOrigin());

  NavigateAndCommit(GURL(kExampleSiteSameOrigin));
  ASSERT_TRUE(ErrorSilencedForOrigin());

  OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(false);
  ASSERT_FALSE(ErrorSilencedForOrigin());

  NavigateAndCommit(GURL(kExampleSiteDifferentOrigin));
  ASSERT_FALSE(ErrorSilencedForOrigin());
}

}  // namespace captions
