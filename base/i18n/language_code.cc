// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code.h"

#include "base/strings/string_util.h"

namespace base {

std::string LanguageCode::ToLegacyICUFormat() const {
  std::string code(ToString());
  base::ReplaceSubstringsAfterOffset(&code, 0, "-", "_");
  return code;
}

std::string_view LanguageCode::ToString() const {
  return std::string_view(code_.data(), length_);
}

}  // namespace base
