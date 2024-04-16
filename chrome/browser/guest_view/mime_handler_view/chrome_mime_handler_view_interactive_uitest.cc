// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/common/constants.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"

using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;

namespace extensions {

class ChromeMimeHandlerViewInteractiveUITest : public ExtensionApiTest {
 public:
  ChromeMimeHandlerViewInteractiveUITest() = default;

  ~ChromeMimeHandlerViewInteractiveUITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  const Extension* LoadTestExtension() {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    EXPECT_EQ(std::string(extension_misc::kMimeHandlerPrivateTestExtensionId),
              extension->id());

    return extension;
  }

  void RunTestWithUrl(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());

    const Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    ResultCatcher catcher;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  void RunTest(const std::string& path) {
    RunTestWithUrl(embedded_test_server()->GetURL("/" + path));
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
};

// Test is flaky on Linux.  https://crbug.com/877627
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Fullscreen DISABLED_Fullscreen
#else
#define MAYBE_Fullscreen Fullscreen
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewInteractiveUITest,
                       MAYBE_Fullscreen) {
  RunTest("testFullscreen.csv");
}

namespace {

void WaitForFullscreenAnimation() {
#if BUILDFLAG(IS_MAC)
  const int delay_in_ms = 1500;
#else
  const int delay_in_ms = 100;
#endif
  // Wait for Mac OS fullscreen animation.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(delay_in_ms));
  run_loop.Run();
}

}  // namespace

// TODO(crbug.com/40714227): Flaky under Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_EscapeExitsFullscreen DISABLED_EscapeExitsFullscreen
#else
#define MAYBE_EscapeExitsFullscreen EscapeExitsFullscreen
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewInteractiveUITest,
                       MAYBE_EscapeExitsFullscreen) {
  // Use the testing subclass of MimeHandlerViewGuest.
  TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());

  const Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  ResultCatcher catcher;

  // Set observer to watch for fullscreen.
  ui_test_utils::FullscreenWaiter fullscreen_waiter(browser(),
                                                    {.tab_fullscreen = true});
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/testFullscreenEscape.csv")));

  // Make sure we have a guestviewmanager.
  auto* embedder_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);

  auto* guest_rwh = guest_view->GetGuestMainFrame()->GetRenderWidgetHost();

  // Wait for fullscreen mode.
  fullscreen_waiter.Wait();
  WaitForFullscreenAnimation();

  // Send a touch to focus the guest. We can't directly test that the correct
  // RenderWidgetHost got focus, but the wait seems to work.
  content::WaitForHitTestData(guest_view->GetGuestMainFrame());
  SimulateMouseClickAt(
      embedder_contents, 0, blink::WebMouseEvent::Button::kLeft,
      guest_rwh->GetView()->TransformPointToRootCoordSpace(gfx::Point(7, 7)));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return IsRenderWidgetHostFocused(guest_rwh); }));
  EXPECT_EQ(embedder_contents->GetFocusedFrame(),
            guest_view->GetGuestMainFrame());

  // Send <esc> to exit fullscreen.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  WaitForFullscreenAnimation();

  // Now wait for the test to succeed, or timeout.
  if (!catcher.GetNextResult())
    FAIL() << catcher.message();
}

}  // namespace extensions
