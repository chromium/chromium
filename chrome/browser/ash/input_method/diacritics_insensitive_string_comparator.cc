// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace ash {
namespace input_method {

DiacriticsInsensitiveStringComparator::DiacriticsInsensitiveStringComparator() {
  // Intentionally only covering Latin-script accented letters likely found in
  // French, Spanish, Dutch, Swedish, Norwegian, Danish, and Catalan.
  diacritics_stripper_ = base::i18n::CreateTransliteratorFromRules(
      "DiacriticStripper",
      "::NFC; "
      "[ á à â ä ā å ] > a; "
      "[ Á À Â Ä Ā Å ] > A; "
      "[ é è ê ë ē   ] > e; "
      "[ É È Ê Ë Ē   ] > E; "
      "[ í ì î ï ī   ] > i; "
      "[ Í Ì Î Ï Ī   ] > I; "
      "[ ó ò ô ö ō ø ] > o; "
      "[ Ó Ò Ô Ö Ō Ø ] > O; "
      "[ ú ù û ü ū   ] > u; "
      "[ Ú Ù Û Ü Ū   ] > U; "
      "[ ý ỳ ŷ ÿ ȳ   ] > y; "
      "[ Ý Ỳ Ŷ Ÿ Ȳ   ] > Y; "
      "ç > c; ñ > n; æ > ae; œ > oe; "
      "Ç > C; Ñ > N; Æ > AE; Œ > OE; ");
}

DiacriticsInsensitiveStringComparator::
    ~DiacriticsInsensitiveStringComparator() = default;

bool DiacriticsInsensitiveStringComparator::Equal(
    const std::u16string& a,
    const std::u16string& b) const {
  std::u16string result_a = diacritics_stripper_->Transliterate(a);
  std::u16string result_b = diacritics_stripper_->Transliterate(b);

  return result_a.compare(result_b) == 0;
}

}  // namespace input_method
}  // namespace ash
