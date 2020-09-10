// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>
#include <deque>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace ClipboardHistoryUtil {

namespace {

constexpr std::array<ui::ClipboardInternalFormat, 7> kAllFormats = {
    ui::ClipboardInternalFormat::kBitmap,
    ui::ClipboardInternalFormat::kHtml,
    ui::ClipboardInternalFormat::kText,
    ui::ClipboardInternalFormat::kRtf,
    ui::ClipboardInternalFormat::kBookmark,
    ui::ClipboardInternalFormat::kCustom,
    ui::ClipboardInternalFormat::kWeb};

// Helpers ---------------------------------------------------------------------

// Sets hardcoded data for all formats on `builder`.
void SetAllFormats(ClipboardHistoryItemBuilder* builder) {
  for (const auto& format : kAllFormats)
    builder->SetFormat(format);
}

}  // namespace

// Tests -----------------------------------------------------------------------

using ClipboardHistoryUtilTest = testing::Test;

TEST_F(ClipboardHistoryUtilTest, CalculateMainFormat) {
  ClipboardHistoryItemBuilder builder;
  SetAllFormats(&builder);

  // We will cycle through all formats in prioritized order.
  std::deque<ui::ClipboardInternalFormat> prioritized_formats = {
      ui::ClipboardInternalFormat::kBitmap,
      ui::ClipboardInternalFormat::kHtml,
      ui::ClipboardInternalFormat::kText,
      ui::ClipboardInternalFormat::kRtf,
      ui::ClipboardInternalFormat::kBookmark,
      ui::ClipboardInternalFormat::kWeb,
      ui::ClipboardInternalFormat::kCustom,
  };

  while (!prioritized_formats.empty()) {
    ui::ClipboardInternalFormat format = prioritized_formats.front();
    EXPECT_EQ(CalculateMainFormat(builder.Build().data()), format);

    // Pop a format and resume testing.
    builder.ClearFormat(format);
    prioritized_formats.pop_front();
  }

  EXPECT_FALSE(CalculateMainFormat(builder.Build().data()).has_value());
}

TEST_F(ClipboardHistoryUtilTest, ContainsFormat) {
  ClipboardHistoryItemBuilder builder;

  for (const auto& format : kAllFormats) {
    EXPECT_FALSE(ContainsFormat(builder.Build().data(), format));
    builder.SetFormat(format);
    EXPECT_TRUE(ContainsFormat(builder.Build().data(), format));
  }
}

TEST_F(ClipboardHistoryUtilTest, ContainsFileSystemData) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_FALSE(ContainsFileSystemData(builder.Build().data()));

  SetAllFormats(&builder);
  EXPECT_FALSE(ContainsFileSystemData(builder.Build().data()));

  builder.SetFileSystemData({"/path/to/My%20File.txt"});
  EXPECT_TRUE(ContainsFileSystemData(builder.Build().data()));
}

TEST_F(ClipboardHistoryUtilTest, GetFileSystemSources) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_TRUE(GetFileSystemSources(builder.Build().data()).empty());

  SetAllFormats(&builder);
  EXPECT_TRUE(GetFileSystemSources(builder.Build().data()).empty());

  builder.SetFileSystemData({"/path/to/My%20File.txt"});
  EXPECT_EQ(GetFileSystemSources(builder.Build().data()),
            base::UTF8ToUTF16("/path/to/My%20File.txt"));
}

TEST_F(ClipboardHistoryUtilTest, IsSupported) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_FALSE(IsSupported(builder.Build().data()));

  for (const auto& format : kAllFormats) {
    if (format != ui::ClipboardInternalFormat::kCustom) {
      builder.SetFormat(format);
      EXPECT_TRUE(IsSupported(builder.Build().data()));
      builder.Clear();
    }
  }

  builder.SetFormat(ui::ClipboardInternalFormat::kCustom);
  EXPECT_FALSE(IsSupported(builder.Build().data()));

  builder.SetFileSystemData({"/path/to/My%20File.txt"});
  EXPECT_TRUE(IsSupported(builder.Build().data()));
}

}  // namespace ClipboardHistoryUtil
}  // namespace ash
