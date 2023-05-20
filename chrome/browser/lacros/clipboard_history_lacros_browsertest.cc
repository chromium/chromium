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

class ClipboardHistoryRefreshLacrosTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          /*enable_clipboard_history_refresh=*/bool> {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    std::vector<std::string> enabled_features{"ClipboardHistoryRefresh",
                                              "Jelly"};
    std::vector<std::string> disabled_features;
    if (!GetParam()) {
      std::swap(enabled_features, disabled_features);
    }
    StartUniqueAshChrome(enabled_features, disabled_features,
                         /*additional_cmdline_switches=*/{},
                         /*bug_number_and_reason=*/
                         {"b/267681869 Switch to shared ash when clipboard "
                          "history refresh is enabled by default"});

    InProcessBrowserTest::SetUp();
  }

  // Returns whether the clipboard history interface is available. It may not be
  // available on earlier versions of Ash Chrome.
  bool IsInterfaceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::ClipboardHistory>();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryRefreshLacrosTest,
                         /*enable_clipboard_history_refresh=*/testing::Bool());

// Verifies that the Lacros render view context menu clipboard history option is
// enabled when and only when there are clipboard item(s) to show.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshLacrosTest, MenuOptionEnabled) {
  // If the clipboard history interface is not available on this version of
  // ash-chrome, this test cannot meaningfully run.
  if (!IsInterfaceAvailable()) {
    return;
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
