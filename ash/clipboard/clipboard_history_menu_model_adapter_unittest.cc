// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/repeating_test_future.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {
using crosapi::mojom::ClipboardHistoryControllerShowSource;

namespace {

ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
  return Shell::Get()->clipboard_history_controller();
}

std::vector<ClipboardHistoryControllerShowSource>
GetClipboardHistoryShowSources() {
  std::vector<ClipboardHistoryControllerShowSource> sources;
  for (int i =
           static_cast<int>(ClipboardHistoryControllerShowSource::kMinValue);
       i <= static_cast<int>(ClipboardHistoryControllerShowSource::kMaxValue);
       ++i) {
    sources.push_back(static_cast<ClipboardHistoryControllerShowSource>(i));
  }
  return sources;
}

}  // namespace

// Base class for `ClipboardHistoryMenuModelAdapter` tests that run with each
// possible `ClipboardHistoryControllerShowSource`.
class ClipboardHistoryMenuModelAdapterShowSourceTest
    : public AshTestBase,
      public testing::WithParamInterface<ClipboardHistoryControllerShowSource> {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetClipboardHistoryController()->set_confirmed_operation_callback_for_test(
        operation_confirmed_future_.GetCallback());
  }

  void WriteTextToClipboardAndConfirm(const std::u16string& str) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  ClipboardHistoryControllerShowSource GetSource() const { return GetParam(); }

 private:
  base::test::RepeatingTestFuture<bool> operation_confirmed_future_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryMenuModelAdapterShowSourceTest,
                         testing::ValuesIn(GetClipboardHistoryShowSources()));

// Verifies that the clipboard history menu has an educational footer iff it was
// shown by the Ctrl+V long-press shortcut.
TEST_P(ClipboardHistoryMenuModelAdapterShowSourceTest,
       ControlVLongpressShowsFooter) {
  // Write items to clipboard history so that the menu can show.
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");

  // Show the clipboard history menu.
  auto* controller = GetClipboardHistoryController();
  EXPECT_TRUE(controller->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_TRUE(controller->IsMenuShowing());

  // Verify that iff the menu was shown via Ctrl+V long-press, the menu has an
  // educational footer item; otherwise, the number of items in the menu should
  // match the number of items in clipboard history.
  const auto* model = controller->context_menu_for_test()->GetModelForTest();
  EXPECT_EQ(controller->history()->GetItems().size(), 2u);
  if (GetSource() == ClipboardHistoryControllerShowSource::kControlVLongpress) {
    ASSERT_EQ(model->GetItemCount(), 3u);
    EXPECT_EQ(model->GetTypeAt(2u), ui::MenuModel::ItemType::TYPE_TITLE);
  } else {
    EXPECT_EQ(model->GetItemCount(), 2u);
  }
}

}  // namespace ash
