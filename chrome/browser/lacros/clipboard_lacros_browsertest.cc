// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

class ClipboardLacrosBrowserTest : public InProcessBrowserTest {
 protected:
  ClipboardLacrosBrowserTest() = default;

  ClipboardLacrosBrowserTest(const ClipboardLacrosBrowserTest&) = delete;
  ClipboardLacrosBrowserTest& operator=(const ClipboardLacrosBrowserTest&) =
      delete;

  void WaitForClipboardText(const std::string& text) {
    base::test::TestFuture<std::string> text_found_future;
    auto look_for_clipboard_text = base::BindRepeating(
        [](base::test::TestFuture<std::string>* text_found_future,
           std::string text) {
          auto* lacros_chrome_service = chromeos::LacrosService::Get();
          std::string read_text = "";
          {
            mojo::ScopedAllowSyncCallForTesting allow_sync_call;
            lacros_chrome_service->GetRemote<crosapi::mojom::Clipboard>()
                ->GetCopyPasteText(&read_text);
          }
          if (read_text == text) {
            text_found_future->SetValue(read_text);
          }
        },
        &text_found_future, text);
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(1),
                std::move(look_for_clipboard_text));
    ASSERT_TRUE(text_found_future.Wait()) << "Clipboard text match not found.";
  }

  ~ClipboardLacrosBrowserTest() override = default;
};

// Tests that accessing the text of the copy-paste clipboard succeeds.
// TODO(https://crbug.com/1157314): This test is not safe to run in parallel
// with other clipboard tests since there's a single exo clipboard.
IN_PROC_BROWSER_TEST_F(ClipboardLacrosBrowserTest, GetCopyPasteText) {
  auto* lacros_chrome_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_chrome_service);

  if (!lacros_chrome_service->IsAvailable<crosapi::mojom::Clipboard>())
    return;

  aura::Window* window = BrowserView::GetBrowserViewForBrowser(browser())
                             ->frame()
                             ->GetNativeWindow();
  std::string id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(id));
  ASSERT_TRUE(
      browser_test_util::SendAndWaitForMouseClick(window->GetRootWindow()));

  // Write some clipboard text and read it back.
  std::string write_text =
      base::StringPrintf("clipboard text %lu", base::RandUint64());
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(base::UTF8ToUTF16(write_text));
  }

  WaitForClipboardText(write_text);
}
