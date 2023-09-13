// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {
const char* kLacrosPageTitleFormat = "Title Of Lacros Browser Test %lu";
const char* kLacrosPageTitleHTMLFormat =
    "<html><head><title>%s</title></head>"
    "<body>This page has a title.</body></html>";

mojo::Remote<crosapi::mojom::SnapshotCapturer> GetWindowCapturer() {
  auto* lacros_chrome_service = chromeos::LacrosService::Get();
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  lacros_chrome_service->GetRemote<crosapi::mojom::ScreenManager>()
      ->GetWindowCapturer(capturer.BindNewPipeAndPassReceiver());

  return capturer;
}

// Used to find the window corresponding to the test page.
uint64_t WaitForWindow(std::string title) {
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer = GetWindowCapturer();

  base::RunLoop run_loop;
  uint64_t window_id;
  auto look_for_window = base::BindRepeating(
      [](mojo::Remote<crosapi::mojom::SnapshotCapturer>* capturer,
         base::RunLoop* run_loop, uint64_t* window_id, std::string tab_title) {
        std::vector<crosapi::mojom::SnapshotSourcePtr> windows;
        {
          mojo::ScopedAllowSyncCallForTesting allow_sync_call;
          (*capturer)->ListSources(&windows);
        }
        for (auto& window : windows) {
          if (window->title == tab_title) {
            if (window_id)
              (*window_id) = window->id;
            run_loop->Quit();
            break;
          }
        }
      },
      &capturer, &run_loop, &window_id, std::move(title));

  // When the browser test start, there is no guarantee that the window is
  // open from ash's perspective.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(1), std::move(look_for_window));
  run_loop.Run();

  return window_id;
}

// |browser| is a browser instance that will be navigated to a fixed URL.
// Returns the window ID from the snapshot crosapi.
uint64_t WaitForLacrosToBeAvailableInAsh(Browser* browser) {
  // Generate a random window title so that multiple lacros_chrome_browsertests
  // can run at the same time without confusing windows.
  std::string title =
      base::StringPrintf(kLacrosPageTitleFormat, base::RandUint64());
  std::string html =
      base::StringPrintf(kLacrosPageTitleHTMLFormat, title.c_str());
  GURL url(std::string("data:text/html,") + html);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

  return WaitForWindow(std::move(title));
}

}  // namespace

class ScreenManagerLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  ScreenManagerLacrosBrowserTest() = default;

  ScreenManagerLacrosBrowserTest(const ScreenManagerLacrosBrowserTest&) =
      delete;
  ScreenManagerLacrosBrowserTest& operator=(
      const ScreenManagerLacrosBrowserTest&) = delete;

  ~ScreenManagerLacrosBrowserTest() override = default;
};

// Tests that taking a screen snapshot via crosapi works.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, ScreenCapturer) {
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  auto* lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::ScreenManager>()->GetScreenCapturer(
      capturer.BindNewPipeAndPassReceiver());

  std::vector<crosapi::mojom::SnapshotSourcePtr> screens;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    capturer->ListSources(&screens);
  }
  ASSERT_LE(1u, screens.size());

  bool success = false;
  SkBitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    capturer->TakeSnapshot(screens[0]->id, &success, &snapshot);
  }
  EXPECT_TRUE(success);
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height(), 0);
  EXPECT_GT(snapshot.width(), 0);
}

// Tests that taking a window snapshot via crosapi works.
// This test makes the browser load a page with specific title, and then scans
// through a list of windows to look for the window with the expected title.
// This test cannot simply asserts exactly 1 window is present because currently
// in lacros_chrome_browsertests, different browser tests share the same
// ash-chrome, so a window could come from any one of them.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, WindowCapturer) {
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  auto* lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::ScreenManager>()->GetWindowCapturer(
      capturer.BindNewPipeAndPassReceiver());

  uint64_t window_id = WaitForLacrosToBeAvailableInAsh(browser());

  bool success = false;
  SkBitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    capturer->TakeSnapshot(window_id, &success, &snapshot);
  }
  ASSERT_TRUE(success);
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height(), 0);
  EXPECT_GT(snapshot.width(), 0);
}

IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, AppWindowCapturer) {
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  auto* lacros_service = chromeos::LacrosService::Get();
  lacros_service->GetRemote<crosapi::mojom::ScreenManager>()->GetWindowCapturer(
      capturer.BindNewPipeAndPassReceiver());

  Browser* app_browser =
      CreateBrowserForApp("test_app_name", browser()->profile());
  uint64_t window_id = WaitForLacrosToBeAvailableInAsh(app_browser);

  bool success = false;
  SkBitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    capturer->TakeSnapshot(window_id, &success, &snapshot);
  }
  ASSERT_TRUE(success);
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height(), 0);
  EXPECT_GT(snapshot.width(), 0);
}
