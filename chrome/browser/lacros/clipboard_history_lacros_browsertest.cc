// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

using ClipboardHistoryBrowserTest = InProcessBrowserTest;

// Verifies that the Lacros render view context menu clipboard history option is
// enabled when and only when there are clipboard item(s) to show.
IN_PROC_BROWSER_TEST_F(ClipboardHistoryBrowserTest, MenuOptionEnabled) {
  // If the clipboard history interface is not available on this version of
  // ash-chrome, this test cannot meaningfully run.
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::ClipboardHistory>()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanPaste;

  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  // When clipboard history is empty, the Clipboard option should be disabled.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CLIPBOARD_HISTORY_MENU));
  EXPECT_FALSE(menu.IsItemEnabled(IDC_CONTENT_CLIPBOARD_HISTORY_MENU));

  // Populate the clipboard so that the menu can be shown.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(u"text");

  // When clipboard history is not empty, the Clipboard option should be
  // enabled.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CLIPBOARD_HISTORY_MENU));
  EXPECT_TRUE(menu.IsItemEnabled(IDC_CONTENT_CLIPBOARD_HISTORY_MENU));
}
