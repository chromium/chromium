// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include <vector>

#include "ash/clipboard/clipboard_history_item.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

ClipboardHistoryItemBuilder::ClipboardHistoryItemBuilder() = default;

ClipboardHistoryItemBuilder::~ClipboardHistoryItemBuilder() = default;

ClipboardHistoryItem ClipboardHistoryItemBuilder::Build() const {
  return ClipboardHistoryItem(BuildData());
}

ui::ClipboardData ClipboardHistoryItemBuilder::BuildData() const {
  ui::ClipboardData data;
  if (text_.has_value())
    data.set_text(text_.value());
  if (markup_.has_value())
    data.set_markup_data(markup_.value());
  if (rtf_.has_value())
    data.SetRTFData(rtf_.value());
  if (!filenames_.empty())
    data.set_filenames(filenames_);
  if (bookmark_title_.has_value())
    data.set_bookmark_title(bookmark_title_.value());
  if (png_.has_value())
    data.SetPngData(png_.value());
  if (custom_format_.has_value() && custom_data_.has_value())
    data.SetCustomData(*custom_format_, custom_data_.value());
  if (web_smart_paste_.has_value())
    data.set_web_smart_paste(web_smart_paste_.value());
  return data;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::Clear() {
  text_ = std::nullopt;
  markup_ = std::nullopt;
  rtf_ = std::nullopt;
  filenames_.clear();
  bookmark_title_ = std::nullopt;
  png_ = std::nullopt;
  custom_format_ = std::nullopt;
  custom_data_ = std::nullopt;
  web_smart_paste_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetFormat(
    ui::ClipboardInternalFormat format) {
  switch (format) {
    case ui::ClipboardInternalFormat::kText:
      return SetText("Text");
    case ui::ClipboardInternalFormat::kHtml:
      return SetMarkup("Markup with an <img> tag");
    case ui::ClipboardInternalFormat::kSvg:
      return SetMarkup("Svg");
    case ui::ClipboardInternalFormat::kRtf:
      return SetRtf("Rtf");
    case ui::ClipboardInternalFormat::kFilenames:
      return SetFilenames({ui::FileInfo(base::FilePath("/dir/filename"),
                                        base::FilePath("filename"))});
    case ui::ClipboardInternalFormat::kBookmark:
      return SetBookmarkTitle("Bookmark Title");
    case ui::ClipboardInternalFormat::kPng:
      return SetPng(gfx::test::CreatePNGBytes(10));
    case ui::ClipboardInternalFormat::kCustom:
      return SetCustomData(
          ui::ClipboardFormatType::Deserialize("Custom Format"), "Custom Data");
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
    case ui::ClipboardInternalFormat::kFilenames:
      return ClearFilenames();
    case ui::ClipboardInternalFormat::kBookmark:
      return ClearBookmarkTitle();
    case ui::ClipboardInternalFormat::kPng:
      return ClearPng();
    case ui::ClipboardInternalFormat::kCustom:
      return ClearCustomData();
    case ui::ClipboardInternalFormat::kWeb:
      return ClearWebSmartPaste();
  }
  NOTREACHED();
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetText(
    const std::string& text) {
  text_ = text;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearText() {
  text_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetMarkup(
    const std::string& markup) {
  markup_ = markup;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearMarkup() {
  markup_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetSvg(
    const std::string& svg) {
  svg_ = svg;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearSvg() {
  svg_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetRtf(
    const std::string& rtf) {
  rtf_ = rtf;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearRtf() {
  rtf_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetFilenames(
    std::vector<ui::FileInfo> filenames) {
  filenames_ = std::move(filenames);
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearFilenames() {
  filenames_.clear();
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetBookmarkTitle(
    const std::string& bookmark_title) {
  bookmark_title_ = bookmark_title;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearBookmarkTitle() {
  bookmark_title_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetPng(
    const scoped_refptr<base::RefCountedMemory>& png) {
  std::vector<uint8_t> data(png->data(), png->data() + png->size());
  return SetPng(std::move(data));
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetPng(
    std::vector<uint8_t> png) {
  png_ = std::move(png);
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearPng() {
  png_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetCustomData(
    const ui::ClipboardFormatType& custom_format,
    const std::string& custom_data) {
  custom_format_ = custom_format;
  custom_data_ = custom_data;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearCustomData() {
  custom_format_ = std::nullopt;
  custom_data_ = std::nullopt;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetFileSystemData(
    const std::initializer_list<std::u16string>& source_list) {
  constexpr char16_t kFileSystemSourcesType[] = u"fs/sources";

  base::Pickle custom_data;
  ui::WriteCustomDataToPickle(
      std::unordered_map<std::u16string, std::u16string>(
          {{kFileSystemSourcesType, base::JoinString(source_list, u"\n")}}),
      &custom_data);

  return SetCustomData(
      ui::ClipboardFormatType::DataTransferCustomType(),
      std::string(custom_data.data_as_char(), custom_data.size()));
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::SetWebSmartPaste(
    bool web_smart_paste) {
  web_smart_paste_ = web_smart_paste;
  return *this;
}

ClipboardHistoryItemBuilder& ClipboardHistoryItemBuilder::ClearWebSmartPaste() {
  web_smart_paste_ = std::nullopt;
  return *this;
}

}  // namespace ash
