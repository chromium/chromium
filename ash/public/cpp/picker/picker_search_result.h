// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
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
    std::u16string text;
    ui::ImageModel icon;

    bool operator==(const SearchRequestData&) const;
  };

  struct EmojiData {
    std::u16string emoji;

    bool operator==(const EmojiData&) const;
  };

  struct SymbolData {
    std::u16string symbol;

    bool operator==(const SymbolData&) const;
  };

  struct EmoticonData {
    std::u16string emoticon;

    bool operator==(const EmoticonData&) const;
  };

  struct ClipboardData {
    enum class DisplayFormat {
      kFile,
      kText,
      kImage,
      kHtml,
    };

    // Unique ID that specifies which item in the clipboard this refers to.
    base::UnguessableToken item_id;
    DisplayFormat display_format;
    std::u16string display_text;
    std::optional<ui::ImageModel> display_image;

    ClipboardData(base::UnguessableToken item_id,
                  DisplayFormat display_format,
                  std::u16string display_text,
                  std::optional<ui::ImageModel> display_image);
    ClipboardData(const ClipboardData&);
    ClipboardData& operator=(const ClipboardData&);
    ~ClipboardData();

    bool operator==(const ClipboardData&) const;
  };

  struct GifData {
    GifData(const GURL& preview_url,
            const GURL& preview_image_url,
            const gfx::Size& preview_dimensions,
            const GURL& full_url,
            const gfx::Size& full_dimensions,
            std::u16string content_description);
    GifData(const GifData&);
    GifData& operator=(const GifData&);
    ~GifData();

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

    bool operator==(const GifData&) const;
  };

  struct BrowsingHistoryData {
    GURL url;
    std::u16string title;
    ui::ImageModel icon;

    bool operator==(const BrowsingHistoryData&) const;
  };

  struct LocalFileData {
    base::FilePath file_path;
    std::u16string title;

    bool operator==(const LocalFileData&) const;
  };

  struct DriveFileData {
    std::u16string title;
    GURL url;
    ui::ImageModel icon;

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
    std::optional<std::string> freeform_text;

    EditorData(
        Mode mode,
        std::u16string display_name,
        std::optional<chromeos::editor_menu::PresetQueryCategory> category,
        std::optional<std::string> preset_query_id,
        std::optional<std::string> freeform_text);
    EditorData(const EditorData&);
    EditorData& operator=(const EditorData&);
    ~EditorData();

    bool operator==(const EditorData&) const;
  };

  using Data = std::variant<TextData,
                            SearchRequestData,
                            EmojiData,
                            SymbolData,
                            EmoticonData,
                            ClipboardData,
                            GifData,
                            BrowsingHistoryData,
                            LocalFileData,
                            DriveFileData,
                            CategoryData,
                            EditorData>;

  PickerSearchResult(const PickerSearchResult&);
  PickerSearchResult& operator=(const PickerSearchResult&);
  PickerSearchResult(PickerSearchResult&&);
  PickerSearchResult& operator=(PickerSearchResult&&);
  ~PickerSearchResult();

  static PickerSearchResult BrowsingHistory(const GURL& url,
                                            std::u16string title,
                                            ui::ImageModel icon);
  static PickerSearchResult Text(
      std::u16string_view text,
      TextData::Source source = TextData::Source::kUnknown);
  static PickerSearchResult Text(
      std::u16string_view primary_text,
      std::u16string_view secondary_text,
      ui::ImageModel icon,
      TextData::Source source = TextData::Source::kUnknown);
  static PickerSearchResult SearchRequest(std::u16string_view text,
                                          ui::ImageModel icon);
  static PickerSearchResult Emoji(std::u16string_view emoji);
  static PickerSearchResult Symbol(std::u16string_view symbol);
  static PickerSearchResult Emoticon(std::u16string_view emoticon);
  static PickerSearchResult Clipboard(
      base::UnguessableToken item_id,
      ClipboardData::DisplayFormat display_format,
      std::u16string display_text,
      std::optional<ui::ImageModel> display_image);
  static PickerSearchResult Gif(const GURL& preview_url,
                                const GURL& preview_image_url,
                                const gfx::Size& preview_dimensions,
                                const GURL& full_url,
                                const gfx::Size& full_dimensions,
                                std::u16string content_description);
  static PickerSearchResult LocalFile(std::u16string title,
                                      base::FilePath file_path);
  static PickerSearchResult DriveFile(std::u16string title,
                                      const GURL& url,
                                      ui::ImageModel icon);
  static PickerSearchResult Category(PickerCategory category);
  static PickerSearchResult Editor(
      EditorData::Mode mode,
      std::u16string display_name,
      std::optional<chromeos::editor_menu::PresetQueryCategory> category,
      std::optional<std::string> preset_query_id,
      std::optional<std::string> freeform_text);

  const Data& data() const;

  bool operator==(const PickerSearchResult&) const;

 private:
  explicit PickerSearchResult(Data data);

  Data data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
