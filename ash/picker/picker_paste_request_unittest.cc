// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_paste_request.h"

#include <optional>
#include <string>

#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/events/event_constants.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using ::testing::_;

using PickerPasteRequestTest = views::ViewsTestBase;

TEST_F(PickerPasteRequestTest, DoesNotPasteWithoutNewFocus) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Show();
  views::test::WidgetVisibleWaiter(widget.get()).Wait();
  MockClipboardHistoryController clipboard_history_controller;
  auto item_id = base::UnguessableToken::Create();

  EXPECT_CALL(clipboard_history_controller, PasteClipboardItemById(_, _, _))
      .Times(0);

  PickerPasteRequest request(
      &clipboard_history_controller,
      aura::client::GetFocusClient(widget->GetNativeView()), item_id);
  widget->CloseNow();
}

TEST_F(PickerPasteRequestTest, PastesOnNewFocus) {
  auto old_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  old_widget->Show();
  views::test::WidgetVisibleWaiter(old_widget.get()).Wait();
  MockClipboardHistoryController clipboard_history_controller;
  auto item_id = base::UnguessableToken::Create();

  EXPECT_CALL(clipboard_history_controller,
              PasteClipboardItemById(
                  item_id.ToString(), ui::EF_NONE,
                  crosapi::mojom::ClipboardHistoryControllerShowSource::
                      kVirtualKeyboard))
      .Times(1);

  PickerPasteRequest request(
      &clipboard_history_controller,
      aura::client::GetFocusClient(old_widget->GetNativeView()), item_id);
  old_widget->CloseNow();
  auto new_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  new_widget->Show();
  views::test::WidgetVisibleWaiter(new_widget.get()).Wait();
}

TEST_F(PickerPasteRequestTest, DoesNotPasteAfterDestruction) {
  auto old_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  old_widget->Show();
  views::test::WidgetVisibleWaiter(old_widget.get()).Wait();
  MockClipboardHistoryController clipboard_history_controller;
  auto item_id = base::UnguessableToken::Create();

  EXPECT_CALL(clipboard_history_controller, PasteClipboardItemById(_, _, _))
      .Times(0);

  {
    PickerPasteRequest request(
        &clipboard_history_controller,
        aura::client::GetFocusClient(old_widget->GetNativeView()), item_id);
    old_widget->CloseNow();
  }
  auto new_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  new_widget->Show();
  views::test::WidgetVisibleWaiter(new_widget.get()).Wait();
}

}  // namespace
}  // namespace ash
