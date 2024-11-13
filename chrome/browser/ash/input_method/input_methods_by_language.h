// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_

#include <string>
#include <vector>

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

const std::vector<std::string>& AfrikaansInputMethods();

const std::vector<std::string>& DanishInputMethods();

const std::vector<std::string>& DutchInputMethods();

const std::vector<std::string>& FinnishInputMethods();

const std::vector<std::string>& EnglishInputMethods();

const std::vector<std::string>& FrenchInputMethods();

const std::vector<std::string>& GermanInputMethods();

const std::vector<std::string>& ItalianInputMethods();

const std::vector<std::string>& JapaneseInputMethods();

const std::vector<std::string>& NorwegianInputMethods();

const std::vector<std::string>& PolishInputMethods();

const std::vector<std::string>& PortugeseInputMethods();

const std::vector<std::string>& SpanishInputMethods();

const std::vector<std::string>& SwedishInputMethods();

LanguageCategory InputMethodToLanguageCategory(std::string_view input_method);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHODS_BY_LANGUAGE_H_
