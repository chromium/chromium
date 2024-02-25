// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_surface.h"

#include <optional>

#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace captions {
namespace {

// A WebContentsObserver that runs tasks until some media starts / stops playing
// fullscreen.
class ScopedFullscreenRunner : public content::WebContentsObserver {
 public:
  explicit ScopedFullscreenRunner(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ScopedFullscreenRunner(const ScopedFullscreenRunner&) = delete;
  ScopedFullscreenRunner& operator=(const ScopedFullscreenRunner&) = delete;

  ~ScopedFullscreenRunner() override {
    // On destruction, wait for any fullscreen hooks to have executed. Then,
    // allow any tasks scheduled by the hooks to complete too.
    run_loop_.Run();
    base::RunLoop().RunUntilIdle();
  }

  void MediaEffectivelyFullscreenChanged(bool /*is_fullscreen*/) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

// A surface client whose methods can have expectations placed on them.
class MockSurfaceClient : public media::mojom::SpeechRecognitionSurfaceClient {
 public:
  MockSurfaceClient() = default;
  ~MockSurfaceClient() override = default;

  MockSurfaceClient(const MockSurfaceClient&) = delete;
  MockSurfaceClient& operator=(const MockSurfaceClient&) = delete;

  // media::mojom::SpeechRecognitionSurfaceClient:
  MOCK_METHOD(void, OnSessionEnded, (), (override));
  MOCK_METHOD(void, OnFullscreenToggled, (), (override));

  void BindToSurface(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
          receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface) {
    receiver_.Bind(std::move(receiver));
    surface_.Bind(std::move(surface));
  }

 private:
  // The remote must be held somewhere even though we don't call methods on it.
  mojo::Remote<media::mojom::SpeechRecognitionSurface> surface_;

  mojo::Receiver<media::mojom::SpeechRecognitionSurfaceClient> receiver_{this};
};

class LiveCaptionSurfaceTest : public InProcessBrowserTest {
 public:
  LiveCaptionSurfaceTest() = default;
  ~LiveCaptionSurfaceTest() override = default;

  LiveCaptionSurfaceTest(const LiveCaptionSurfaceTest&) = delete;
  LiveCaptionSurfaceTest& operator=(const LiveCaptionSurfaceTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Load premade test pages.
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    CHECK(embedded_test_server()->Start());
  }

  // Opens a new tab and waits for it to load the given URL.
  content::WebContents* LoadNewTab(GURL url) {
    content::RenderFrameHost* rfh = ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return content::WebContents::FromRenderFrameHost(rfh);
  }

  // Attaches a new surface to the given web contents and binds it to the given
  // client.
  LiveCaptionSurface* NewSurfaceForWebContents(
      content::WebContents* web_contents,
      MockSurfaceClient* mock_client) {
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSurface>
        surface_receiver;
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurfaceClient>
        client_remote;

    mock_client->BindToSurface(client_remote.InitWithNewPipeAndPassReceiver(),
                               surface_receiver.InitWithNewPipeAndPassRemote());

    auto* surface = LiveCaptionSurface::GetOrCreateForWebContents(web_contents);
    surface->BindToSurfaceClient(std::move(surface_receiver),
                                 std::move(client_remote));

    return surface;
  }

  const GURL kAboutBlankUrl = GURL(url::kAboutBlankURL);
};

// Test that the surface can be used to focus its tab.
IN_PROC_BROWSER_TEST_F(LiveCaptionSurfaceTest, Activate) {
  // Create an initial tab with a surface attached.
  MockSurfaceClient client;
  content::WebContents* wc_1 = LoadNewTab(kAboutBlankUrl);
  LiveCaptionSurface* surface = NewSurfaceForWebContents(wc_1, &client);

  // The initial tab should be focused.
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), wc_1);

  // Create a new tab, which should start focused.
  content::WebContents* wc_2 = LoadNewTab(kAboutBlankUrl);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), wc_2);

  // Activating the first tab via its surface should bring it into focus.
  surface->Activate();
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), wc_1);
}

// Test that the surface correctly reports tab bounds on screen.
IN_PROC_BROWSER_TEST_F(LiveCaptionSurfaceTest, Bounds) {
  // Create a tab with a surface attached.
  MockSurfaceClient client;
  LiveCaptionSurface* surface =
      NewSurfaceForWebContents(LoadNewTab(kAboutBlankUrl), &client);

  // Callback to assign bounds to local variables.
  const auto assign_bounds = [](gfx::Rect* d,
                                const std::optional<gfx::Rect>& b) {
    ASSERT_TRUE(b.has_value());
    *d = *b;
  };

  // Set known window bounds.
  const gfx::Rect window_bounds_1 = gfx::Rect(10, 10, 800, 600);
  browser()->window()->SetBounds(window_bounds_1);

  // Fetch bounds using the surface.
  gfx::Rect bounds_1;
  surface->GetBounds(base::BindOnce(assign_bounds, &bounds_1));
  base::RunLoop().RunUntilIdle();

  // The surface should have correctly reported the window bounds.
  EXPECT_EQ(window_bounds_1, bounds_1);

  // Set new window bounds.
  const gfx::Rect window_bounds_2 = gfx::Rect(50, 50, 800, 600);
  browser()->window()->SetBounds(window_bounds_2);

  // Fetch bounds using the surface.
  gfx::Rect bounds_2;
  surface->GetBounds(base::BindOnce(assign_bounds, &bounds_2));
  base::RunLoop().RunUntilIdle();

  // The surface should have correctly reported the window bounds.
  EXPECT_EQ(window_bounds_2, bounds_2);
}

