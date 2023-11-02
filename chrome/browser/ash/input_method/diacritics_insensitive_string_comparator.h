// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_INSENSITIVE_STRING_COMPARATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_INSENSITIVE_STRING_COMPARATOR_H_

#include <string>

#include "third_party/icu/source/i18n/unicode/translit.h"

namespace ash {
namespace input_method {

class DiacriticsInsensitiveStringComparator {
 public:
  DiacriticsInsensitiveStringComparator();
  ~DiacriticsInsensitiveStringComparator();

  DiacriticsInsensitiveStringComparator(
      const DiacriticsInsensitiveStringComparator&) = delete;
  DiacriticsInsensitiveStringComparator& operator=(
      const DiacriticsInsensitiveStringComparator&) = delete;

  bool Equal(const std::u16string& a, const std::u16string& b) const;

 private:
  std::unique_ptr<icu::Transliterator> diacritics_stripper_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_INSENSITIVE_STRING_COMPARATOR_H_
