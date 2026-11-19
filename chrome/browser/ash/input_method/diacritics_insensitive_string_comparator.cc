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
  // Rules live in
  // `//third_party/rust/icu4x_data/transliterators/diacritic_stripper.txt` and
  // `//base/i18n/transliterator.cc`.
  diacritics_stripper_ =
      base::i18n::CreateTransliterator("und-t-und-x0-diacstr");
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
