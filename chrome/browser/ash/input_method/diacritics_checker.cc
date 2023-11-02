// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/diacritics_checker.h"

#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Intentionally only covering Latin-script accented letters likely found in
// French, Spanish, Dutch, Swedish, Norwegian, Danish, and Catalan.
constexpr char HAS_DIACRITICS_REGEX[] =
    "["
    "áàâäāåÁÀÂÄĀÅ"
    "éèêëēÉÈÊËĒ"
    "íìîïīÍÌÎÏĪ"
    "óòôöōøÓÒÔÖŌØ"
    "úùûüūÚÙÛÜŪ"
    "ýỳŷÿȳÝỲŶŸȲ"
    "çñæœÇÑÆŒ"
    "]";

}  // namespace

namespace ash {
namespace input_method {

bool HasDiacritics(const std::u16string& text) {
  if (text.empty()) {
    return false;
  }

  std::string text_utf8 = base::UTF16ToUTF8(text);
  return re2::RE2::PartialMatch(text_utf8, HAS_DIACRITICS_REGEX);
}

}  // namespace input_method
}  // namespace ash
