// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace chromeos::editor_menu {
enum class PresetQueryCategory;
}

namespace ash {

// Represents a search result, which might be text or other types of media.
// TODO(b/310088338): Support result types beyond just literal text and gifs.
class ASH_PUBLIC_EXPORT PickerSearchResult {
 public:
  struct TextData {
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

    TextData(std::u16string primary_text,
             std::u16string secondary_text,
             ui::ImageModel icon,
             Source source);

    TextData(const TextData&);
    TextData& operator=(const TextData&);
    ~TextData();

    bool operator==(const TextData&) const;
  };

  struct SearchRequestData {
    std::u16string primary_text;
    std::u16string secondary_text;
    ui::ImageModel icon;

    bool operator==(const SearchRequestData&) const;
  };

  struct EmojiData {
    enum class Type { kEmoji, kSymbol, kEmoticon };

    Type type;
    std::u16string text;
    std::u16string name;

    bool operator==(const EmojiData&) const;
  };

  struct ClipboardData {
    enum class DisplayFormat {
      kFile,
      kText,
      kImage,
      kHtml,
      kUrl,
    };

    // Unique ID that specifies which item in the clipboard this refers to.
    base::UnguessableToken item_id;
    DisplayFormat display_format;
    // If this is 1, `display_text` should be the filename of the file.
    size_t file_count;
    std::u16string display_text;
    std::optional<ui::ImageModel> display_image;
    bool is_recent;

    ClipboardData(base::UnguessableToken item_id,
                  DisplayFormat display_format,
                  size_t file_count,
                  std::u16string display_text,
                  std::optional<ui::ImageModel> display_image,
                  bool is_recent);
    ClipboardData(const ClipboardData&);
    ClipboardData& operator=(const ClipboardData&);
    ~ClipboardData();

    bool operator==(const ClipboardData&) const;
  };

  struct BrowsingHistoryData {
    GURL url;
    std::u16string title;
    ui::ImageModel icon;
    bool best_match;

    BrowsingHistoryData(GURL url,
                        std::u16string title,
                        ui::ImageModel icon,
                        bool best_match);
    BrowsingHistoryData(const BrowsingHistoryData&);
    BrowsingHistoryData& operator=(const BrowsingHistoryData&);
    ~BrowsingHistoryData();

    bool operator==(const BrowsingHistoryData&) const;
  };

  struct LocalFileData {
    base::FilePath file_path;
    std::u16string title;
    bool best_match;

    bool operator==(const LocalFileData&) const;
  };

  struct DriveFileData {
    std::optional<std::string> id;
    std::u16string title;
    GURL url;
    base::FilePath file_path;
    bool best_match;

    DriveFileData(std::optional<std::string> id,
                  std::u16string title,
                  GURL url,
                  base::FilePath file_path,
                  bool best_match);
    DriveFileData(const DriveFileData&);
    DriveFileData& operator=(const DriveFileData&);
    ~DriveFileData();

    bool operator==(const DriveFileData&) const;
  };

  struct CategoryData {
    PickerCategory category;

    bool operator==(const CategoryData&) const;
  };

  struct EditorData {
    enum class Mode { kWrite, kRewrite };

    Mode mode;
    std::u16string display_name;
    std::optional<chromeos::editor_menu::PresetQueryCategory> category;
    std::optional<std::string> preset_query_id;

    EditorData(
        Mode mode,
        std::u16string display_name,
        std::optional<chromeos::editor_menu::PresetQueryCategory> category,
        std::optional<std::string> preset_query_id);
    EditorData(const EditorData&);
    EditorData& operator=(const EditorData&);
    ~EditorData();

    bool operator==(const EditorData&) const;
  };

  struct NewWindowData {
    enum Type {
      kDoc,
      kSheet,
      kSlide,
      kChrome,
    };

    Type type;

    bool operator==(const NewWindowData&) const;
  };

  struct CapsLockData {
    enum class Shortcut {
      kAltLauncher,
      kAltSearch,
      kFnRightAlt,
    };

    bool enabled;
    Shortcut shortcut;

    bool operator==(const CapsLockData&) const;
  };

  struct CaseTransformData {
    enum Type {
      kUpperCase,
      kLowerCase,
      kTitleCase,
    };

    Type type;

    bool operator==(const CaseTransformData&) const;
  };

  using Data = std::variant<TextData,
                            SearchRequestData,
                            EmojiData,
                            ClipboardData,
                            BrowsingHistoryData,
                            LocalFileData,
                            DriveFileData,
                            CategoryData,
                            EditorData,
                            NewWindowData,
                            CapsLockData,
                            CaseTransformData>;

  PickerSearchResult(const PickerSearchResult&);
  PickerSearchResult& operator=(const PickerSearchResult&);
  PickerSearchResult(PickerSearchResult&&);
  PickerSearchResult& operator=(PickerSearchResult&&);
  ~PickerSearchResult();

  static PickerSearchResult BrowsingHistory(const GURL& url,
                                            std::u16string title,
                                            ui::ImageModel icon,
                                            bool best_match = false);
  static PickerSearchResult Text(
      std::u16string_view text,
      TextData::Source source = TextData::Source::kUnknown);
  static PickerSearchResult Text(
      std::u16string_view primary_text,
      std::u16string_view secondary_text,
      ui::ImageModel icon,
      TextData::Source source = TextData::Source::kUnknown);
  static PickerSearchResult SearchRequest(std::u16string_view primary_text,
                                          std::u16string_view secondary_text,
                                          ui::ImageModel icon);
  static PickerSearchResult Emoji(std::u16string_view emoji,
                                  std::u16string name = u"");
  static PickerSearchResult Symbol(std::u16string_view symbol,
                                   std::u16string name = u"");
  static PickerSearchResult Emoticon(std::u16string_view emoticon,
                                     std::u16string name = u"");
  static PickerSearchResult Clipboard(
      base::UnguessableToken item_id,
      ClipboardData::DisplayFormat display_format,
      size_t file_count,
      std::u16string display_text,
      std::optional<ui::ImageModel> display_image,
      bool is_recent);
  static PickerSearchResult LocalFile(std::u16string title,
                                      base::FilePath file_path,
                                      bool best_match = false);
  static PickerSearchResult DriveFile(std::optional<std::string> id,
                                      std::u16string title,
                                      const GURL& url,
                                      base::FilePath file_path,
                                      bool best_match = false);
  static PickerSearchResult Category(PickerCategory category);
  static PickerSearchResult Editor(
      EditorData::Mode mode,
      std::u16string display_name,
      std::optional<chromeos::editor_menu::PresetQueryCategory> category,
      std::optional<std::string> preset_query_id);
  static PickerSearchResult NewWindow(NewWindowData::Type type);
  static PickerSearchResult CapsLock(bool enabled,
                                     CapsLockData::Shortcut shortcut);
  static PickerSearchResult CaseTransform(CaseTransformData::Type type);

  const Data& data() const;

  bool operator==(const PickerSearchResult&) const;

 private:
  explicit PickerSearchResult(Data data);

  Data data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
