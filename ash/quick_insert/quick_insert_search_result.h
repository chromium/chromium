// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_SEARCH_RESULT_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_SEARCH_RESULT_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace chromeos::editor_menu {
enum class PresetQueryCategory;
}

namespace ash {

struct ASH_EXPORT QuickInsertTextResult {
  enum class Source {
    kUnknown,  // This should only be used for tests.
    kDate,
    kMath,
    kCaseTransform,
    kOmnibox,
  };

  std::u16string primary_text;
  std::u16string secondary_text;
  ui::ImageModel icon;
  Source source;

  explicit QuickInsertTextResult(std::u16string_view text,
                                 QuickInsertTextResult::Source source =
                                     QuickInsertTextResult::Source::kUnknown);
  explicit QuickInsertTextResult(std::u16string_view primary_text,
                                 std::u16string_view secondary_text,
                                 ui::ImageModel icon,
                                 Source source);
  QuickInsertTextResult(const QuickInsertTextResult&);
  QuickInsertTextResult& operator=(const QuickInsertTextResult&);
  QuickInsertTextResult(QuickInsertTextResult&&);
  QuickInsertTextResult& operator=(QuickInsertTextResult&&);
  ~QuickInsertTextResult();

  bool operator==(const QuickInsertTextResult&) const;
};

struct ASH_EXPORT QuickInsertSearchRequestResult {
  std::u16string primary_text;
  std::u16string secondary_text;
  ui::ImageModel icon;

  explicit QuickInsertSearchRequestResult(std::u16string_view primary_text,
                                          std::u16string_view secondary_text,
                                          ui::ImageModel icon);
  QuickInsertSearchRequestResult(const QuickInsertSearchRequestResult&);
  QuickInsertSearchRequestResult& operator=(
      const QuickInsertSearchRequestResult&);
  QuickInsertSearchRequestResult(QuickInsertSearchRequestResult&&);
  QuickInsertSearchRequestResult& operator=(QuickInsertSearchRequestResult&&);
  ~QuickInsertSearchRequestResult();

  bool operator==(const QuickInsertSearchRequestResult&) const;
};

struct ASH_EXPORT QuickInsertEmojiResult {
  enum class Type { kEmoji, kSymbol, kEmoticon };

  Type type;
  std::u16string text;
  std::u16string name;

  static QuickInsertEmojiResult Emoji(std::u16string_view text,
                                      std::u16string name = u"");
  static QuickInsertEmojiResult Symbol(std::u16string_view text,
                                       std::u16string name = u"");
  static QuickInsertEmojiResult Emoticon(std::u16string_view text,
                                         std::u16string name = u"");

  explicit QuickInsertEmojiResult(Type type,
                                  std::u16string_view text,
                                  std::u16string name);
  QuickInsertEmojiResult(const QuickInsertEmojiResult&);
  QuickInsertEmojiResult& operator=(const QuickInsertEmojiResult&);
  QuickInsertEmojiResult(QuickInsertEmojiResult&&);
  QuickInsertEmojiResult& operator=(QuickInsertEmojiResult&&);
  ~QuickInsertEmojiResult();

  bool operator==(const QuickInsertEmojiResult&) const;
};

struct ASH_EXPORT QuickInsertGifResult {
  QuickInsertGifResult(const GURL& preview_url,
                       const GURL& preview_image_url,
                       const gfx::Size& preview_dimensions,
                       const GURL& full_url,
                       const gfx::Size& full_dimensions,
                       std::u16string content_description);
  QuickInsertGifResult(const QuickInsertGifResult&);
  QuickInsertGifResult& operator=(const QuickInsertGifResult&);
  QuickInsertGifResult(QuickInsertGifResult&&);
  QuickInsertGifResult& operator=(QuickInsertGifResult&&);
  ~QuickInsertGifResult();

  // A url to an animated preview gif media source.
  GURL preview_url;

  // A url to an unanimated preview image of the gif media source.
  GURL preview_image_url;

  // Width and height of the GIF at `preview_url`.
  gfx::Size preview_dimensions;

  // A url to a full-sized gif media source.
  GURL full_url;

  // Width and height of the GIF at `full_url`.
  gfx::Size full_dimensions;

  // A textual description of the content, primarily used for accessibility
  // features.
  std::u16string content_description;

