// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code.h"

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

LanguageCode::LanguageCode(std::string_view code) : code_(code) {
  CHECK(code.size() >= 2);
}

}  // namespace base
