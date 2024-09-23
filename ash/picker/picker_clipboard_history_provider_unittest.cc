// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_clipboard_history_provider.h"

#include <optional>
#include <utility>
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/file_info.h"
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

class PickerClipboardHistoryProviderTest : public views::ViewsTestBase {};

TEST_F(PickerClipboardHistoryProviderTest, FetchesTextResult) {
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
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
                  expected_item_id, PickerClipboardResult::DisplayFormat::kText,
                  0, u"xyz", std::nullopt, true))));
}

TEST_F(PickerClipboardHistoryProviderTest, FetchesImageResult) {
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
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(
      future.Get(),
      ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
          expected_item_id, PickerClipboardResult::DisplayFormat::kImage, 0, _,
          expected_display_image, true))));
}

TEST_F(PickerClipboardHistoryProviderTest, FetchesSingleFileResult) {
  base::UnguessableToken expected_item_id;
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            ClipboardHistoryItem item =
                builder
                    .SetFilenames({ui::FileInfo(base::FilePath("/dir/filename"),
                                                base::FilePath("filename"))})
                    .Build();
            expected_item_id = item.id();
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
                  expected_item_id, PickerClipboardResult::DisplayFormat::kFile,
                  1, u"filename", std::nullopt, true))));
}

TEST_F(PickerClipboardHistoryProviderTest, FetchesMultipleFileResults) {
  base::UnguessableToken expected_item_id;
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            ClipboardHistoryItem item =
                builder
                    .SetFilenames(
                        {ui::FileInfo(base::FilePath("/dir/filename1"),
                                      base::FilePath("filename1")),
                         {ui::FileInfo(base::FilePath("/dir/filename2"),
                                       base::FilePath("filename2"))}})
                    .Build();
            expected_item_id = item.id();
            std::move(callback).Run({item});
          });

  base::SimpleTestClock clock;
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
                  expected_item_id, PickerClipboardResult::DisplayFormat::kFile,
                  2, u"2 files", std::nullopt, true))));
}

TEST_F(PickerClipboardHistoryProviderTest, SetsIsRecentFieldFalse) {
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
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());
  clock.Advance(base::Hours(1));

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"");

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
                  expected_item_id, PickerClipboardResult::DisplayFormat::kText,
                  0, u"xyz", std::nullopt, false))));
}

TEST_F(PickerClipboardHistoryProviderTest, FiletersResultByQuery) {
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
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"123");

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<PickerClipboardResult>(
                  FieldsAre(_, PickerClipboardResult::DisplayFormat::kText, 0,
                            u"12345", std::nullopt, true))));
}

TEST_F(PickerClipboardHistoryProviderTest, FiltersOutHtmlResults) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce([](ClipboardHistoryController::GetHistoryValuesCallback
                       callback) {
        ClipboardHistoryItemBuilder builder;
        std::move(callback).Run(
            {builder.SetFormat(ui::ClipboardInternalFormat::kHtml).Build()});
      });

  base::SimpleTestClock clock;
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"123");

  EXPECT_THAT(future.Get(), IsEmpty());
}

TEST_F(PickerClipboardHistoryProviderTest, FiltersOutLongResults) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            std::move(callback).Run(
                {builder.SetFormat(ui::ClipboardInternalFormat::kText)
                     .SetText(std::string(10001, 'a'))
                     .Build()});
          });

  base::SimpleTestClock clock;
  PickerClipboardHistoryProvider provider(&clock);
  clock.SetNow(base::Time::Now());

  base::test::TestFuture<std::vector<PickerSearchResult>> future;
  provider.FetchResults(future.GetCallback(), /*query=*/u"a");

  EXPECT_THAT(future.Get(), IsEmpty());
}

}  // namespace
}  // namespace ash
