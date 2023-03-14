// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_item.h"

#include <vector>

#include "ash/clipboard/clipboard_history_util.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

ClipboardHistoryItem::DisplayFormat CalculateDisplayFormat(
    const ClipboardHistoryItem& item) {
  switch (item.main_format()) {
    case ui::ClipboardInternalFormat::kPng:
      return ClipboardHistoryItem::DisplayFormat::kPng;
    case ui::ClipboardInternalFormat::kHtml:
      if ((item.data().markup_data().find("<img") == std::string::npos) &&
          (item.data().markup_data().find("<table") == std::string::npos)) {
        return ClipboardHistoryItem::DisplayFormat::kText;
      }
      return ClipboardHistoryItem::DisplayFormat::kHtml;
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
      return ClipboardHistoryItem::DisplayFormat::kText;
    case ui::ClipboardInternalFormat::kFilenames:
      return ClipboardHistoryItem::DisplayFormat::kFile;
    case ui::ClipboardInternalFormat::kCustom:
      return clipboard_history_util::ContainsFileSystemData(item.data())
                 ? ClipboardHistoryItem::DisplayFormat::kFile
                 : ClipboardHistoryItem::DisplayFormat::kText;
  }
}

absl::optional<ui::ImageModel> DetermineDisplayImage(
    const ClipboardHistoryItem& item) {
  absl::optional<ui::ImageModel> maybe_image;
  switch (item.display_format()) {
    case ClipboardHistoryItem::DisplayFormat::kText:
    case ClipboardHistoryItem::DisplayFormat::kFile:
      break;
    case ClipboardHistoryItem::DisplayFormat::kPng: {
      gfx::Image image;
      if (const auto& maybe_png = item.data().maybe_png()) {
        image = gfx::Image::CreateFrom1xPNGBytes(maybe_png.value().data(),
                                                 maybe_png.value().size());
      } else {
        // If we have not yet encoded the bitmap to a PNG, just create the
        // image using the available bitmap. No information is lost here.
        auto maybe_bitmap = item.data().GetBitmapIfPngNotEncoded();
        DCHECK(maybe_bitmap.has_value());
        image = gfx::Image::CreateFrom1xBitmap(maybe_bitmap.value());
      }
      maybe_image = ui::ImageModel::FromImage(image);
      break;
    }
    case ClipboardHistoryItem::DisplayFormat::kHtml:
      // The `ClipboardHistoryResourceManager` will update this preview once an
      // image model is rendered.
      maybe_image = clipboard_history_util::GetHtmlPreviewPlaceholder();
      break;
  }
  return maybe_image;
}

// Returns the text to display for the file system data contained within `data`.
std::u16string DetermineDisplayTextForFileSystemData(
    const ui::ClipboardData& data) {
  // This code should not be reached if `data` doesn't contain file system data.
  std::u16string sources;
  std::vector<base::StringPiece16> source_list;
  clipboard_history_util::GetSplitFileSystemData(data, &source_list, &sources);
  if (sources.empty()) {
    NOTREACHED();
    return std::u16string();
  }

  // Strip path information, so all that's left are file names.
  for (auto& source : source_list) {
    source = source.substr(source.find_last_of(u"/") + 1);
  }

  // Join file names, unescaping encoded character sequences for display. This
  // ensures that "My%20File.txt" will display as "My File.txt".
  return base::UTF8ToUTF16(base::UnescapeURLComponent(
      base::UTF16ToUTF8(base::JoinString(source_list, u", ")),
      base::UnescapeRule::SPACES));
}

std::u16string DetermineDisplayText(const ClipboardHistoryItem& item) {
  switch (item.main_format()) {
    case ui::ClipboardInternalFormat::kPng:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_IMAGE);
    case ui::ClipboardInternalFormat::kText:
      return base::UTF8ToUTF16(item.data().text());
    case ui::ClipboardInternalFormat::kHtml:
      // Show plain text if it exists. Otherwise, show the placeholder.
      if (!item.data().text().empty()) {
        return base::UTF8ToUTF16(item.data().text());
      }

      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_HTML);
    case ui::ClipboardInternalFormat::kSvg:
      return base::UTF8ToUTF16(item.data().svg_data());
    case ui::ClipboardInternalFormat::kRtf:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_RTF_CONTENT);
    case ui::ClipboardInternalFormat::kBookmark:
      return base::UTF8ToUTF16(item.data().bookmark_title());
    case ui::ClipboardInternalFormat::kWeb:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_MENU_WEB_SMART_PASTE);
    case ui::ClipboardInternalFormat::kFilenames:
    case ui::ClipboardInternalFormat::kCustom:
      // Currently, the only supported type of custom data is file system data.
      return DetermineDisplayTextForFileSystemData(item.data());
  }
}

absl::optional<ui::ImageModel> DetermineIcon(const ClipboardHistoryItem& item) {
  if (item.display_format() != ClipboardHistoryItem::DisplayFormat::kFile) {
    return absl::nullopt;
  }

  return clipboard_history_util::GetIconForFileClipboardItem(item);
}

}  // namespace

ClipboardHistoryItem::ClipboardHistoryItem(ui::ClipboardData data)
    : id_(base::UnguessableToken::Create()),
      data_(std::move(data)),
      time_copied_(base::Time::Now()),
      main_format_(clipboard_history_util::CalculateMainFormat(data_).value()),
      display_format_(CalculateDisplayFormat(*this)),
      display_image_(DetermineDisplayImage(*this)),
      display_text_(DetermineDisplayText(*this)),
      icon_(DetermineIcon(*this)) {}

ClipboardHistoryItem::ClipboardHistoryItem(const ClipboardHistoryItem&) =
    default;

ClipboardHistoryItem::ClipboardHistoryItem(ClipboardHistoryItem&&) = default;

ClipboardHistoryItem::~ClipboardHistoryItem() = default;

ui::ClipboardData ClipboardHistoryItem::ReplaceEquivalentData(
    ui::ClipboardData&& new_data) {
  DCHECK(data_ == new_data);
  time_copied_ = base::Time::Now();
  // If work has already been done to encode an image belonging to both data
  // instances, make sure it is not lost.
  if (data_.maybe_png() && !new_data.maybe_png())
    new_data.SetPngDataAfterEncoding(*data_.maybe_png());
  return std::exchange(data_, std::move(new_data));
}

}  // namespace ash