  bool operator==(const QuickInsertGifResult&) const;
};

struct ASH_EXPORT QuickInsertClipboardResult {
  enum class DisplayFormat {
    kFile,
    kText,
    kImage,
    kHtml,
  };

  // Unique ID that specifies which item in the clipboard this refers to.
  base::UnguessableToken item_id;
  DisplayFormat display_format;
  // If this is 1, `display_text` should be the filename of the file.
  size_t file_count;
  std::u16string display_text;
  std::optional<ui::ImageModel> display_image;
  bool is_recent;

  explicit QuickInsertClipboardResult(
      base::UnguessableToken item_id,
      DisplayFormat display_format,
      size_t file_count,
      std::u16string display_text,
      std::optional<ui::ImageModel> display_image,
      bool is_recent);
  QuickInsertClipboardResult(const QuickInsertClipboardResult&);
  QuickInsertClipboardResult& operator=(const QuickInsertClipboardResult&);
  QuickInsertClipboardResult(QuickInsertClipboardResult&&);
  QuickInsertClipboardResult& operator=(QuickInsertClipboardResult&&);
  ~QuickInsertClipboardResult();

  bool operator==(const QuickInsertClipboardResult&) const;
};

struct ASH_EXPORT QuickInsertBrowsingHistoryResult {
  GURL url;
  std::u16string title;
  ui::ImageModel icon;
  bool best_match;

  explicit QuickInsertBrowsingHistoryResult(GURL url,
                                            std::u16string title,
                                            ui::ImageModel icon,
                                            bool best_match = false);
  QuickInsertBrowsingHistoryResult(const QuickInsertBrowsingHistoryResult&);
  QuickInsertBrowsingHistoryResult& operator=(
      const QuickInsertBrowsingHistoryResult&);
  QuickInsertBrowsingHistoryResult(QuickInsertBrowsingHistoryResult&&);
  QuickInsertBrowsingHistoryResult& operator=(
      QuickInsertBrowsingHistoryResult&&);
  ~QuickInsertBrowsingHistoryResult();

  bool operator==(const QuickInsertBrowsingHistoryResult&) const;
};

struct ASH_EXPORT QuickInsertLocalFileResult {
  std::u16string title;
  base::FilePath file_path;
  bool best_match;

  explicit QuickInsertLocalFileResult(std::u16string title,
                                      base::FilePath file_path,
                                      bool best_match = false);
  QuickInsertLocalFileResult(const QuickInsertLocalFileResult&);
  QuickInsertLocalFileResult& operator=(const QuickInsertLocalFileResult&);
  QuickInsertLocalFileResult(QuickInsertLocalFileResult&&);
  QuickInsertLocalFileResult& operator=(QuickInsertLocalFileResult&&);
  ~QuickInsertLocalFileResult();

  bool operator==(const QuickInsertLocalFileResult&) const;
};

struct ASH_EXPORT QuickInsertDriveFileResult {
  std::optional<std::string> id;
  std::u16string title;
  GURL url;
  base::FilePath file_path;
  bool best_match;

  explicit QuickInsertDriveFileResult(std::optional<std::string> id,
                                      std::u16string title,
                                      GURL url,
                                      base::FilePath file_path,
                                      bool best_match = false);
  QuickInsertDriveFileResult(const QuickInsertDriveFileResult&);
  QuickInsertDriveFileResult& operator=(const QuickInsertDriveFileResult&);
  QuickInsertDriveFileResult(QuickInsertDriveFileResult&&);
  QuickInsertDriveFileResult& operator=(QuickInsertDriveFileResult&&);
  ~QuickInsertDriveFileResult();

  bool operator==(const QuickInsertDriveFileResult&) const;
};

struct ASH_EXPORT QuickInsertCategoryResult {
  QuickInsertCategory category;

  explicit QuickInsertCategoryResult(QuickInsertCategory category);
  QuickInsertCategoryResult(const QuickInsertCategoryResult&);
  QuickInsertCategoryResult& operator=(const QuickInsertCategoryResult&);
  QuickInsertCategoryResult(QuickInsertCategoryResult&&);
  QuickInsertCategoryResult& operator=(QuickInsertCategoryResult&&);
  ~QuickInsertCategoryResult();

  bool operator==(const QuickInsertCategoryResult&) const;
};

struct ASH_EXPORT QuickInsertEditorResult {
  enum class Mode { kWrite, kRewrite };

