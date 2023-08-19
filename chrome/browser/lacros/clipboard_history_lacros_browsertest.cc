// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lacros/clipboard_history_lacros.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/widget/widget.h"

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

  void WriteTextToClipboard(const std::u16string& text) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste).WriteText(text);

    // TODO(http://b/278916298): implement testing in an end-to-end way after
    // http://b/283834862 is fixed.
    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      crosapi::ClipboardHistoryLacros* clipboard_history_lacros =
          crosapi::ClipboardHistoryLacros::Get();
      std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>&
          cached_descriptors = clipboard_history_lacros->cached_descriptors_;
      cached_descriptors.insert(
          cached_descriptors.begin(),
          crosapi::mojom::ClipboardHistoryItemDescriptor(
              /*id=*/base::UnguessableToken::Create(),
              /*display_format=*/
              crosapi::mojom::ClipboardHistoryDisplayFormat::kText, text,
              /*file_count=*/0));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryRefreshLacrosTest,
                         /*enable_clipboard_history_refresh=*/testing::Bool());

// Verifies that the Lacros render view context menu clipboard history option
// works as expected.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshLacrosTest,
                       MenuOptionOnRenderViewContextMenu) {
  // If the clipboard history interface is not available on this version of
  // ash-chrome, this test cannot meaningfully run.
  if (!IsInterfaceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanPaste;

  const int clipboard_history_command_id =
      chromeos::features::IsClipboardHistoryRefreshEnabled()
          ? IDC_CONTENT_PASTE_FROM_CLIPBOARD
          : IDC_CONTENT_CLIPBOARD_HISTORY_MENU;

  {
    TestRenderViewContextMenu menu(*browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame(),
                                   params);
    menu.Init();

    // When clipboard history is empty, the Clipboard option should be disabled.
    EXPECT_TRUE(menu.IsItemPresent(clipboard_history_command_id));
    EXPECT_FALSE(menu.IsItemEnabled(clipboard_history_command_id));
  }

  // Populate the clipboard so that the menu can be shown.
  WriteTextToClipboard(u"text");

  {
    TestRenderViewContextMenu menu(*browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetPrimaryMainFrame(),
                                   params);
    menu.Init();

    const ui::SimpleMenuModel& menu_model = menu.menu_model();
    absl::optional<size_t> target_command_index =
        menu_model.GetIndexOfCommandId(clipboard_history_command_id);
    ASSERT_TRUE(target_command_index);

    // The clipboard history menu option should be enabled since clipboard
    // history is non-empty.
    EXPECT_TRUE(menu_model.IsEnabledAt(*target_command_index));

    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      // Because the refresh feature is enabled, the clipboard history menu item
      // should be a submenu item.
      ui::MenuModel* const submenu_model =
          menu_model.GetSubmenuModelAt(*target_command_index);
      ASSERT_TRUE(submenu_model);
      ASSERT_EQ(submenu_model->GetItemCount(), 2u);
      EXPECT_EQ(submenu_model->GetLabelAt(0), u"text");
      EXPECT_EQ(submenu_model->GetLabelAt(1),
                l10n_util::GetStringUTF16(
                    IDS_CONTEXT_MENU_SHOW_CLIPBOARD_HISTORY_MENU));
    } else {
      // Because the refresh feature is disabled, the clipboard history menu
      // item should be a command item.
      EXPECT_EQ(menu_model.GetTypeAt(*target_command_index),
                ui::MenuModel::ItemType::TYPE_COMMAND);
    }
  }
}

// Checks that the Lacros text services context menu clipboard history option is
// 1. A command menu item if the clipboard history refresh feature is disabled;
// 2. A submenu item if the clipboard history refresh feature is enabled.
IN_PROC_BROWSER_TEST_P(ClipboardHistoryRefreshLacrosTest,
                       MenuOptionOnTextServicesContextMenu) {
  // If the clipboard history interface is not available on this version of
  // ash-chrome, this test cannot meaningfully run.
  if (!IsInterfaceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }

  // Create a textfield.
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(200, 200);
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  auto textfield_widget = std::make_unique<views::Widget>();
  textfield_widget->Init(std::move(params));
  views::Textfield* textfield =
      textfield_widget->SetContentsView(std::make_unique<views::Textfield>());
  textfield->SetAccessibleName(u"Textfield");
  textfield_widget->Show();

  views::TextfieldTestApi api(textfield);
  api.UpdateContextMenu();

  const int clipboard_history_command_id =
      chromeos::features::IsClipboardHistoryRefreshEnabled()
          ? IDS_APP_PASTE_FROM_CLIPBOARD
          : IDS_APP_SHOW_CLIPBOARD_HISTORY;

  // Search the parent model and the command index of
  // `clipboard_history_command_id`.
  ui::MenuModel* target_command_parent_model = api.context_menu_contents();
  size_t target_command_index = 0u;
  ui::MenuModel::GetModelAndIndexForCommandId(clipboard_history_command_id,
                                              &target_command_parent_model,
                                              &target_command_index);
  ASSERT_EQ(target_command_parent_model, api.context_menu_contents());
  ASSERT_GT(target_command_index, 0u);

  // Before having clipboard history item descriptors, the clipboard history
  // menu option is disabled.
  EXPECT_FALSE(target_command_parent_model->IsEnabledAt(target_command_index));

  // Write some clipboard data then update the context menu.
  WriteTextToClipboard(u"b");
  WriteTextToClipboard(u"a");
  api.UpdateContextMenu();
  target_command_parent_model = api.context_menu_contents();

  // The clipboard history menu option becomes enabled after writing data.
  EXPECT_TRUE(target_command_parent_model->IsEnabledAt(target_command_index));

  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    // Because the refresh feature is enabled, the clipboard history menu item
    // should be a submenu item.
    EXPECT_EQ(target_command_parent_model->GetTypeAt(target_command_index),
              ui::MenuModel::ItemType::TYPE_SUBMENU);

    // Check the submenu model data.
    ui::MenuModel* const submenu_model =
        target_command_parent_model->GetSubmenuModelAt(target_command_index);
    ASSERT_TRUE(submenu_model);
    ASSERT_EQ(submenu_model->GetItemCount(), 3u);
    EXPECT_EQ(submenu_model->GetLabelAt(0), u"a");
    EXPECT_EQ(submenu_model->GetLabelAt(1), u"b");
    EXPECT_EQ(submenu_model->GetLabelAt(2),
              l10n_util::GetStringUTF16(IDS_APP_SHOW_CLIPBOARD_HISTORY));
  } else {
    // Because the refresh feature is disabled, the clipboard history menu item
    // should be a command item.
    EXPECT_EQ(target_command_parent_model->GetTypeAt(target_command_index),
              ui::MenuModel::ItemType::TYPE_COMMAND);
  }
}
