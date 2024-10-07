// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_search_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

PickerTextResult::PickerTextResult(std::u16string_view text,
                                   PickerTextResult::Source source)
    : PickerTextResult(text, u"", ui::ImageModel(), source) {}

PickerTextResult::PickerTextResult(std::u16string_view primary_text,
                                   std::u16string_view secondary_text,
                                   ui::ImageModel icon,
                                   Source source)
    : primary_text(primary_text),
      secondary_text(secondary_text),
      icon(std::move(icon)),
      source(source) {}

PickerTextResult::PickerTextResult(const PickerTextResult&) = default;
PickerTextResult& PickerTextResult::operator=(const PickerTextResult&) =
    default;
PickerTextResult::~PickerTextResult() = default;

bool PickerTextResult::operator==(const PickerTextResult&) const = default;

PickerSearchRequestResult::PickerSearchRequestResult(
    std::u16string_view primary_text,
    std::u16string_view secondary_text,
    ui::ImageModel icon)
    : primary_text(primary_text),
      secondary_text(secondary_text),
      icon(std::move(icon)) {}

PickerSearchRequestResult::PickerSearchRequestResult(
    const PickerSearchRequestResult&) = default;
PickerSearchRequestResult& PickerSearchRequestResult::operator=(
    const PickerSearchRequestResult&) = default;
PickerSearchRequestResult::~PickerSearchRequestResult() = default;

bool PickerSearchRequestResult::operator==(
    const PickerSearchRequestResult&) const = default;

PickerEmojiResult PickerEmojiResult::Emoji(std::u16string_view text,
                                           std::u16string name) {
  return PickerEmojiResult(Type::kEmoji, text, std::move(name));
}

PickerEmojiResult PickerEmojiResult::Symbol(std::u16string_view text,
                                            std::u16string name) {
  return PickerEmojiResult(Type::kSymbol, text, std::move(name));
}

PickerEmojiResult PickerEmojiResult::Emoticon(std::u16string_view text,
                                              std::u16string name) {
  return PickerEmojiResult(Type::kEmoticon, text, std::move(name));
}

PickerEmojiResult::PickerEmojiResult(Type type,
                                     std::u16string_view text,
                                     std::u16string name)
    : type(type), text(text), name(std::move(name)) {}

PickerEmojiResult::PickerEmojiResult(const PickerEmojiResult&) = default;
PickerEmojiResult& PickerEmojiResult::operator=(const PickerEmojiResult&) =
    default;
PickerEmojiResult::~PickerEmojiResult() = default;

bool PickerEmojiResult::operator==(const PickerEmojiResult&) const = default;

PickerGifResult::PickerGifResult(const GURL& preview_url,
                                 const GURL& preview_image_url,
                                 const gfx::Size& preview_dimensions,
                                 const GURL& full_url,
                                 const gfx::Size& full_dimensions,
                                 std::u16string content_description)
    : preview_url(preview_url),
      preview_image_url(preview_image_url),
      preview_dimensions(preview_dimensions),
      full_url(full_url),
      full_dimensions(full_dimensions),
      content_description(std::move(content_description)) {}

PickerGifResult::PickerGifResult(const PickerGifResult&) = default;

PickerGifResult& PickerGifResult::operator=(const PickerGifResult&) = default;

PickerGifResult::~PickerGifResult() = default;

bool PickerGifResult::operator==(const PickerGifResult&) const = default;

PickerClipboardResult::PickerClipboardResult(
    base::UnguessableToken item_id,
    DisplayFormat display_format,
    size_t file_count,
    std::u16string display_text,
    std::optional<ui::ImageModel> display_image,
    bool is_recent)
    : item_id(item_id),
      display_format(display_format),
      file_count(file_count),
      display_text(std::move(display_text)),
      display_image(std::move(display_image)),
      is_recent(is_recent) {}

PickerClipboardResult::PickerClipboardResult(const PickerClipboardResult&) =
    default;

PickerClipboardResult& PickerClipboardResult::operator=(
    const PickerClipboardResult&) = default;

PickerClipboardResult::~PickerClipboardResult() = default;

bool PickerClipboardResult::operator==(const PickerClipboardResult&) const =
    default;

PickerLocalFileResult::PickerLocalFileResult(std::u16string title,
                                             base::FilePath file_path,
                                             bool best_match)
    : title(std::move(title)),
      file_path(std::move(file_path)),
      best_match(best_match) {}

PickerLocalFileResult::PickerLocalFileResult(const PickerLocalFileResult&) =
    default;

PickerLocalFileResult& PickerLocalFileResult::operator=(
    const PickerLocalFileResult&) = default;

PickerLocalFileResult::~PickerLocalFileResult() = default;

