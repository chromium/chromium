// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager_;
};

// Tests that taking a screen snapshot via crosapi works.
IN_PROC_BROWSER_TEST_F(ScreenManagerLacrosBrowserTest, ScreenCapturer) {
  BindScreenManager();

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

  mojo::Remote<crosapi::mojom::SnapshotCapturer> capturer;
  screen_manager_->GetWindowCapturer(capturer.BindNewPipeAndPassReceiver());

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
