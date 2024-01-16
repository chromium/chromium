// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_search_results.h"

namespace ash {

bool PickerSearchResult::TextData::operator==(
    const PickerSearchResult::TextData&) const = default;

bool PickerSearchResult::GifData::operator==(
    const PickerSearchResult::GifData&) const = default;

PickerSearchResult::~PickerSearchResult() = default;

PickerSearchResult::PickerSearchResult(const PickerSearchResult&) = default;

PickerSearchResult& PickerSearchResult::operator=(const PickerSearchResult&) =
    default;

PickerSearchResult PickerSearchResult::Text(std::u16string_view text) {
  return PickerSearchResult(TextData{.text = std::u16string(text)});
}

PickerSearchResult PickerSearchResult::Gif(const GURL& url) {
  return PickerSearchResult(GifData{.url = url});
}

bool PickerSearchResult::operator==(const PickerSearchResult&) const = default;

const PickerSearchResult::Data& PickerSearchResult::data() const {
  return data_;
}

PickerSearchResult::PickerSearchResult(Data data) : data_(std::move(data)) {}

PickerSearchResults::Section::Section(
    const std::u16string& heading,
    base::span<const PickerSearchResult> results)
    : heading_(heading), results_(results.begin(), results.end()) {}

PickerSearchResults::Section::Section(const Section& other) = default;

PickerSearchResults::Section& PickerSearchResults::Section::operator=(
    const Section& other) = default;

PickerSearchResults::Section::~Section() = default;

const std::u16string& PickerSearchResults::Section::heading() const {
  return heading_;
}

base::span<const PickerSearchResult> PickerSearchResults::Section::results()
    const {
  return results_;
}

PickerSearchResults::PickerSearchResults(base::span<const Section> sections)
    : sections_(sections.begin(), sections.end()) {}

PickerSearchResults::PickerSearchResults(const PickerSearchResults& other) =
    default;

PickerSearchResults& PickerSearchResults::operator=(
    const PickerSearchResults& other) = default;

PickerSearchResults::~PickerSearchResults() = default;

base::span<const PickerSearchResults::Section> PickerSearchResults::sections()
    const {
  return sections_;
}

}  // namespace ash
