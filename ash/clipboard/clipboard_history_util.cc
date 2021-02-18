// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"

namespace ash {
namespace ClipboardHistoryUtil {

namespace {

constexpr char kFileSystemSourcesType[] = "fs/sources";

// The array of formats in order of decreasing priority.
constexpr ui::ClipboardInternalFormat kPrioritizedFormats[] = {
    ui::ClipboardInternalFormat::kBitmap,
    ui::ClipboardInternalFormat::kHtml,
    ui::ClipboardInternalFormat::kText,
    ui::ClipboardInternalFormat::kRtf,
    ui::ClipboardInternalFormat::kFilenames,
    ui::ClipboardInternalFormat::kBookmark,
    ui::ClipboardInternalFormat::kWeb,
    ui::ClipboardInternalFormat::kCustom};

}  // namespace

base::Optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data) {
  for (const auto& format : kPrioritizedFormats) {
    if (ContainsFormat(data, format)) {
      return format;
    }
  }
  return base::nullopt;
}

ClipboardHistoryDisplayFormat CalculateDisplayFormat(
    const ui::ClipboardData& data) {
  switch (CalculateMainFormat(data).value()) {
    case ui::ClipboardInternalFormat::kBitmap:
      return ClipboardHistoryDisplayFormat::kBitmap;
    case ui::ClipboardInternalFormat::kHtml:
      if ((data.markup_data().find("<img") == std::string::npos) &&
          (data.markup_data().find("<table") == std::string::npos)) {
        return ClipboardHistoryDisplayFormat::kText;
      }
      return ClipboardHistoryDisplayFormat::kHtml;
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kFilenames:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
      return ClipboardHistoryDisplayFormat::kText;
    case ui::ClipboardInternalFormat::kCustom:
      return ContainsFileSystemData(data)
                 ? ClipboardHistoryDisplayFormat::kFile
                 : ClipboardHistoryDisplayFormat::kText;
  }
}

bool ContainsFormat(const ui::ClipboardData& data,
                    ui::ClipboardInternalFormat format) {
  return data.format() & static_cast<int>(format);
}

void RecordClipboardHistoryItemDeleted(const ClipboardHistoryItem& item) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted",
      CalculateDisplayFormat(item.data()));
}

void RecordClipboardHistoryItemPasted(const ClipboardHistoryItem& item) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted",
      CalculateDisplayFormat(item.data()));
}

bool ContainsFileSystemData(const ui::ClipboardData& data) {
  return !GetFileSystemSources(data).empty();
}

void GetSplitFileSystemData(const ui::ClipboardData& data,
                            std::vector<base::StringPiece16>* source_list,
                            base::string16* sources) {
  DCHECK(sources);
  DCHECK(sources->empty());
  DCHECK(source_list);
  DCHECK(source_list->empty());

  *sources = GetFileSystemSources(data);
  if (sources->empty()) {
    // Not a file system data.
    return;
  }

  // Split sources into a list.
  *source_list =
      base::SplitStringPiece(*sources, base::UTF8ToUTF16("\n"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

size_t GetCountOfCopiedFiles(const ui::ClipboardData& data) {
  base::string16 sources;
  std::vector<base::StringPiece16> source_list;
  GetSplitFileSystemData(data, &source_list, &sources);

  if (sources.empty()) {
    // Not a file system data.
    return 0;
  }

  return source_list.size();
}

base::string16 GetFileSystemSources(const ui::ClipboardData& data) {
  if (!ContainsFormat(data, ui::ClipboardInternalFormat::kCustom))
    return base::string16();

  // Attempt to read file system sources in the custom data.
  base::string16 sources;
  ui::ReadCustomDataForType(
      data.custom_data_data().c_str(), data.custom_data_data().size(),
      base::UTF8ToUTF16(kFileSystemSourcesType), &sources);

  return sources;
}

bool IsSupported(const ui::ClipboardData& data) {
  const base::Optional<ui::ClipboardInternalFormat> format =
      CalculateMainFormat(data);

  // Empty `data` is not supported.
  if (!format.has_value())
    return false;

  // The only supported type of custom data is file system data.
  if (format.value() == ui::ClipboardInternalFormat::kCustom)
    return ContainsFileSystemData(data);

  return true;
}

bool IsEnabledInCurrentMode() {
  const auto* session_controller = Shell::Get()->session_controller();

  // The clipboard history menu is enabled only when a user has logged in and
  // login UI is hidden.
  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return false;
  }

  switch (session_controller->login_status()) {
    case LoginStatus::NOT_LOGGED_IN:
    case LoginStatus::LOCKED:
    case LoginStatus::KIOSK_APP:
    case LoginStatus::PUBLIC:
      return false;
    case LoginStatus::USER:
    case LoginStatus::GUEST:
    case LoginStatus::CHILD:
      return true;
  }
}

}  // namespace ClipboardHistoryUtil
}  // namespace ash