bool PickerLocalFileResult::operator==(const PickerLocalFileResult&) const =
    default;

PickerDriveFileResult::PickerDriveFileResult(std::optional<std::string> id,
                                             std::u16string title,
                                             GURL url,
                                             base::FilePath file_path,
                                             bool best_match)
    : id(std::move(id)),
      title(std::move(title)),
      url(std::move(url)),
      file_path(std::move(file_path)),
      best_match(best_match) {}

PickerDriveFileResult::PickerDriveFileResult(const PickerDriveFileResult&) =
    default;

PickerDriveFileResult& PickerDriveFileResult::operator=(
    const PickerDriveFileResult&) = default;

PickerDriveFileResult::~PickerDriveFileResult() = default;

bool PickerDriveFileResult::operator==(const PickerDriveFileResult&) const =
    default;

PickerBrowsingHistoryResult::PickerBrowsingHistoryResult(GURL url,
                                                         std::u16string title,
                                                         ui::ImageModel icon,
                                                         bool best_match)
    : url(std::move(url)),
      title(std::move(title)),
      icon(std::move(icon)),
      best_match(best_match) {}

PickerBrowsingHistoryResult::PickerBrowsingHistoryResult(
    const PickerBrowsingHistoryResult&) = default;

PickerBrowsingHistoryResult& PickerBrowsingHistoryResult::operator=(
    const PickerBrowsingHistoryResult&) = default;

PickerBrowsingHistoryResult::~PickerBrowsingHistoryResult() = default;

bool PickerBrowsingHistoryResult::operator==(
    const PickerBrowsingHistoryResult&) const = default;

PickerCategoryResult::PickerCategoryResult(PickerCategory category)
    : category(category) {}

PickerCategoryResult::PickerCategoryResult(const PickerCategoryResult&) =
    default;
PickerCategoryResult& PickerCategoryResult::operator=(
    const PickerCategoryResult&) = default;
PickerCategoryResult::~PickerCategoryResult() = default;

bool PickerCategoryResult::operator==(const PickerCategoryResult&) const =
    default;

PickerEditorResult::PickerEditorResult(
    Mode mode,
    std::u16string display_name,
    std::optional<chromeos::editor_menu::PresetQueryCategory> category,
    std::optional<std::string> preset_query_id)
    : mode(mode),
      display_name(std::move(display_name)),
      category(std::move(category)),
      preset_query_id(std::move(preset_query_id)) {}

PickerEditorResult::PickerEditorResult(const PickerEditorResult&) = default;

PickerEditorResult& PickerEditorResult::operator=(const PickerEditorResult&) =
    default;

PickerEditorResult::~PickerEditorResult() = default;

bool PickerEditorResult::operator==(const PickerEditorResult&) const = default;

PickerLobsterResult::PickerLobsterResult(std::u16string display_name)
    : display_name(std::move(display_name)) {}

PickerLobsterResult::PickerLobsterResult(const PickerLobsterResult&) = default;

PickerLobsterResult& PickerLobsterResult::operator=(
    const PickerLobsterResult&) = default;

PickerLobsterResult::~PickerLobsterResult() = default;

bool PickerLobsterResult::operator==(const PickerLobsterResult&) const =
    default;

PickerNewWindowResult::PickerNewWindowResult(Type type) : type(type) {}

PickerNewWindowResult::PickerNewWindowResult(const PickerNewWindowResult&) =
    default;
PickerNewWindowResult& PickerNewWindowResult::operator=(
    const PickerNewWindowResult&) = default;
PickerNewWindowResult::~PickerNewWindowResult() = default;

bool PickerNewWindowResult::operator==(const PickerNewWindowResult&) const =
    default;

PickerCapsLockResult::PickerCapsLockResult(
    bool enabled,
    PickerCapsLockResult::Shortcut shortcut)
    : enabled(enabled), shortcut(shortcut) {}

PickerCapsLockResult::PickerCapsLockResult(const PickerCapsLockResult&) =
    default;
PickerCapsLockResult& PickerCapsLockResult::operator=(
    const PickerCapsLockResult&) = default;
PickerCapsLockResult::~PickerCapsLockResult() = default;

bool PickerCapsLockResult::operator==(const PickerCapsLockResult&) const =
    default;

PickerCaseTransformResult::PickerCaseTransformResult(Type type) : type(type) {}

PickerCaseTransformResult::PickerCaseTransformResult(
    const PickerCaseTransformResult&) = default;
PickerCaseTransformResult& PickerCaseTransformResult::operator=(
    const PickerCaseTransformResult&) = default;
PickerCaseTransformResult::~PickerCaseTransformResult() = default;

bool PickerCaseTransformResult::operator==(
    const PickerCaseTransformResult&) const = default;

}  // namespace ash
