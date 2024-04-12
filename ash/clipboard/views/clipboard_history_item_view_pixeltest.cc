// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ClipboardHistoryItemViewPixelTest : public AshTestBase {
 protected:
  void WriteImageToClipboardAndConfirm(const SkBitmap& bitmap) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteImage(bitmap);
    }
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

 private:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()
        ->clipboard_history_controller()
        ->set_confirmed_operation_callback_for_test(
            operation_confirmed_future_.GetRepeatingCallback());
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  base::test::TestFuture<bool> operation_confirmed_future_;
};

TEST_F(ClipboardHistoryItemViewPixelTest, BitmapItemView) {
  // Write an image to clipboard then show the clipboard history menu.
  WriteImageToClipboardAndConfirm(
      gfx::test::CreateBitmap(/*width=*/100, /*height=*/100));
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);

  views::Widget* const menu_host_widget =
      FindWidgetWithNameAndWaitIfNeeded("MenuHost");
  ASSERT_TRUE(menu_host_widget);
  views::View* const bitmap_contents =
      menu_host_widget->GetContentsView()->GetViewByID(
          clipboard_history_util::kBitmapItemView);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bitmap_item_view",
      /*revision_number=*/0, bitmap_contents));

  // Press the tab key to focus on the delete button.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_TAB, ui::EF_NONE);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "bitmap_item_view_with_delete_button",
      /*revision_number=*/0, bitmap_contents));
}

}  // namespace ash
