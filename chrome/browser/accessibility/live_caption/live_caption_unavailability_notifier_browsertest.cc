// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_unavailability_notifier.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption/live_caption_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/live_caption_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace captions {

class LiveCaptionUnavailabilityNotifierTest : public LiveCaptionBrowserTest {
 public:
  LiveCaptionUnavailabilityNotifierTest() = default;
  ~LiveCaptionUnavailabilityNotifierTest() override = default;
  LiveCaptionUnavailabilityNotifierTest(
      const LiveCaptionUnavailabilityNotifierTest&) = delete;
  LiveCaptionUnavailabilityNotifierTest& operator=(
      const LiveCaptionUnavailabilityNotifierTest&) = delete;

  // LiveCaptionBrowserTest:
  void SetUp() override {
    // This is required for the fullscreen video tests.
    embedded_test_server()->ServeFilesFromSourceDirectory(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    LiveCaptionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreateLiveCaptionUnavailabilityNotifier(
      content::RenderFrameHost* frame_host) {
    mojo::Remote<media::mojom::MediaFoundationRendererNotifier> remote;
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver;
    remote.Bind(receiver.InitWithNewPipeAndPassRemote());
    LiveCaptionUnavailabilityNotifier::Create(frame_host, std::move(receiver));
    remotes_.emplace(frame_host, std::move(remote));
  }

  void MediaFoundationRendererCreated(content::RenderFrameHost* frame_host) {
    mojo::PendingRemote<media::mojom::MediaFoundationRendererObserver> remote;
    remotes_[frame_host]->MediaFoundationRendererCreated(
        remote.InitWithNewPipeAndPassReceiver());
  }

  bool HasBubbleController() {
    return LiveCaptionControllerFactory::GetForProfile(browser()->profile())
               ->caption_bubble_controller_for_testing() != nullptr;
  }

  void ExpectIsWidgetVisible(bool visible) {
#if defined(TOOLKIT_VIEWS)
    CaptionBubbleController* bubble_controller =
        LiveCaptionControllerFactory::GetForProfile(browser()->profile())
            ->caption_bubble_controller_for_testing();
    EXPECT_EQ(visible, bubble_controller->IsWidgetVisibleForTesting());
#endif
  }

  void DestroyNotifiers() { remotes_.clear(); }

 private:
  std::map<content::RenderFrameHost*,
           mojo::Remote<media::mojom::MediaFoundationRendererNotifier>>
      remotes_;
};

IN_PROC_BROWSER_TEST_F(LiveCaptionUnavailabilityNotifierTest,
                       CaptionBubbleDestroyed) {
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/40898509): Remove when live captioning is supported.
  GTEST_SKIP() << "Live captioning not supported on Win Arm64";
#else
  content::RenderFrameHost* frame_host = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  CreateLiveCaptionUnavailabilityNotifier(frame_host);

  SetLiveCaptionEnabled(true);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(false);

  MediaFoundationRendererCreated(frame_host);
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(true);

  DestroyNotifiers();
  base::RunLoop().RunUntilIdle();
  ExpectIsWidgetVisible(false);
#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
}

}  // namespace captions
