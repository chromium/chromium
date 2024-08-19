// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/picker/picker_search_result.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace ash {

PickerSearchResult::TextData::TextData(std::u16string primary_text,
                                       std::u16string secondary_text,
                                       ui::ImageModel icon,
                                       Source source)
    : primary_text(std::move(primary_text)),
      secondary_text(std::move(secondary_text)),
      icon(std::move(icon)),
      source(source) {}

PickerSearchResult::TextData::TextData(const PickerSearchResult::TextData&) =
    default;

PickerSearchResult::TextData& PickerSearchResult::TextData::operator=(
    const PickerSearchResult::TextData&) = default;

PickerSearchResult::TextData::~TextData() = default;

bool PickerSearchResult::TextData::operator==(
    const PickerSearchResult::TextData&) const = default;

bool PickerSearchResult::SearchRequestData::operator==(
    const PickerSearchResult::SearchRequestData&) const = default;

bool PickerSearchResult::EmojiData::operator==(
    const PickerSearchResult::EmojiData&) const = default;

PickerSearchResult::ClipboardData::ClipboardData(
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

PickerSearchResult::ClipboardData::ClipboardData(
    const PickerSearchResult::ClipboardData&) = default;

PickerSearchResult::ClipboardData& PickerSearchResult::ClipboardData::operator=(
    const PickerSearchResult::ClipboardData&) = default;

PickerSearchResult::ClipboardData::~ClipboardData() = default;

bool PickerSearchResult::ClipboardData::operator==(
    const PickerSearchResult::ClipboardData&) const = default;

bool PickerSearchResult::LocalFileData::operator==(const LocalFileData&) const =
    default;

PickerSearchResult::DriveFileData::DriveFileData(std::optional<std::string> id,
                                                 std::u16string title,
                                                 GURL url,
                                                 base::FilePath file_path,
                                                 bool best_match)
    : id(std::move(id)),
      title(std::move(title)),
      url(std::move(url)),
      file_path(std::move(file_path)),
      best_match(best_match) {}

PickerSearchResult::DriveFileData::DriveFileData(const DriveFileData&) =
    default;

PickerSearchResult::DriveFileData& PickerSearchResult::DriveFileData::operator=(
    const DriveFileData&) = default;

PickerSearchResult::DriveFileData::~DriveFileData() = default;

bool PickerSearchResult::DriveFileData::operator==(const DriveFileData&) const =
    default;

PickerSearchResult::BrowsingHistoryData::BrowsingHistoryData(
    GURL url,
    std::u16string title,
    ui::ImageModel icon,
    bool best_match)
    : url(std::move(url)),
      title(std::move(title)),
      icon(std::move(icon)),
      best_match(best_match) {}

PickerSearchResult::BrowsingHistoryData::BrowsingHistoryData(
    const BrowsingHistoryData&) = default;

PickerSearchResult::BrowsingHistoryData&
PickerSearchResult::BrowsingHistoryData::operator=(const BrowsingHistoryData&) =
    default;

PickerSearchResult::BrowsingHistoryData::~BrowsingHistoryData() = default;

bool PickerSearchResult::BrowsingHistoryData::operator==(
    const PickerSearchResult::BrowsingHistoryData&) const = default;

bool PickerSearchResult::CategoryData::operator==(const CategoryData&) const =
    default;

PickerSearchResult::EditorData::EditorData(
    Mode mode,
    std::u16string display_name,
    std::optional<chromeos::editor_menu::PresetQueryCategory> category,
    std::optional<std::string> preset_query_id)
    : mode(mode),
      display_name(std::move(display_name)),
      category(std::move(category)),
      preset_query_id(std::move(preset_query_id)) {}

PickerSearchResult::EditorData::EditorData(
    const PickerSearchResult::EditorData&) = default;

PickerSearchResult::EditorData& PickerSearchResult::EditorData::operator=(
    const PickerSearchResult::EditorData&) = default;

PickerSearchResult::EditorData::~EditorData() = default;

bool PickerSearchResult::EditorData::operator==(const EditorData&) const =
    default;

bool PickerSearchResult::NewWindowData::operator==(const NewWindowData&) const =
    default;

bool PickerSearchResult::CapsLockData::operator==(const CapsLockData&) const =
    default;

bool PickerSearchResult::CaseTransformData::operator==(
    const CaseTransformData&) const = default;

PickerSearchResult::~PickerSearchResult() = default;

PickerSearchResult::PickerSearchResult(const PickerSearchResult&) = default;

PickerSearchResult& PickerSearchResult::operator=(const PickerSearchResult&) =
    default;

PickerSearchResult::PickerSearchResult(PickerSearchResult&&) = default;

PickerSearchResult& PickerSearchResult::operator=(PickerSearchResult&&) =
    default;

PickerSearchResult PickerSearchResult::Text(std::u16string_view text,
                                            TextData::Source source) {
  return PickerSearchResult(
      TextData(std::u16string(text), u"", ui::ImageModel(), source));
}

PickerSearchResult PickerSearchResult::Text(std::u16string_view primary_text,
                                            std::u16string_view secondary_text,
                                            ui::ImageModel icon,
                                            TextData::Source source) {
  return PickerSearchResult(TextData(std::u16string(primary_text),
                                     std::u16string(secondary_text),
                                     std::move(icon), source));
}

PickerSearchResult PickerSearchResult::SearchRequest(
    std::u16string_view primary_text,
    std::u16string_view secondary_text,
    ui::ImageModel icon) {
  return PickerSearchResult(
      SearchRequestData{.primary_text = std::u16string(primary_text),
                        .secondary_text = std::u16string(secondary_text),
                        .icon = std::move(icon)});
}

PickerSearchResult PickerSearchResult::Emoji(std::u16string_view emoji,
                                             std::u16string name) {
  return PickerSearchResult(EmojiData{.type = EmojiData::Type::kEmoji,
                                      .text = std::u16string(emoji),
                                      .name = std::move(name)});
}

PickerSearchResult PickerSearchResult::Symbol(std::u16string_view symbol,
                                              std::u16string name) {
  return PickerSearchResult(EmojiData{.type = EmojiData::Type::kSymbol,
                                      .text = std::u16string(symbol),
                                      .name = std::move(name)});
}

PickerSearchResult PickerSearchResult::Emoticon(std::u16string_view emoticon,
                                                std::u16string name) {
  return PickerSearchResult(EmojiData{.type = EmojiData::Type::kEmoticon,
                                      .text = std::u16string(emoticon),
                                      .name = std::move(name)});
}

PickerSearchResult PickerSearchResult::Clipboard(
    base::UnguessableToken item_id,
    ClipboardData::DisplayFormat display_format,
    size_t file_count,
    std::u16string display_text,
    std::optional<ui::ImageModel> display_image,
    bool is_recent) {
  return PickerSearchResult(ClipboardData(item_id, display_format, file_count,
                                          std::move(display_text),
                                          std::move(display_image), is_recent));
}

PickerSearchResult PickerSearchResult::BrowsingHistory(const GURL& url,
                                                       std::u16string title,
                                                       ui::ImageModel icon,
                                                       bool best_match) {
  return PickerSearchResult(
      BrowsingHistoryData(url, std::move(title), std::move(icon), best_match));
}

PickerSearchResult PickerSearchResult::LocalFile(std::u16string title,
                                                 base::FilePath file_path,
                                                 bool best_match) {
  return PickerSearchResult(LocalFileData{.file_path = std::move(file_path),
                                          .title = std::move(title),
                                          .best_match = best_match});
}

PickerSearchResult PickerSearchResult::DriveFile(std::optional<std::string> id,
                                                 std::u16string title,
                                                 const GURL& url,
                                                 base::FilePath file_path,
                                                 bool best_match) {
  return PickerSearchResult(DriveFileData(std::move(id), std::move(title), url,
                                          std::move(file_path), best_match));
}

PickerSearchResult PickerSearchResult::Category(PickerCategory category) {
  return PickerSearchResult(CategoryData{.category = category});
}

PickerSearchResult PickerSearchResult::Editor(
    PickerSearchResult::EditorData::Mode mode,
    std::u16string display_name,
    std::optional<chromeos::editor_menu::PresetQueryCategory> category,
    std::optional<std::string> text_query_id) {
  return PickerSearchResult(EditorData(mode, std::move(display_name),
                                       std::move(category),
                                       std::move(text_query_id)));
}

PickerSearchResult PickerSearchResult::NewWindow(
    PickerSearchResult::NewWindowData::Type type) {
  return PickerSearchResult(NewWindowData{.type = type});
}

PickerSearchResult PickerSearchResult::CapsLock(
    bool enabled,
    CapsLockData::Shortcut shortcut) {
  return PickerSearchResult(
      CapsLockData{.enabled = enabled, .shortcut = shortcut});
}

PickerSearchResult PickerSearchResult::CaseTransform(
    CaseTransformData::Type type) {
  return PickerSearchResult(CaseTransformData{.type = type});
}

bool PickerSearchResult::operator==(const PickerSearchResult&) const = default;

const PickerSearchResult::Data& PickerSearchResult::data() const {
  return data_;
}

PickerSearchResult::PickerSearchResult(Data data) : data_(std::move(data)) {}

}  // namespace ash
