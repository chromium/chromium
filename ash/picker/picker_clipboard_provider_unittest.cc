// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_provider.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

class PickerClipboardProviderTest : public views::ViewsTestBase {};

TEST_F(PickerClipboardProviderTest, FetchesRecentTextResult) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            builder.SetFormat(ui::ClipboardInternalFormat::kText);
            builder.SetText("xyz");
            std::move(callback).Run({builder.Build()});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(base::DoNothing(), &clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::unique_ptr<PickerListItemView>> future;
  provider.FetchResult(future.GetRepeatingCallback());

  EXPECT_EQ(future.Get()->GetPrimaryTextForTesting(), u"xyz");
  EXPECT_TRUE(future.Get()->GetPrimaryImageForTesting().IsEmpty());
}

TEST_F(PickerClipboardProviderTest, FetchesRecentImageResult) {
  ui::ImageModel expected_display_image =
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16));
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            builder.SetFormat(ui::ClipboardInternalFormat::kPng);
            builder.SetPng(std::vector<uint8_t>({1, 2, 3}));
            auto item = builder.Build();
            item.SetDisplayImage(expected_display_image);
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(base::DoNothing(), &clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::unique_ptr<PickerListItemView>> future;
  provider.FetchResult(future.GetRepeatingCallback());

  EXPECT_EQ(future.Get()->GetPrimaryTextForTesting(), u"");
  EXPECT_EQ(future.Get()->GetPrimaryImageForTesting(), expected_display_image);
}

TEST_F(PickerClipboardProviderTest, DoesNotFetchOldResult) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            builder.SetFormat(ui::ClipboardInternalFormat::kText);
            builder.SetText("xyz");
            std::move(callback).Run({builder.Build()});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(base::DoNothing(), &clock);
  clock.SetNow(base::Time::Now());
  clock.Advance(base::Hours(1));

  base::test::TestFuture<std::unique_ptr<PickerListItemView>> future;
  provider.FetchResult(future.GetRepeatingCallback());

  EXPECT_FALSE(future.IsReady());
}
}  // namespace
}  // namespace ash
