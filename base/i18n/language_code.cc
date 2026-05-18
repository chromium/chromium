// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"

namespace base {

LanguageCode::~LanguageCode() = default;
LanguageCode::LanguageCode(const LanguageCode&) = default;
LanguageCode& LanguageCode::operator=(const LanguageCode&) = default;

std::string LanguageCode::ToLegacyICUFormat() const {
  std::string code(ToString());
  base::ReplaceSubstringsAfterOffset(&code, 0, "-", "_");
  return code;
}

std::string_view LanguageCode::ToString() const {
  return code_.AsString();
}

LanguageCode::LanguageCode(ImmutableStringType code) : code_(std::move(code)) {
  CHECK(code_.AsString().size() >= 2);
}

RegionCode::RegionCode(std::string_view code)
    : size_(base::checked_cast<uint8_t>(code.size())) {
  CHECK_LE(code.size(), 3u);
  std::copy(code.begin(), code.end(), code_.begin());
}

std::string_view RegionCode::ToString() const {
  return std::string_view(code_.data(), static_cast<size_t>(size_));
}

std::optional<RegionCode> LanguageCode::GetRegionCode() const {
  std::string_view code = code_.AsString();
  // Region codes are at least 2 chars, and language is at least 2.
  // "en-US" is 5 chars.
  if (code.size() < 3) {
    return std::nullopt;
  }

  size_t first_hyphen = code.find('-');
  if (first_hyphen == std::string_view::npos) {
    return std::nullopt;
  }

  size_t second_hyphen = code.find('-', first_hyphen + 1);

  size_t second_subtag_len;
  if (second_hyphen == std::string_view::npos) {
    second_subtag_len = code.size() - first_hyphen - 1;
  } else {
    second_subtag_len = second_hyphen - first_hyphen - 1;
  }

  // BCP47 subtag rules:
  // - Language: 2-3 characters (at the start).
  // - Script: Exactly 4 characters (e.g., "Latn", "Hant").
  // - Region: 2 characters (ISO 3166-1 alpha-2) or 3 digits (UN M.49).
  // - Extension: 1 character prefix (e.g., "u-", "x-").
  // - Variant: 5-8 characters (or 4 if it starts with a digit).

  // Check if the second subtag is a region code (length 2 or 3).
  // This effectively skips single-character extensions and 4-character scripts.
  if (second_subtag_len >= 2 && second_subtag_len <= 3) {
    return RegionCode(code.substr(first_hyphen + 1, second_subtag_len));
  }

  // If the second subtag was not a region, it might be a script (length 4).
  // If so, the region could be the third subtag.
  if (second_subtag_len != 4 || second_hyphen == std::string_view::npos) {
    return std::nullopt;
  }

  // Extract the third subtag and check if it's a region (length 2 or 3).
  size_t third_hyphen = code.find('-', second_hyphen + 1);
  size_t third_subtag_len;
  if (third_hyphen == std::string_view::npos) {
    third_subtag_len = code.size() - second_hyphen - 1;
  } else {
    third_subtag_len = third_hyphen - second_hyphen - 1;
  }

  if (third_subtag_len >= 2 && third_subtag_len <= 3) {
    return RegionCode(code.substr(second_hyphen + 1, third_subtag_len));
  }

  return std::nullopt;
}

}  // namespace base
