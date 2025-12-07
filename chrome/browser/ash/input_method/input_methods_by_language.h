// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_

#include <string_view>

#include "base/containers/span.h"

namespace ash::input_method {

enum class LanguageCategory {
  kAfrikaans,
  kDanish,
  kDutch,
  kFinnish,
  kEnglish,
  kFrench,
  kGerman,
  kItalian,
  kJapanese,
  kNorwegian,
  kPolish,
  kPortugese,
  kSpanish,
  kSwedish,
  kOther,
};

base::span<const std::string_view> AfrikaansInputMethods();

base::span<const std::string_view> DanishInputMethods();

base::span<const std::string_view> DutchInputMethods();

base::span<const std::string_view> FinnishInputMethods();

base::span<const std::string_view> EnglishInputMethods();

base::span<const std::string_view> FrenchInputMethods();

base::span<const std::string_view> GermanInputMethods();

base::span<const std::string_view> ItalianInputMethods();

base::span<const std::string_view> JapaneseInputMethods();

base::span<const std::string_view> NorwegianInputMethods();

base::span<const std::string_view> PolishInputMethods();

base::span<const std::string_view> PortugeseInputMethods();

base::span<const std::string_view> SpanishInputMethods();

base::span<const std::string_view> SwedishInputMethods();

LanguageCategory InputMethodToLanguageCategory(std::string_view input_method);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_
