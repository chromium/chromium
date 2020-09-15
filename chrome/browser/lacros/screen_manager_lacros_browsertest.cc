// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/bitmap.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const char* kLacrosPageTitleHTML =
    "<html><head><title>Title Of Lacros Browser Test</title></head>"
    "<body>This page has a title.</body></html>";

}  // namespace

class ScreenManagerLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  ScreenManagerLacrosBrowserTest() = default;

  ScreenManagerLacrosBrowserTest(const ScreenManagerLacrosBrowserTest&) =
      delete;
  ScreenManagerLacrosBrowserTest& operator=(
      const ScreenManagerLacrosBrowserTest&) = delete;

  ~ScreenManagerLacrosBrowserTest() override = default;

  void BindScreenManager() {
    mojo::PendingRemote<crosapi::mojom::ScreenManager> pending_screen_manager;
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver =
        pending_screen_manager.InitWithNewPipeAndPassReceiver();
    auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
    ASSERT_TRUE(lacros_chrome_service);
    lacros_chrome_service->BindScreenManagerReceiver(
        std::move(pending_receiver));
    screen_manager_.Bind(std::move(pending_screen_manager));
  }

  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager_;
};

// Tests that taking a screen snapshot via crosapi works.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, TakeScreenSnapshot) {
  BindScreenManager();
  crosapi::Bitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    screen_manager_->TakeScreenSnapshot(&snapshot);
  }
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height, 0u);
  EXPECT_GT(snapshot.width, 0u);
  EXPECT_GT(snapshot.pixels.size(), 0u);
}

// Tests that taking a screen snapshot via crosapi works.
// This test makes the browser load a page with specific title, and then scans
// through a list of windows to look for the window with the expected title.
// This test cannot simply asserts exactly 1 window is present because currently
// in lacros_chrome_browsertests, different browser tests share the same
// ash-chrome, so a window could come from any one of them.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, TakeWindowSnapshot) {
  GURL url(std::string("data:text/html,") + kLacrosPageTitleHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  BindScreenManager();
  base::RunLoop run_loop;
  bool found_window = false;
  uint64_t window_id;
  auto look_for_window = base::BindRepeating(
      [](mojo::Remote<crosapi::mojom::ScreenManager>* screen_manager,
         base::RunLoop* run_loop, bool* found_window, uint64_t* window_id) {
        mojo::ScopedAllowSyncCallForTesting allow_sync_call;
        const base::string16 tab_title(
            base::ASCIIToUTF16("Title Of Lacros Browser Test"));
        std::string expected_window_title = l10n_util::GetStringFUTF8(
            IDS_BROWSER_WINDOW_TITLE_FORMAT, tab_title);

        std::vector<crosapi::mojom::WindowDetailsPtr> windows;
        (*screen_manager)->ListWindows(&windows);
        for (auto& window_details : windows) {
          if (window_details->title == expected_window_title) {
            (*found_window) = true;
            (*window_id) = window_details->id;
            run_loop->Quit();
            break;
          }
        }
      },
      &screen_manager_, &run_loop, &found_window, &window_id);
  // When the browser test start, there is no guaranteen that the window is
  // open from ash's perspective.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              std::move(look_for_window));
  run_loop.Run();
  ASSERT_TRUE(found_window);

  bool success = false;
  crosapi::Bitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    screen_manager_->TakeWindowSnapshot(window_id, &success, &snapshot);
  }
  ASSERT_TRUE(success);
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height, 0u);
  EXPECT_GT(snapshot.width, 0u);
  EXPECT_GT(snapshot.pixels.size(), 0u);
}
