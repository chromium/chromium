// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/picker/picker_search_result.h"

#include <string>
#include <string_view>
#include <utility>

#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

bool PickerSearchResult::TextData::operator==(
    const PickerSearchResult::TextData&) const = default;

bool PickerSearchResult::EmojiData::operator==(
    const PickerSearchResult::EmojiData&) const = default;

bool PickerSearchResult::SymbolData::operator==(
    const PickerSearchResult::SymbolData&) const = default;

bool PickerSearchResult::EmoticonData::operator==(
    const PickerSearchResult::EmoticonData&) const = default;

PickerSearchResult::GifData::GifData(const GURL& url,
                                     const GURL& preview_image_url,
                                     const gfx::Size& dimensions,
                                     std::u16string content_description)
    : url(url),
      preview_image_url(preview_image_url),
      dimensions(dimensions),
      content_description(std::move(content_description)) {}

PickerSearchResult::GifData::GifData(const PickerSearchResult::GifData&) =
    default;

PickerSearchResult::GifData& PickerSearchResult::GifData::operator=(
    const PickerSearchResult::GifData&) = default;

PickerSearchResult::GifData::~GifData() = default;

bool PickerSearchResult::GifData::operator==(
    const PickerSearchResult::GifData&) const = default;

bool PickerSearchResult::BrowsingHistoryData::operator==(
    const PickerSearchResult::BrowsingHistoryData&) const = default;

PickerSearchResult::~PickerSearchResult() = default;

PickerSearchResult::PickerSearchResult(const PickerSearchResult&) = default;

PickerSearchResult& PickerSearchResult::operator=(const PickerSearchResult&) =
    default;

PickerSearchResult PickerSearchResult::Text(std::u16string_view text) {
  return PickerSearchResult(TextData{.text = std::u16string(text)});
}

PickerSearchResult PickerSearchResult::Emoji(std::u16string_view emoji) {
  return PickerSearchResult(EmojiData{.emoji = std::u16string(emoji)});
}

PickerSearchResult PickerSearchResult::Symbol(std::u16string_view symbol) {
  return PickerSearchResult(SymbolData{.symbol = std::u16string(symbol)});
}

PickerSearchResult PickerSearchResult::Emoticon(std::u16string_view emoticon) {
  return PickerSearchResult(EmoticonData{.emoticon = std::u16string(emoticon)});
}

PickerSearchResult PickerSearchResult::Gif(const GURL& url,
                                           const GURL& preview_image_url,
                                           const gfx::Size& dimensions,
                                           std::u16string content_description) {
  return PickerSearchResult(GifData(url, preview_image_url, dimensions,
                                    std::move(content_description)));
}

PickerSearchResult PickerSearchResult::BrowsingHistory(const GURL& url,
                                                       std::u16string title,
                                                       ui::ImageModel icon) {
  return PickerSearchResult(BrowsingHistoryData{
      .url = url, .title = std::move(title), .icon = std::move(icon)});
}

bool PickerSearchResult::operator==(const PickerSearchResult&) const = default;

const PickerSearchResult::Data& PickerSearchResult::data() const {
  return data_;
}

PickerSearchResult::PickerSearchResult(Data data) : data_(std::move(data)) {}

}  // namespace ash
