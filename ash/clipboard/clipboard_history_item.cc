// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_item.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

crosapi::mojom::ClipboardHistoryDisplayFormat CalculateDisplayFormat(
    const ClipboardHistoryItem& item) {
  switch (item.main_format()) {
    case ui::ClipboardInternalFormat::kPng:
      return crosapi::mojom::ClipboardHistoryDisplayFormat::kPng;
    case ui::ClipboardInternalFormat::kHtml:
      if (!base::Contains(item.data().markup_data(), "<img") &&
          !base::Contains(item.data().markup_data(), "<table")) {
        return crosapi::mojom::ClipboardHistoryDisplayFormat::kText;
      }
      return crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml;
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
      return crosapi::mojom::ClipboardHistoryDisplayFormat::kText;
    case ui::ClipboardInternalFormat::kFilenames:
      return crosapi::mojom::ClipboardHistoryDisplayFormat::kFile;
    case ui::ClipboardInternalFormat::kCustom:
      return clipboard_history_util::ContainsFileSystemData(item.data())
                 ? crosapi::mojom::ClipboardHistoryDisplayFormat::kFile
                 : crosapi::mojom::ClipboardHistoryDisplayFormat::kText;
  }
}

std::optional<ui::ImageModel> DetermineDisplayImage(
    const ClipboardHistoryItem& item) {
  std::optional<ui::ImageModel> maybe_image;
  switch (item.display_format()) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng: {
      gfx::Image image;
      if (const auto& maybe_png = item.data().maybe_png()) {
        image = gfx::Image::CreateFrom1xPNGBytes(maybe_png.value());
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
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
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
  std::vector<std::u16string_view> source_list;
  clipboard_history_util::GetSplitFileSystemData(data, &source_list, &sources);
  CHECK(!sources.empty());

  size_t file_count = source_list.size();
  if (chromeos::features::IsClipboardHistoryRefreshEnabled() &&
      file_count > 1u) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_ASH_CLIPBOARD_HISTORY_FILE_COUNT, file_count);
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

std::optional<gfx::ElideBehavior> DetermineDisplayTextElideBehavior(
    const ClipboardHistoryItem& item) {
  return chromeos::features::IsClipboardHistoryRefreshEnabled() &&
                 chromeos::clipboard_history::IsUrl(item.display_text())
             ? std::make_optional(gfx::ELIDE_MIDDLE)
             : std::nullopt;
}

std::optional<size_t> DetermineDisplayTextMaxLines(
    const ClipboardHistoryItem& item) {
  return chromeos::features::IsClipboardHistoryRefreshEnabled() &&
                 chromeos::clipboard_history::IsUrl(item.display_text())
             ? std::make_optional(1u)
             : std::nullopt;
}

std::optional<ui::ImageModel> DetermineIcon(const ClipboardHistoryItem& item) {
  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    return chromeos::clipboard_history::GetIconForDescriptor(
        clipboard_history_util::ItemToDescriptor(item));
  }

  if (item.display_format() !=
      crosapi::mojom::ClipboardHistoryDisplayFormat::kFile) {
    return std::nullopt;
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
      display_text_elide_behavior_(DetermineDisplayTextElideBehavior(*this)),
      display_text_max_lines_(DetermineDisplayTextMaxLines(*this)),
      file_count_(clipboard_history_util::GetCountOfCopiedFiles(data_)),
      icon_(DetermineIcon(*this)) {}

ClipboardHistoryItem::ClipboardHistoryItem(const ClipboardHistoryItem& other)
    : id_(other.id_),
      data_(other.data_),
      time_copied_(other.time_copied_),
      main_format_(other.main_format_),
      display_format_(other.display_format_),
      display_image_(other.display_image_),
      display_text_(other.display_text_),
      display_text_elide_behavior_(other.display_text_elide_behavior_),
      display_text_max_lines_(other.display_text_max_lines_),
      file_count_(other.file_count_),
      icon_(other.icon_),
      secondary_display_text_(other.secondary_display_text_) {}

ClipboardHistoryItem::ClipboardHistoryItem(ClipboardHistoryItem&& other)
    : id_(std::move(other.id_)),
      data_(std::move(other.data_)),
      time_copied_(std::move(other.time_copied_)),
      main_format_(std::move(other.main_format_)),
      display_format_(std::move(other.display_format_)),
      display_image_(std::move(other.display_image_)),
      display_text_(std::move(other.display_text_)),
      display_text_elide_behavior_(
          std::move(other.display_text_elide_behavior_)),
      display_text_max_lines_(std::move(other.display_text_max_lines_)),
      file_count_(std::move(other.file_count_)),
      icon_(std::move(other.icon_)),
      secondary_display_text_(std::move(other.secondary_display_text_)) {}

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

void ClipboardHistoryItem::SetDisplayImage(
    const ui::ImageModel& display_image) {
  CHECK(display_image.IsImage());
  display_image_ = display_image;
  display_image_updated_callbacks_.Notify();
}

base::CallbackListSubscription
ClipboardHistoryItem::AddDisplayImageUpdatedCallback(
    base::RepeatingClosure callback) const {
  return display_image_updated_callbacks_.Add(std::move(callback));
}

}  // namespace ash
