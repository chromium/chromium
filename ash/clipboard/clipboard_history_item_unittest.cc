// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_item.h"

#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

using ClipboardHistoryItemTest = AshTestBase;

TEST_F(ClipboardHistoryItemTest, GetImageDataUrl) {
  using DisplayFormat = ClipboardHistoryItem::DisplayFormat;

  constexpr const auto* kDataUrlStart = "data:image/png;base64,";

  for (size_t i = 0; i <= static_cast<size_t>(DisplayFormat::kMaxValue); ++i) {
    ClipboardHistoryItemBuilder builder;
    const auto display_format = static_cast<DisplayFormat>(i);
    switch (display_format) {
      case DisplayFormat::kText:
        builder.SetFormat(ui::ClipboardInternalFormat::kText);
        break;
      case DisplayFormat::kPng:
        builder.SetFormat(ui::ClipboardInternalFormat::kPng);
        break;
      case DisplayFormat::kHtml:
        builder.SetFormat(ui::ClipboardInternalFormat::kHtml);
        break;
      case DisplayFormat::kFile:
        builder.SetFormat(ui::ClipboardInternalFormat::kFilenames);
        break;
    }
    const auto item = builder.Build();
    EXPECT_EQ(item.display_format(), display_format);

    const auto maybe_image_data_url = item.GetImageDataUrl();
    if (display_format == DisplayFormat::kText) {
      EXPECT_FALSE(maybe_image_data_url);
    } else {
      ASSERT_TRUE(maybe_image_data_url);
      EXPECT_TRUE(base::StartsWith(*maybe_image_data_url, kDataUrlStart));
    }
  }
}

TEST_F(ClipboardHistoryItemTest, DisplayText) {
  base::test::ScopedRestoreICUDefaultLocale locale("en_US");

  // Populate a builder with all the data formats that we expect to handle.
  ClipboardHistoryItemBuilder builder;
  builder.SetText("Text")
      .SetMarkup("HTML with no image or table tags")
      .SetRtf("Rtf")
      .SetFilenames({ui::FileInfo(base::FilePath("/path/to/File.txt"),
                                  base::FilePath("File.txt")),
                     ui::FileInfo(base::FilePath("/path/to/Other%20File.txt"),
                                  base::FilePath("Other%20File.txt"))})
      .SetBookmarkTitle("Bookmark Title")
      .SetPng(gfx::test::CreatePNGBytes(10))
      .SetFileSystemData({u"/path/to/File.txt", u"/path/to/Other%20File.txt"})
      .SetWebSmartPaste(true);

  // PNG data always takes precedence. When we must show text rather than the
  // image itself, we display the PNG placeholder text.
  EXPECT_EQ(builder.Build().display_text(),
            l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_IMAGE));

  builder.ClearPng();

  // In the absence of PNG data, HTML data takes precedence, but we use the
  // plain-text format for the label.
  EXPECT_EQ(builder.Build().display_text(), u"Text");

  builder.ClearText();

  // If plain text does not exist, we show a placeholder label.
  EXPECT_EQ(builder.Build().display_text(),
            l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_HTML));

  builder.SetText("Text");

  builder.ClearMarkup();

  // In the absence of HTML data, text data takes precedence.
  EXPECT_EQ(builder.Build().display_text(), u"Text");

  builder.ClearText();

  // In the absence of text data, RTF data takes precedence.
  EXPECT_EQ(builder.Build().display_text(),
            l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_RTF_CONTENT));

  builder.ClearRtf();

  // In the absence of RTF data, filename data takes precedence.
  EXPECT_EQ(builder.Build().display_text(), u"File.txt, Other File.txt");

  builder.ClearFilenames();

  // In the absence of filename data, bookmark data takes precedence.
  EXPECT_EQ(builder.Build().display_text(), u"Bookmark Title");

  builder.ClearBookmarkTitle();

  // In the absence of bookmark data, web smart paste data takes precedence.
  EXPECT_EQ(builder.Build().display_text(),
            l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_WEB_SMART_PASTE));

  builder.ClearWebSmartPaste();

  // In the absence of web smart paste data, file system data takes precedence.
  // NOTE: File system data is the only kind of custom data currently supported.
  EXPECT_EQ(builder.Build().display_text(), u"File.txt, Other File.txt");
}

}  // namespace ash
