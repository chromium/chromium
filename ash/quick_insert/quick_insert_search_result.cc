// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_search_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

QuickInsertTextResult::QuickInsertTextResult(
    std::u16string_view text,
    QuickInsertTextResult::Source source)
    : QuickInsertTextResult(text, u"", ui::ImageModel(), source) {}

QuickInsertTextResult::QuickInsertTextResult(std::u16string_view primary_text,
                                             std::u16string_view secondary_text,
                                             ui::ImageModel icon,
                                             Source source)
    : primary_text(primary_text),
      secondary_text(secondary_text),
      icon(std::move(icon)),
      source(source) {}

QuickInsertTextResult::QuickInsertTextResult(const QuickInsertTextResult&) =
    default;
QuickInsertTextResult& QuickInsertTextResult::operator=(
    const QuickInsertTextResult&) = default;
QuickInsertTextResult::QuickInsertTextResult(QuickInsertTextResult&&) = default;
QuickInsertTextResult& QuickInsertTextResult::operator=(
    QuickInsertTextResult&&) = default;
QuickInsertTextResult::~QuickInsertTextResult() = default;

bool QuickInsertTextResult::operator==(const QuickInsertTextResult&) const =
    default;

QuickInsertSearchRequestResult::QuickInsertSearchRequestResult(
    std::u16string_view primary_text,
    std::u16string_view secondary_text,
    ui::ImageModel icon)
    : primary_text(primary_text),
      secondary_text(secondary_text),
      icon(std::move(icon)) {}

QuickInsertSearchRequestResult::QuickInsertSearchRequestResult(
    const QuickInsertSearchRequestResult&) = default;
QuickInsertSearchRequestResult& QuickInsertSearchRequestResult::operator=(
    const QuickInsertSearchRequestResult&) = default;
QuickInsertSearchRequestResult::QuickInsertSearchRequestResult(
    QuickInsertSearchRequestResult&&) = default;
QuickInsertSearchRequestResult& QuickInsertSearchRequestResult::operator=(
    QuickInsertSearchRequestResult&&) = default;
QuickInsertSearchRequestResult::~QuickInsertSearchRequestResult() = default;

bool QuickInsertSearchRequestResult::operator==(
    const QuickInsertSearchRequestResult&) const = default;

QuickInsertEmojiResult QuickInsertEmojiResult::Emoji(std::u16string_view text,
                                                     std::u16string name) {
  return QuickInsertEmojiResult(Type::kEmoji, text, std::move(name));
}

QuickInsertEmojiResult QuickInsertEmojiResult::Symbol(std::u16string_view text,
                                                      std::u16string name) {
  return QuickInsertEmojiResult(Type::kSymbol, text, std::move(name));
}

QuickInsertEmojiResult QuickInsertEmojiResult::Emoticon(
    std::u16string_view text,
    std::u16string name) {
  return QuickInsertEmojiResult(Type::kEmoticon, text, std::move(name));
}

QuickInsertEmojiResult::QuickInsertEmojiResult(Type type,
                                               std::u16string_view text,
                                               std::u16string name)
    : type(type), text(text), name(std::move(name)) {}

QuickInsertEmojiResult::QuickInsertEmojiResult(const QuickInsertEmojiResult&) =
    default;
QuickInsertEmojiResult& QuickInsertEmojiResult::operator=(
    const QuickInsertEmojiResult&) = default;
QuickInsertEmojiResult::QuickInsertEmojiResult(QuickInsertEmojiResult&&) =
    default;
QuickInsertEmojiResult& QuickInsertEmojiResult::operator=(
    QuickInsertEmojiResult&&) = default;
QuickInsertEmojiResult::~QuickInsertEmojiResult() = default;

bool QuickInsertEmojiResult::operator==(const QuickInsertEmojiResult&) const =
    default;

