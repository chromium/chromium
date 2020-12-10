// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_test_util.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
const char* kLacrosPageTitleFormat = "Title Of Lacros Browser Test %lu";
const char* kLacrosPageTitleHTMLFormat =
    "<html><head><title>%s</title></head>"
    "<body>This page has a title.</body></html>";

mojo::Remote<crosapi::mojom::SnapshotCapturer> GetWindowCapturer() {
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();

  mojo::PendingRemote<crosapi::mojom::ScreenManager> pending_screen_manager;
  lacros_chrome_service->BindScreenManagerReceiver(
      pending_screen_manager.InitWithNewPipeAndPassReceiver());

  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager;
  screen_manager.Bind(std::move(pending_screen_manager));

  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  screen_manager->GetWindowCapturer(capturer.BindNewPipeAndPassReceiver());

  return capturer;
}

// Used to find the window corresponding to the test page.
uint64_t WaitForWindow(std::string title) {
  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer = GetWindowCapturer();

  base::RunLoop run_loop;
  uint64_t window_id;
  base::string16 tab_title(base::ASCIIToUTF16(title));
  auto look_for_window = base::BindRepeating(
      [](mojo::Remote<crosapi::mojom::SnapshotCapturer>* capturer,
         base::RunLoop* run_loop, uint64_t* window_id,
         base::string16 tab_title) {
        std::string expected_window_title = l10n_util::GetStringFUTF8(
            IDS_BROWSER_WINDOW_TITLE_FORMAT, tab_title);
        std::vector<crosapi::mojom::SnapshotSourcePtr> windows;
        {
          mojo::ScopedAllowSyncCallForTesting allow_sync_call;
          (*capturer)->ListSources(&windows);
        }
        for (auto& window : windows) {
          if (window->title == expected_window_title) {
            if (window_id)
              (*window_id) = window->id;
            run_loop->Quit();
            break;
          }
        }
      },
      &capturer, &run_loop, &window_id, std::move(tab_title));

  // When the browser test start, there is no guarantee that the window is
  // open from ash's perspective.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              std::move(look_for_window));
  run_loop.Run();

  return window_id;
}

}  // namespace

uint64_t WaitForLacrosToBeAvailableInAsh(Browser* browser) {
  // Generate a random window title so that multiple lacros_chrome_browsertests
  // can run at the same time without confusing windows.
  std::string title =
      base::StringPrintf(kLacrosPageTitleFormat, base::RandUint64());
  std::string html =
      base::StringPrintf(kLacrosPageTitleHTMLFormat, title.c_str());
  GURL url(std::string("data:text/html,") + html);
  ui_test_utils::NavigateToURL(browser, url);

  return WaitForWindow(std::move(title));
}
