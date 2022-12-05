// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"

namespace ash::clipboard_history_util {

namespace {

constexpr char16_t kFileSystemSourcesType[] = u"fs/sources";

// The array of formats in order of decreasing priority.
constexpr ui::ClipboardInternalFormat kPrioritizedFormats[] = {
    ui::ClipboardInternalFormat::kPng,
    ui::ClipboardInternalFormat::kHtml,
    ui::ClipboardInternalFormat::kText,
    ui::ClipboardInternalFormat::kRtf,
    ui::ClipboardInternalFormat::kFilenames,
    ui::ClipboardInternalFormat::kBookmark,
    ui::ClipboardInternalFormat::kWeb,
    ui::ClipboardInternalFormat::kCustom};

}  // namespace

absl::optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data) {
  for (const auto& format : kPrioritizedFormats) {
    if (ContainsFormat(data, format)) {
      return format;
    }
  }
  return absl::nullopt;
}

DisplayFormat CalculateDisplayFormat(const ui::ClipboardData& data) {
  switch (CalculateMainFormat(data).value()) {
    case ui::ClipboardInternalFormat::kPng:
      return DisplayFormat::kPng;
    case ui::ClipboardInternalFormat::kHtml:
      if ((data.markup_data().find("<img") == std::string::npos) &&
          (data.markup_data().find("<table") == std::string::npos)) {
        return DisplayFormat::kText;
      }
      return DisplayFormat::kHtml;
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
      return DisplayFormat::kText;
    case ui::ClipboardInternalFormat::kFilenames:
      return DisplayFormat::kFile;
    case ui::ClipboardInternalFormat::kCustom:
      return ContainsFileSystemData(data) ? DisplayFormat::kFile
                                          : DisplayFormat::kText;
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
                            std::u16string* sources) {
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
  *source_list = base::SplitStringPiece(*sources, u"\n", base::TRIM_WHITESPACE,
                                        base::SPLIT_WANT_NONEMPTY);
}

size_t GetCountOfCopiedFiles(const ui::ClipboardData& data) {
  std::u16string sources;
  std::vector<base::StringPiece16> source_list;
  GetSplitFileSystemData(data, &source_list, &sources);

  if (sources.empty()) {
    // Not a file system data.
    return 0;
  }

  return source_list.size();
}

std::u16string GetFileSystemSources(const ui::ClipboardData& data) {
  // Outside of the Files app, file system sources are written as filenames.
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kFilenames)) {
    std::vector<std::string> sources;
    for (const ui::FileInfo& filename : data.filenames())
      sources.push_back(filename.path.value());
    return base::UTF8ToUTF16(base::JoinString(sources, "\n"));
  }

  // Within the Files app, file system sources are written as custom data.
  if (!ContainsFormat(data, ui::ClipboardInternalFormat::kCustom))
    return std::u16string();

  // Attempt to read file system sources in the custom data.
  std::u16string sources;
  ui::ReadCustomDataForType(data.custom_data_data().c_str(),
                            data.custom_data_data().size(),
                            kFileSystemSourcesType, &sources);

  return sources;
}

bool IsSupported(const ui::ClipboardData& data) {
  const absl::optional<ui::ClipboardInternalFormat> format =
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

ui::ImageModel GetIconForFileClipboardItem(const ClipboardHistoryItem& item,
                                           const std::string& file_name) {
  DCHECK_EQ(DisplayFormat::kFile, CalculateDisplayFormat(item.data()));
  const int copied_files_count = GetCountOfCopiedFiles(item.data());

  if (copied_files_count == 0)
    return ui::ImageModel();

  if (copied_files_count == 1) {
    return ui::ImageModel::FromImageSkia(chromeos::GetIconForPath(
        base::FilePath(file_name),
        ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()));
  }
  constexpr std::array<const gfx::VectorIcon*, 9> icons = {
      &kTwoFilesIcon,   &kThreeFilesIcon, &kFourFilesIcon,
      &kFiveFilesIcon,  &kSixFilesIcon,   &kSevenFilesIcon,
      &kEightFilesIcon, &kNineFilesIcon,  &kMoreThanNineFilesIcon};
  int icon_index = std::min(copied_files_count - 2, (int)icons.size() - 1);
  return ui::ImageModel::FromVectorIcon(*icons[icon_index],
                                        cros_tokens::kColorPrimary);
}

}  // namespace ash::clipboard_history_util
