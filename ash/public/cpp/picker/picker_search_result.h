// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_

#include <string>
#include <string_view>
#include <variant>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

// Represents a search result, which might be text or other types of media.
// TODO(b/310088338): Support result types beyond just literal text and gifs.
class ASH_PUBLIC_EXPORT PickerSearchResult {
 public:
  struct TextData {
    std::u16string text;

    bool operator==(const TextData&) const;
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

  struct GifData {
    GifData(const GURL& url,
            const GURL& preview_image_url,
            const gfx::Size& dimensions,
            std::u16string content_description);
    GifData(const GifData&);
    GifData& operator=(const GifData&);
    ~GifData();

    // A url to the gif media source.
    GURL url;

    // A url to a preview image of the gif media source.
    GURL preview_image_url;

    // Width and height of the GIF at `url`.
    gfx::Size dimensions;

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

  using Data = std::variant<TextData,
                            EmojiData,
                            SymbolData,
                            EmoticonData,
                            GifData,
                            BrowsingHistoryData>;

  PickerSearchResult(const PickerSearchResult&);
  PickerSearchResult& operator=(const PickerSearchResult&);
  ~PickerSearchResult();

  static PickerSearchResult BrowsingHistory(const GURL& url,
                                            std::u16string title,
                                            ui::ImageModel icon);
  static PickerSearchResult Text(std::u16string_view text);
  static PickerSearchResult Emoji(std::u16string_view emoji);
  static PickerSearchResult Symbol(std::u16string_view symbol);
  static PickerSearchResult Emoticon(std::u16string_view emoticon);
  static PickerSearchResult Gif(const GURL& url,
                                const GURL& preview_image_url,
                                const gfx::Size& dimensions,
                                std::u16string content_description);

  const Data& data() const;

  bool operator==(const PickerSearchResult&) const;

 private:
  explicit PickerSearchResult(Data data);

  Data data_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PICKER_PICKER_SEARCH_RESULT_H_