QuickInsertGifResult::QuickInsertGifResult(const GURL& preview_url,
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

QuickInsertGifResult::QuickInsertGifResult(const QuickInsertGifResult&) =
    default;

QuickInsertGifResult& QuickInsertGifResult::operator=(
    const QuickInsertGifResult&) = default;

QuickInsertGifResult::QuickInsertGifResult(QuickInsertGifResult&&) = default;

QuickInsertGifResult& QuickInsertGifResult::operator=(QuickInsertGifResult&&) =
    default;

QuickInsertGifResult::~QuickInsertGifResult() = default;

bool QuickInsertGifResult::operator==(const QuickInsertGifResult&) const =
    default;

QuickInsertClipboardResult::QuickInsertClipboardResult(
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

QuickInsertClipboardResult::QuickInsertClipboardResult(
    const QuickInsertClipboardResult&) = default;

QuickInsertClipboardResult& QuickInsertClipboardResult::operator=(
    const QuickInsertClipboardResult&) = default;

QuickInsertClipboardResult::QuickInsertClipboardResult(
    QuickInsertClipboardResult&&) = default;

QuickInsertClipboardResult& QuickInsertClipboardResult::operator=(
    QuickInsertClipboardResult&&) = default;

QuickInsertClipboardResult::~QuickInsertClipboardResult() = default;

bool QuickInsertClipboardResult::operator==(
    const QuickInsertClipboardResult&) const = default;

QuickInsertLocalFileResult::QuickInsertLocalFileResult(std::u16string title,
                                                       base::FilePath file_path,
                                                       bool best_match)
    : title(std::move(title)),
      file_path(std::move(file_path)),
      best_match(best_match) {}

QuickInsertLocalFileResult::QuickInsertLocalFileResult(
    const QuickInsertLocalFileResult&) = default;

QuickInsertLocalFileResult& QuickInsertLocalFileResult::operator=(
    const QuickInsertLocalFileResult&) = default;

QuickInsertLocalFileResult::QuickInsertLocalFileResult(
    QuickInsertLocalFileResult&&) = default;

QuickInsertLocalFileResult& QuickInsertLocalFileResult::operator=(
    QuickInsertLocalFileResult&&) = default;

QuickInsertLocalFileResult::~QuickInsertLocalFileResult() = default;

bool QuickInsertLocalFileResult::operator==(
    const QuickInsertLocalFileResult&) const = default;

QuickInsertDriveFileResult::QuickInsertDriveFileResult(
    std::optional<std::string> id,
    std::u16string title,
    GURL url,
    base::FilePath file_path,
    bool best_match)
    : id(std::move(id)),
      title(std::move(title)),
      url(std::move(url)),
      file_path(std::move(file_path)),
      best_match(best_match) {}

QuickInsertDriveFileResult::QuickInsertDriveFileResult(
    const QuickInsertDriveFileResult&) = default;

QuickInsertDriveFileResult& QuickInsertDriveFileResult::operator=(
    const QuickInsertDriveFileResult&) = default;

QuickInsertDriveFileResult::QuickInsertDriveFileResult(
    QuickInsertDriveFileResult&&) = default;

QuickInsertDriveFileResult& QuickInsertDriveFileResult::operator=(
    QuickInsertDriveFileResult&&) = default;

QuickInsertDriveFileResult::~QuickInsertDriveFileResult() = default;

bool QuickInsertDriveFileResult::operator==(
    const QuickInsertDriveFileResult&) const = default;

QuickInsertBrowsingHistoryResult::QuickInsertBrowsingHistoryResult(
    GURL url,
    std::u16string title,
    ui::ImageModel icon,
    bool best_match)
    : url(std::move(url)),
      title(std::move(title)),
      icon(std::move(icon)),
      best_match(best_match) {}

QuickInsertBrowsingHistoryResult::QuickInsertBrowsingHistoryResult(
    const QuickInsertBrowsingHistoryResult&) = default;

QuickInsertBrowsingHistoryResult& QuickInsertBrowsingHistoryResult::operator=(
    const QuickInsertBrowsingHistoryResult&) = default;

QuickInsertBrowsingHistoryResult::QuickInsertBrowsingHistoryResult(
    QuickInsertBrowsingHistoryResult&&) = default;

QuickInsertBrowsingHistoryResult& QuickInsertBrowsingHistoryResult::operator=(
    QuickInsertBrowsingHistoryResult&&) = default;

QuickInsertBrowsingHistoryResult::~QuickInsertBrowsingHistoryResult() = default;

bool QuickInsertBrowsingHistoryResult::operator==(
    const QuickInsertBrowsingHistoryResult&) const = default;

QuickInsertCategoryResult::QuickInsertCategoryResult(
    QuickInsertCategory category)
    : category(category) {}

QuickInsertCategoryResult::QuickInsertCategoryResult(
    const QuickInsertCategoryResult&) = default;
QuickInsertCategoryResult& QuickInsertCategoryResult::operator=(
    const QuickInsertCategoryResult&) = default;
QuickInsertCategoryResult::QuickInsertCategoryResult(
    QuickInsertCategoryResult&&) = default;
QuickInsertCategoryResult& QuickInsertCategoryResult::operator=(
    QuickInsertCategoryResult&&) = default;
QuickInsertCategoryResult::~QuickInsertCategoryResult() = default;

bool QuickInsertCategoryResult::operator==(
    const QuickInsertCategoryResult&) const = default;

QuickInsertEditorResult::QuickInsertEditorResult(
    Mode mode,
    std::u16string display_name,
    std::optional<chromeos::editor_menu::PresetQueryCategory> category,
    std::optional<std::string> preset_query_id)
    : mode(mode),
      display_name(std::move(display_name)),
      category(std::move(category)),
      preset_query_id(std::move(preset_query_id)) {}

QuickInsertEditorResult::QuickInsertEditorResult(
    const QuickInsertEditorResult&) = default;

QuickInsertEditorResult& QuickInsertEditorResult::operator=(
    const QuickInsertEditorResult&) = default;

QuickInsertEditorResult::QuickInsertEditorResult(QuickInsertEditorResult&&) =
    default;

QuickInsertEditorResult& QuickInsertEditorResult::operator=(
    QuickInsertEditorResult&&) = default;

QuickInsertEditorResult::~QuickInsertEditorResult() = default;

bool QuickInsertEditorResult::operator==(const QuickInsertEditorResult&) const =
    default;

QuickInsertLobsterResult::QuickInsertLobsterResult(Mode mode,
                                                   std::u16string display_name)
    : mode(mode), display_name(std::move(display_name)) {}

QuickInsertLobsterResult::QuickInsertLobsterResult(
    const QuickInsertLobsterResult&) = default;

QuickInsertLobsterResult& QuickInsertLobsterResult::operator=(
    const QuickInsertLobsterResult&) = default;

QuickInsertLobsterResult::QuickInsertLobsterResult(QuickInsertLobsterResult&&) =
    default;

QuickInsertLobsterResult& QuickInsertLobsterResult::operator=(
    QuickInsertLobsterResult&&) = default;

QuickInsertLobsterResult::~QuickInsertLobsterResult() = default;

bool QuickInsertLobsterResult::operator==(
    const QuickInsertLobsterResult&) const = default;

QuickInsertNewWindowResult::QuickInsertNewWindowResult(Type type)
    : type(type) {}

QuickInsertNewWindowResult::QuickInsertNewWindowResult(
    const QuickInsertNewWindowResult&) = default;
QuickInsertNewWindowResult& QuickInsertNewWindowResult::operator=(
    const QuickInsertNewWindowResult&) = default;
QuickInsertNewWindowResult::QuickInsertNewWindowResult(
    QuickInsertNewWindowResult&&) = default;
QuickInsertNewWindowResult& QuickInsertNewWindowResult::operator=(
    QuickInsertNewWindowResult&&) = default;
QuickInsertNewWindowResult::~QuickInsertNewWindowResult() = default;

bool QuickInsertNewWindowResult::operator==(
    const QuickInsertNewWindowResult&) const = default;

QuickInsertCapsLockResult::QuickInsertCapsLockResult(
    bool enabled,
    QuickInsertCapsLockResult::Shortcut shortcut)
    : enabled(enabled), shortcut(shortcut) {}

QuickInsertCapsLockResult::QuickInsertCapsLockResult(
    const QuickInsertCapsLockResult&) = default;
QuickInsertCapsLockResult& QuickInsertCapsLockResult::operator=(
    const QuickInsertCapsLockResult&) = default;
QuickInsertCapsLockResult::QuickInsertCapsLockResult(
    QuickInsertCapsLockResult&&) = default;
QuickInsertCapsLockResult& QuickInsertCapsLockResult::operator=(
    QuickInsertCapsLockResult&&) = default;
QuickInsertCapsLockResult::~QuickInsertCapsLockResult() = default;

bool QuickInsertCapsLockResult::operator==(
    const QuickInsertCapsLockResult&) const = default;

QuickInsertCaseTransformResult::QuickInsertCaseTransformResult(Type type)
    : type(type) {}

QuickInsertCaseTransformResult::QuickInsertCaseTransformResult(
    const QuickInsertCaseTransformResult&) = default;
QuickInsertCaseTransformResult& QuickInsertCaseTransformResult::operator=(
    const QuickInsertCaseTransformResult&) = default;
QuickInsertCaseTransformResult::QuickInsertCaseTransformResult(
    QuickInsertCaseTransformResult&&) = default;
QuickInsertCaseTransformResult& QuickInsertCaseTransformResult::operator=(
    QuickInsertCaseTransformResult&&) = default;
QuickInsertCaseTransformResult::~QuickInsertCaseTransformResult() = default;

bool QuickInsertCaseTransformResult::operator==(
    const QuickInsertCaseTransformResult&) const = default;

}  // namespace ash
