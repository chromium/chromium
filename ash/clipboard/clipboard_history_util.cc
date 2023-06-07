// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/metrics/histogram_macros.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/menu/menu_config.h"

namespace ash::clipboard_history_util {

namespace {

// Constants -------------------------------------------------------------------

constexpr char16_t kFileSystemSourcesType[] = u"fs/sources";

constexpr int kPlaceholderImageWidth = 234;
constexpr int kPlaceholderImageHeight = 74;
constexpr int kPlaceholderImageOutlineCornerRadius = 8;

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

// Helper classes --------------------------------------------------------------

// Used to draw a placeholder HTML preview to be shown while the real HTML is
// rendering.
class UnrenderedHtmlPlaceholderImage : public gfx::CanvasImageSource {
 public:
  UnrenderedHtmlPlaceholderImage()
      : gfx::CanvasImageSource(
            gfx::Size(kPlaceholderImageWidth, kPlaceholderImageHeight)) {}
  UnrenderedHtmlPlaceholderImage(const UnrenderedHtmlPlaceholderImage&) =
      delete;
  UnrenderedHtmlPlaceholderImage& operator=(
      const UnrenderedHtmlPlaceholderImage&) = delete;
  ~UnrenderedHtmlPlaceholderImage() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey100);
    canvas->DrawRoundRect(
        /*rect=*/{kPlaceholderImageWidth, kPlaceholderImageHeight},
        kPlaceholderImageOutlineCornerRadius, flags);

    flags = cc::PaintFlags();
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    const gfx::ImageSkia center_image = gfx::CreateVectorIcon(
        kUnrenderedHtmlPlaceholderIcon,
        ClipboardHistoryViews::kBitmapItemPlaceholderIconSize,
        gfx::kGoogleGrey600);
    canvas->DrawImageInt(
        center_image, (size().width() - center_image.size().width()) / 2,
        (size().height() - center_image.size().height()) / 2, flags);
  }
};

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

bool ContainsFormat(const ui::ClipboardData& data,
                    ui::ClipboardInternalFormat format) {
  return data.format() & static_cast<int>(format);
}

void RecordClipboardHistoryItemDeleted(const ClipboardHistoryItem& item) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatDeleted",
      item.display_format());
}

void RecordClipboardHistoryItemPasted(const ClipboardHistoryItem& item) {
  UMA_HISTOGRAM_ENUMERATION(
      "Ash.ClipboardHistory.ContextMenu.DisplayFormatPasted",
      item.display_format());
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
    for (const ui::FileInfo& filename : data.filenames()) {
      sources.push_back(filename.path.value());
    }
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

ui::ImageModel GetIconForFileClipboardItem(const ClipboardHistoryItem& item) {
  DCHECK_EQ(item.display_format(),
            crosapi::mojom::ClipboardHistoryDisplayFormat::kFile);
  const int copied_files_count = GetCountOfCopiedFiles(item.data());
  if (copied_files_count == 0)
    return ui::ImageModel();

  constexpr std::array<const gfx::VectorIcon*, 9> icons = {
      &kTwoFilesIcon,   &kThreeFilesIcon, &kFourFilesIcon,
      &kFiveFilesIcon,  &kSixFilesIcon,   &kSevenFilesIcon,
      &kEightFilesIcon, &kNineFilesIcon,  &kMoreThanNineFilesIcon};
  int icon_index = std::min(copied_files_count - 2, (int)icons.size() - 1);

  const auto* vector_icon = copied_files_count == 1
                                ? &chromeos::GetIconForPath(base::FilePath(
                                      base::UTF16ToUTF8(item.display_text())))
                                : icons[icon_index];
  return ui::ImageModel::FromVectorIcon(*vector_icon, ui::kColorSysSecondary);
}

ui::ImageModel GetHtmlPreviewPlaceholder() {
  static base::NoDestructor<ui::ImageModel> model(
      chromeos::features::IsClipboardHistoryRefreshEnabled()
          ? ui::ImageModel::FromVectorIcon(
                kUnrenderedHtmlPlaceholderIcon, cros_tokens::kCrosSysOutline,
                ClipboardHistoryViews::kBitmapItemPlaceholderIconSize)
          : ui::ImageModel::FromImageSkia(gfx::CanvasImageSource::MakeImageSkia<
                                          UnrenderedHtmlPlaceholderImage>()));
  return *model;
}

std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor>
GetItemDescriptorsFrom(const std::list<ClipboardHistoryItem>& items) {
  std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor> item_descriptors;
  for (const auto& item : items) {
    item_descriptors.emplace_back(item.id(), item.display_format(),
                                  item.display_text(), item.file_count());
  }
  return item_descriptors;
}

int GetPreferredItemViewWidth() {
  return views::MenuConfig::instance().touchable_menu_min_width;
}

}  // namespace ash::clipboard_history_util