  Mode mode;
  std::u16string display_name;
  std::optional<chromeos::editor_menu::PresetQueryCategory> category;
  std::optional<std::string> preset_query_id;

  QuickInsertEditorResult(
      Mode mode,
      std::u16string display_name,
      std::optional<chromeos::editor_menu::PresetQueryCategory> category,
      std::optional<std::string> preset_query_id);
  QuickInsertEditorResult(const QuickInsertEditorResult&);
  QuickInsertEditorResult& operator=(const QuickInsertEditorResult&);
  QuickInsertEditorResult(QuickInsertEditorResult&&);
  QuickInsertEditorResult& operator=(QuickInsertEditorResult&&);
  ~QuickInsertEditorResult();

  bool operator==(const QuickInsertEditorResult&) const;
};

struct ASH_EXPORT QuickInsertLobsterResult {
  enum class Mode { kNoSelection, kWithSelection };

  Mode mode;
  std::u16string display_name;

  QuickInsertLobsterResult(Mode mode, std::u16string display_name);
  QuickInsertLobsterResult(const QuickInsertLobsterResult&);
  QuickInsertLobsterResult& operator=(const QuickInsertLobsterResult&);
  QuickInsertLobsterResult(QuickInsertLobsterResult&&);
  QuickInsertLobsterResult& operator=(QuickInsertLobsterResult&&);
  ~QuickInsertLobsterResult();

  bool operator==(const QuickInsertLobsterResult&) const;
};

struct ASH_EXPORT QuickInsertNewWindowResult {
  enum Type {
    kDoc,
    kSheet,
    kSlide,
    kChrome,
  };

  Type type;

  explicit QuickInsertNewWindowResult(Type type);
  QuickInsertNewWindowResult(const QuickInsertNewWindowResult&);
  QuickInsertNewWindowResult& operator=(const QuickInsertNewWindowResult&);
  QuickInsertNewWindowResult(QuickInsertNewWindowResult&&);
  QuickInsertNewWindowResult& operator=(QuickInsertNewWindowResult&&);
  ~QuickInsertNewWindowResult();

  bool operator==(const QuickInsertNewWindowResult&) const;
};

struct ASH_EXPORT QuickInsertCapsLockResult {
  enum class Shortcut {
    kAltLauncher,
    kAltSearch,
    kFnRightAlt,
  };

  bool enabled;
  Shortcut shortcut;

  explicit QuickInsertCapsLockResult(bool enabled, Shortcut shortcut);
  QuickInsertCapsLockResult(const QuickInsertCapsLockResult&);
  QuickInsertCapsLockResult& operator=(const QuickInsertCapsLockResult&);
  QuickInsertCapsLockResult(QuickInsertCapsLockResult&&);
  QuickInsertCapsLockResult& operator=(QuickInsertCapsLockResult&&);
  ~QuickInsertCapsLockResult();

  bool operator==(const QuickInsertCapsLockResult&) const;
};

struct ASH_EXPORT QuickInsertCaseTransformResult {
  enum Type {
    kUpperCase,
    kLowerCase,
    kTitleCase,
  };

  Type type;

  explicit QuickInsertCaseTransformResult(Type type);
  QuickInsertCaseTransformResult(const QuickInsertCaseTransformResult&);
  QuickInsertCaseTransformResult& operator=(
      const QuickInsertCaseTransformResult&);
  QuickInsertCaseTransformResult(QuickInsertCaseTransformResult&&);
  QuickInsertCaseTransformResult& operator=(QuickInsertCaseTransformResult&&);
  ~QuickInsertCaseTransformResult();

  bool operator==(const QuickInsertCaseTransformResult&) const;
};

using QuickInsertSearchResult = std::variant<QuickInsertTextResult,
                                             QuickInsertSearchRequestResult,
                                             QuickInsertEmojiResult,
                                             QuickInsertGifResult,
                                             QuickInsertClipboardResult,
                                             QuickInsertBrowsingHistoryResult,
                                             QuickInsertLocalFileResult,
                                             QuickInsertDriveFileResult,
                                             QuickInsertCategoryResult,
                                             QuickInsertEditorResult,
                                             QuickInsertLobsterResult,
                                             QuickInsertNewWindowResult,
                                             QuickInsertCapsLockResult,
                                             QuickInsertCaseTransformResult>;

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_SEARCH_RESULT_H_
