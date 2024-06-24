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

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::VariantWith;

class PickerClipboardProviderTest : public views::ViewsTestBase {};

TEST_F(PickerClipboardProviderTest, FetchesTextResult) {
  base::UnguessableToken expected_item_id;
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            ClipboardHistoryItem item =
                builder.SetFormat(ui::ClipboardInternalFormat::kText)
                    .SetText("xyz")
                    .Build();
            expected_item_id = item.id();
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
                      expected_item_id,
                      PickerSearchResult::ClipboardData::DisplayFormat::kText,
                      u"xyz", std::nullopt, true)))));
}

TEST_F(PickerClipboardProviderTest, FetchesImageResult) {
  base::UnguessableToken expected_item_id;
  ui::ImageModel expected_display_image =
      ui::ImageModel::FromImage(gfx::test::CreateImage(16, 16));
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            ClipboardHistoryItem item =
                builder.SetFormat(ui::ClipboardInternalFormat::kPng)
                    .SetPng({1, 2, 3})
                    .Build();
            expected_item_id = item.id();
            item.SetDisplayImage(expected_display_image);
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
                      expected_item_id,
                      PickerSearchResult::ClipboardData::DisplayFormat::kImage,
                      _, expected_display_image, true)))));
}

TEST_F(PickerClipboardProviderTest, SetsIsRecentFieldFalse) {
  base::UnguessableToken expected_item_id;
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            ClipboardHistoryItem item =
                builder.SetFormat(ui::ClipboardInternalFormat::kText)
                    .SetText("xyz")
                    .Build();
            expected_item_id = item.id();
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(&clock);
  clock.SetNow(base::Time::Now());
  clock.Advance(base::Hours(1));

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"");

  EXPECT_THAT(future.Get(),
              ElementsAre(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
                      expected_item_id,
                      PickerSearchResult::ClipboardData::DisplayFormat::kText,
                      u"xyz", std::nullopt, false)))));
}

TEST_F(PickerClipboardProviderTest, FiletersResultByQuery) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            std::move(callback).Run(
                {builder.SetFormat(ui::ClipboardInternalFormat::kText)
                     .SetText("xyz")
                     .Build(),
                 builder.SetFormat(ui::ClipboardInternalFormat::kText)
                     .SetText("12345")
                     .Build()});
          });

  base::SimpleTestClock clock;
  PickerClipboardProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"123");

  EXPECT_THAT(
      future.Get(),
      ElementsAre(Property(
          "data", &PickerSearchResult::data,
          VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
              _, PickerSearchResult::ClipboardData::DisplayFormat::kText,
              u"12345", std::nullopt, true)))));
}
}  // namespace
}  // namespace ash
