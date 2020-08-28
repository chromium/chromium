// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/test_support/clipboard_history_item_builder.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

ClipboardHistoryItemBuilder::ClipboardHistoryItemBuilder() = default;

ClipboardHistoryItemBuilder::~ClipboardHistoryItemBuilder() = default;

ClipboardHistoryItem ClipboardHistoryItemBuilder::Build() const {
  ui::ClipboardData data;
  if (text_.has_value())
    data.set_text(text_.value());
  if (markup_.has_value())
    data.set_markup_data(markup_.value());
  if (rtf_.has_value())
    data.SetRTFData(rtf_.value());
  if (bookmark_title_.has_value())
    data.set_bookmark_title(bookmark_title_.value());
  if (bitmap_.has_value())
    data.SetBitmapData(bitmap_.value());
  if (custom_format_.has_value() && custom_data_.has_value())
    data.SetCustomData(custom_format_.value(), custom_data_.value());
  if (web_smart_paste_.has_value())
    data.set_web_smart_paste(web_smart_paste_.value());
  return ClipboardHistoryItem(std::move(data));
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::Clear() {
  text_ = base::nullopt;
  markup_ = base::nullopt;
  rtf_ = base::nullopt;
  bookmark_title_ = base::nullopt;
  bitmap_ = base::nullopt;
  custom_format_ = base::nullopt;
  custom_data_ = base::nullopt;
  web_smart_paste_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetFormat(
    ui::ClipboardInternalFormat format) {
  switch (format) {
    case ui::ClipboardInternalFormat::kText:
      return SetText("Text");
    case ui::ClipboardInternalFormat::kHtml:
      return SetMarkup("Markup");
    case ui::ClipboardInternalFormat::kSvg:
      return SetMarkup("Svg");
    case ui::ClipboardInternalFormat::kRtf:
      return SetRtf("Rtf");
    case ui::ClipboardInternalFormat::kBookmark:
      return SetBookmarkTitle("Bookmark Title");
    case ui::ClipboardInternalFormat::kBitmap:
      return SetBitmap(gfx::test::CreateBitmap(10, 10));
    case ui::ClipboardInternalFormat::kCustom:
      return SetCustomData("Custom Format", "Custom Data");
    case ui::ClipboardInternalFormat::kWeb:
      return SetWebSmartPaste(true);
  }
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearFormat(
    ui::ClipboardInternalFormat format) {
  switch (format) {
    case ui::ClipboardInternalFormat::kText:
      return ClearText();
    case ui::ClipboardInternalFormat::kHtml:
      return ClearMarkup();
    case ui::ClipboardInternalFormat::kSvg:
      return ClearSvg();
    case ui::ClipboardInternalFormat::kRtf:
      return ClearRtf();
    case ui::ClipboardInternalFormat::kBookmark:
      return ClearBookmarkTitle();
    case ui::ClipboardInternalFormat::kBitmap:
      return ClearBitmap();
    case ui::ClipboardInternalFormat::kCustom:
      return ClearCustomData();
    case ui::ClipboardInternalFormat::kWeb:
      return ClearWebSmartPaste();
  }
  NOTREACHED();
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetText(
    const std::string& text) {
  text_ = text;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearText() {
  text_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetMarkup(
    const std::string& markup) {
  markup_ = markup;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearMarkup() {
  markup_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetSvg(
    const std::string& svg) {
  svg_ = svg;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearSvg() {
  svg_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetRtf(
    const std::string& rtf) {
  rtf_ = rtf;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearRtf() {
  rtf_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetBookmarkTitle(
    const std::string& bookmark_title) {
  bookmark_title_ = bookmark_title;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearBookmarkTitle() {
  bookmark_title_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetBitmap(
    const SkBitmap& bitmap) {
  bitmap_ = bitmap;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearBitmap() {
  bitmap_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetCustomData(
    const std::string& custom_format,
    const std::string& custom_data) {
  custom_format_ = custom_format;
  custom_data_ = custom_data;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearCustomData() {
  custom_format_ = base::nullopt;
  custom_data_ = base::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetFileSystemData(
    const std::initializer_list<std::string>& source_list) {
  constexpr char kFileSystemSourcesType[] = "fs/sources";

  base::Pickle custom_data;
  ui::WriteCustomDataToPickle(
      std::unordered_map<base::string16, base::string16>(
          {{base::UTF8ToUTF16(kFileSystemSourcesType),
            base::UTF8ToUTF16(base::JoinString(source_list, "\n"))}}),
      &custom_data);

  return SetCustomData(
      ui::ClipboardFormatType::GetWebCustomDataType().GetName(),
      std::string(static_cast<const char*>(custom_data.data()),
                  custom_data.size()));
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetWebSmartPaste(
    bool web_smart_paste) {
  web_smart_paste_ = web_smart_paste;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearWebSmartPaste() {
  web_smart_paste_ = base::nullopt;
  return *this;
}

}  // namespace ash