// Test that the client is informed of fullscreen changes.
IN_PROC_BROWSER_TEST_F(LiveCaptionSurfaceTest, Fullscreen) {
  // Load a pre-prepared page that defines JS to (un/)fullscreen the tab.
  MockSurfaceClient client;
  content::WebContents* web_contents =
      LoadNewTab(embedded_test_server()->GetURL("/media/fullscreen.html"));
  NewSurfaceForWebContents(web_contents, &client);

  // We use a mock function to set up "checkpoints" by which each side effect
  // should have occurred.
  testing::MockFunction<void(int)> checkpointer;

  {
    testing::InSequence seq;

    // Fullscreen media on the page.
    EXPECT_CALL(client, OnFullscreenToggled());
    EXPECT_CALL(checkpointer, Call(1));
    {
      ScopedFullscreenRunner runner(web_contents);
      EXPECT_TRUE(
          content::ExecJs(web_contents, "makeFullscreen('small_video')"));
    }
    checkpointer.Call(1);

    // Un-fullscreen the media.
    EXPECT_CALL(client, OnFullscreenToggled());
    EXPECT_CALL(checkpointer, Call(2));
    {
      ScopedFullscreenRunner runner(web_contents);
      EXPECT_TRUE(content::ExecJs(web_contents, "exitFullscreen()"));
    }
    checkpointer.Call(2);
  }
}

// Test that the surface correctly reports session IDs.
IN_PROC_BROWSER_TEST_F(LiveCaptionSurfaceTest, SessionIds) {
  // Create two tabs with two surfaces.
  MockSurfaceClient client_1, client_2;
  LiveCaptionSurface* surface_1 =
      NewSurfaceForWebContents(LoadNewTab(kAboutBlankUrl), &client_1);
  LiveCaptionSurface* surface_2 =
      NewSurfaceForWebContents(LoadNewTab(kAboutBlankUrl), &client_2);

  // The session IDs of the two surfaces should be different because they
  // represent different web contents.
  const base::UnguessableToken init_id_1 = surface_1->session_id();
  EXPECT_NE(init_id_1, surface_2->session_id());

  // Navigating a tab shouldn't change its session ID.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kAboutBlankUrl));
  EXPECT_EQ(init_id_1, surface_1->session_id());
}

// Test that a surface reports the end of live caption sessions.
IN_PROC_BROWSER_TEST_F(LiveCaptionSurfaceTest, Sessions) {
  // Create two tabs with surfaces attached.
  MockSurfaceClient client_1, client_2;
  content::WebContents* wc_1 = LoadNewTab(kAboutBlankUrl);
  content::WebContents* wc_2 = LoadNewTab(kAboutBlankUrl);
  LiveCaptionSurface* surface_1 = NewSurfaceForWebContents(wc_1, &client_1);
  NewSurfaceForWebContents(wc_2, &client_2);

  // We use a mock function to set up "checkpoints" by which each side effect
  // should have occurred.
  testing::MockFunction<void(int)> checkpointer;

  {
    testing::InSequence seq;

    // We will start by making a navigation in the focused tab. We expect this
    // should end its session, but not the other tab's.

    EXPECT_CALL(client_1, OnSessionEnded()).Times(0);
    EXPECT_CALL(client_2, OnSessionEnded());
    EXPECT_CALL(checkpointer, Call(1));

    ASSERT_EQ(wc_2, browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kAboutBlankUrl));
    base::RunLoop().RunUntilIdle();
    checkpointer.Call(1);

    // Next we will refresh the first tab. We expect this should end its
    // session.

    EXPECT_CALL(client_1, OnSessionEnded());
    EXPECT_CALL(checkpointer, Call(2));

    surface_1->Activate();
    ASSERT_EQ(wc_1, browser()->tab_strip_model()->GetActiveWebContents());

    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    content::WaitForLoadStop(wc_1);
    base::RunLoop().RunUntilIdle();
    checkpointer.Call(2);

    // Lastly, we will navigate from the first tab. This should end the session
    // again.

    EXPECT_CALL(client_1, OnSessionEnded());
    EXPECT_CALL(checkpointer, Call(3));

    // Navigating from the first tab should end its session again.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kAboutBlankUrl));
    base::RunLoop().RunUntilIdle();
    checkpointer.Call(3);
  }
}

}  // namespace
}  // namespace captions
