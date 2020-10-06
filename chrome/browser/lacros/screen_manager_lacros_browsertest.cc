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

const char* kLacrosPageTitle = "Title Of Lacros Browser Test";
const char* kLacrosPageTitleHTML =
    "<html><head><title>Title Of Lacros Browser Test</title></head>"
    "<body>This page has a title.</body></html>";

using ListWindowsCallback = base::RepeatingCallback<void(
    std::vector<crosapi::mojom::SnapshotSourcePtr>*)>;

// Used to find the window corresponding to the test page.
bool FindTestWindow(ListWindowsCallback list_windows, uint64_t* window_id) {
  base::RunLoop run_loop;
  bool found_window = false;
  auto look_for_window = base::BindRepeating(
      [](ListWindowsCallback list_windows, base::RunLoop* run_loop,
         bool* found_window, uint64_t* window_id) {
        const base::string16 tab_title(base::ASCIIToUTF16(kLacrosPageTitle));
        std::string expected_window_title = l10n_util::GetStringFUTF8(
            IDS_BROWSER_WINDOW_TITLE_FORMAT, tab_title);
        std::vector<crosapi::mojom::SnapshotSourcePtr> windows;
        list_windows.Run(&windows);
        for (auto& window : windows) {
          if (window->title == expected_window_title) {
            (*found_window) = true;
            (*window_id) = window->id;
            run_loop->Quit();
            break;
          }
        }
      },
      std::move(list_windows), &run_loop, &found_window, window_id);

  // When the browser test start, there is no guarantee that the window is
  // open from ash's perspective.
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1),
              std::move(look_for_window));
  run_loop.Run();

  return found_window;
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

  void BindScreenManager() {
    auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
    ASSERT_TRUE(lacros_chrome_service);

    mojo::PendingRemote<crosapi::mojom::ScreenManager> pending_screen_manager;
    lacros_chrome_service->BindScreenManagerReceiver(
        pending_screen_manager.InitWithNewPipeAndPassReceiver());

    screen_manager_.Bind(std::move(pending_screen_manager));
  }

  uint32_t QueryScreenManagerVersion() {
    // Synchronously fetch the version of the ScreenManager interface. The
    // version field will be available as |screen_manager_.version()|.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    screen_manager_.QueryVersion(
        base::BindOnce([](base::OnceClosure done_closure,
                          uint32_t version) { std::move(done_closure).Run(); },
                       run_loop.QuitClosure()));
    run_loop.Run();
    return screen_manager_.version();
  }

  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager_;
};

// Tests that taking a screen snapshot via crosapi works.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest,
                       DeprecatedTakeScreenSnapshot) {
  BindScreenManager();
  crosapi::Bitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    screen_manager_->DeprecatedTakeScreenSnapshot(&snapshot);
  }
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height, 0u);
  EXPECT_GT(snapshot.width, 0u);
  EXPECT_GT(snapshot.pixels.size(), 0u);
}

// Tests that taking a screen snapshot via crosapi works.
// This test makes the browser load a page with specific title, and then scans
// through a list of windows to look for the window with the expected title.
// This test cannot simply assert exactly 1 window is present because currently
// in lacros_chrome_browsertests, different browser tests share the same
// ash-chrome, so a window could come from any one of them.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest,
                       DeprecatedTakeWindowSnapshot) {
  GURL url(std::string("data:text/html,") + kLacrosPageTitleHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  BindScreenManager();

  auto list_windows = base::BindRepeating(
      [](mojo::Remote<crosapi::mojom::ScreenManager>* screen_manager,
         std::vector<crosapi::mojom::SnapshotSourcePtr>* windows) {
        mojo::ScopedAllowSyncCallForTesting allow_sync_call;
        (*screen_manager)->DeprecatedListWindows(windows);
      },
      &screen_manager_);

  uint64_t window_id;
  bool found_window = FindTestWindow(std::move(list_windows), &window_id);
  ASSERT_TRUE(found_window);

  bool success = false;
  crosapi::Bitmap snapshot;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    screen_manager_->DeprecatedTakeWindowSnapshot(window_id, &success,
                                                  &snapshot);
  }
  ASSERT_TRUE(success);
  // Verify the snapshot is non-empty.
  EXPECT_GT(snapshot.height, 0u);
  EXPECT_GT(snapshot.width, 0u);
  EXPECT_GT(snapshot.pixels.size(), 0u);
}

// Tests that taking a screen snapshot via crosapi works.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, ScreenCapturer) {
  BindScreenManager();

  if (QueryScreenManagerVersion() < 1) {
    LOG(WARNING) << "Ash version does not support required method.";
    return;
  }

  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  screen_manager_->GetScreenCapturer(capturer.BindNewPipeAndPassReceiver());

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
  BindScreenManager();

  if (QueryScreenManagerVersion() < 1) {
    LOG(WARNING) << "Ash version does not support required method.";
    return;
  }

  GURL url(std::string("data:text/html,") + kLacrosPageTitleHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  screen_manager_->GetWindowCapturer(capturer.BindNewPipeAndPassReceiver());

  auto list_windows = base::BindRepeating(
      [](mojo::Remote<crosapi::mojom::SnapshotCapturer>* capturer,
         std::vector<crosapi::mojom::SnapshotSourcePtr>* windows) {
        mojo::ScopedAllowSyncCallForTesting allow_sync_call;
        (*capturer)->ListSources(windows);
      },
      &capturer);

  uint64_t window_id;
  bool found_window = FindTestWindow(std::move(list_windows), &window_id);

  ASSERT_TRUE(found_window);

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
