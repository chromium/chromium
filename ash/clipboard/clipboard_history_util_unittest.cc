// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>
#include <deque>
#include <string_view>

#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"

namespace ash::clipboard_history_util {

namespace {

constexpr std::array<ui::ClipboardInternalFormat, 8> kAllFormats = {
    ui::ClipboardInternalFormat::kPng,
    ui::ClipboardInternalFormat::kHtml,
    ui::ClipboardInternalFormat::kText,
    ui::ClipboardInternalFormat::kRtf,
    ui::ClipboardInternalFormat::kBookmark,
    ui::ClipboardInternalFormat::kCustom,
    ui::ClipboardInternalFormat::kWeb,
    ui::ClipboardInternalFormat::kFilenames};

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
      ui::ClipboardInternalFormat::kPng,
      ui::ClipboardInternalFormat::kHtml,
      ui::ClipboardInternalFormat::kText,
      ui::ClipboardInternalFormat::kRtf,
      ui::ClipboardInternalFormat::kFilenames,
      ui::ClipboardInternalFormat::kBookmark,
      ui::ClipboardInternalFormat::kWeb,
      ui::ClipboardInternalFormat::kCustom,
  };

  while (!prioritized_formats.empty()) {
    ui::ClipboardInternalFormat format = prioritized_formats.front();
    EXPECT_EQ(CalculateMainFormat(builder.BuildData()), format);

    // Pop a format and resume testing.
    builder.ClearFormat(format);
    prioritized_formats.pop_front();
  }

  EXPECT_FALSE(CalculateMainFormat(builder.BuildData()).has_value());
}

TEST_F(ClipboardHistoryUtilTest, ContainsFormat) {
  ClipboardHistoryItemBuilder builder;

  for (const auto& format : kAllFormats) {
    EXPECT_FALSE(ContainsFormat(builder.BuildData(), format));
    builder.SetFormat(format);
    EXPECT_TRUE(ContainsFormat(builder.BuildData(), format));
  }
}

TEST_F(ClipboardHistoryUtilTest, ContainsFileSystemData) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_FALSE(ContainsFileSystemData(builder.BuildData()));

  SetAllFormats(&builder);
  builder.ClearFormat(ui::ClipboardInternalFormat::kFilenames);
  EXPECT_FALSE(ContainsFileSystemData(builder.BuildData()));

  // Outside the Files app, file system sources are written to filenames.
  builder.SetFormat(ui::ClipboardInternalFormat::kFilenames);
  EXPECT_TRUE(ContainsFileSystemData(builder.BuildData()));
  builder.ClearFormat(ui::ClipboardInternalFormat::kFilenames);

  // Within the Files app, file system sources are written to custom data.
  builder.SetFileSystemData({u"/path/to/My%20File.txt"});
  EXPECT_TRUE(ContainsFileSystemData(builder.BuildData()));
}

TEST_F(ClipboardHistoryUtilTest, GetFileSystemSources) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_TRUE(GetFileSystemSources(builder.BuildData()).empty());

  SetAllFormats(&builder);
  builder.ClearFormat(ui::ClipboardInternalFormat::kFilenames);
  EXPECT_TRUE(GetFileSystemSources(builder.BuildData()).empty());

  // Outside the Files app, file system sources are written to filenames.
  builder.SetFilenames({ui::FileInfo(base::FilePath("/path/to/My%20File.txt"),
                                     base::FilePath("My%20File.txt"))});
  EXPECT_EQ(GetFileSystemSources(builder.BuildData()),
            u"/path/to/My%20File.txt");
  builder.ClearFilenames();

  // Within the Files app, file system sources are written to custom data.
  builder.SetFileSystemData({u"/path/to/My%20File.txt"});
  EXPECT_EQ(GetFileSystemSources(builder.BuildData()),
            u"/path/to/My%20File.txt");
}

TEST_F(ClipboardHistoryUtilTest, GetSplitFileSystemData) {
  const std::string file_name1("File1.txt"), file_name2("File2.txt");
  const std::u16string file_name1_16(base::UTF8ToUTF16(file_name1)),
      file_name2_16(base::UTF8ToUTF16(file_name2));

  ClipboardHistoryItemBuilder builder;
  std::u16string sources;
  std::vector<std::u16string_view> source_list;

  // Outside the Files app, file system sources are written to filenames.
  builder.SetFilenames(
      {ui::FileInfo(base::FilePath(file_name1), base::FilePath(file_name1)),
       ui::FileInfo(base::FilePath(file_name2), base::FilePath(file_name2))});
  GetSplitFileSystemData(builder.BuildData(), &source_list, &sources);
  EXPECT_EQ(file_name1_16, source_list[0]);
  EXPECT_EQ(file_name2_16, source_list[1]);
  builder.ClearFilenames();

  sources.clear();
  source_list.clear();

  // Within the Files app, file system sources are written to custom data.
  builder.SetFileSystemData({file_name1_16, file_name2_16});
  GetSplitFileSystemData(builder.BuildData(), &source_list, &sources);
  EXPECT_EQ(file_name1_16, source_list[0]);
  EXPECT_EQ(file_name2_16, source_list[1]);
}

TEST_F(ClipboardHistoryUtilTest, GetFilesCount) {
  ClipboardHistoryItemBuilder builder;

  // Outside the Files app, file system sources are written to filenames.
  builder.SetFilenames({ui::FileInfo(base::FilePath("/path/to/My%20File1.txt"),
                                     base::FilePath("My%20File1.txt")),
                        ui::FileInfo(base::FilePath("/path/to/My%20File2.txt"),
                                     base::FilePath("My%20File2.txt"))});
  EXPECT_EQ(2u, GetCountOfCopiedFiles(builder.BuildData()));
  builder.ClearFilenames();

  // Within the Files app, file system sources are written to custom data.
  builder.SetFileSystemData(
      {u"/path/to/My%20File1.txt", u"/path/to/My%20File2.txt"});
  EXPECT_EQ(2u, GetCountOfCopiedFiles(builder.BuildData()));
}

TEST_F(ClipboardHistoryUtilTest, IsSupported) {
  ClipboardHistoryItemBuilder builder;

  EXPECT_FALSE(IsSupported(builder.BuildData()));

  for (const auto& format : kAllFormats) {
    if (format != ui::ClipboardInternalFormat::kCustom) {
      builder.SetFormat(format);
      EXPECT_TRUE(IsSupported(builder.BuildData()));
      builder.Clear();
    }
  }

  builder.SetFormat(ui::ClipboardInternalFormat::kCustom);
  EXPECT_FALSE(IsSupported(builder.BuildData()));

  builder.SetFileSystemData({u"/path/to/My%20File.txt"});
  EXPECT_TRUE(IsSupported(builder.BuildData()));
}

}  // namespace ash::clipboard_history_util
