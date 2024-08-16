// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_methods_by_language.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"

namespace ash::input_method {

const std::vector<std::string>& AfrikaansInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      // Afrikaans does not have a separate IME.
  });
  return *input_methods;
}

const std::vector<std::string>& DanishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:dk::dan",  // Danish
  });
  return *input_methods;
}

const std::vector<std::string>& DutchInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:be::nld",         // Dutch (Belgium)
      "xkb:us:intl_pc:nld",  // Dutch (Netherlands) with US intl pc keyboard
      "xkb:us:intl:nld",     // Dutch (Netherlands)
  });
  return *input_methods;
}

const std::vector<std::string>& FinnishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:fi::fin",  // Finnish
  });
  return *input_methods;
}

const std::vector<std::string>& EnglishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:ca:eng:eng",           // Canada
      "xkb:gb::eng",              // UK
      "xkb:gb:extd:eng",          // UK Extended
      "xkb:gb:dvorak:eng",        // UK Dvorak
      "xkb:in::eng",              // India
      "xkb:pk::eng",              // Pakistan
      "xkb:us:altgr-intl:eng",    // US Extended
      "xkb:us:colemak:eng",       // US Colemak
      "xkb:us:dvorak:eng",        // US Dvorak
      "xkb:us:dvp:eng",           // US Programmer Dvorak
      "xkb:us:intl_pc:eng",       // US Intl (PC)
      "xkb:us:intl:eng",          // US Intl
      "xkb:us:workman-intl:eng",  // US Workman Intl
      "xkb:us:workman:eng",       // US Workman
      "xkb:us::eng",              // US
      "xkb:za:gb:eng"             // South Africa
  });
  return *input_methods;
}

const std::vector<std::string>& FrenchInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:be::fra",        // French (Belgium)
      "xkb:ca::fra",        // French (Canada)
      "xkb:ca:multix:fra",  // French (Canada) with multilingual keyboard
      "xkb:fr::fra",        // French (France)
      "xkb:fr:bepo:fra",    // French (France) with bepo keyboard
      "xkb:ch:fr:fra",      // French (Switzerland)
  });
  return *input_methods;
}

const std::vector<std::string>& GermanInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:be::ger",     // German (Belgium)
      "xkb:de::ger",     // German (Germany)
      "xkb:de:neo:ger",  // German (Germany) with neo keyboard
      "xkb:ch::ger",     // German (Switzerland)
  });
  return *input_methods;
}

const std::vector<std::string>& ItalianInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:it::ita",  // Italian
  });
  return *input_methods;
}

const std::vector<std::string>& JapaneseInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:jp::jpn",   // Alphanumeric with Japanese keyboard
      "nacl_mozc_us",  // Japanese with US keyboard
      "nacl_mozc_jp",  // Japanese
  });
  return *input_methods;
}

const std::vector<std::string>& NorwegianInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:no::nob",  // Norwegian
  });
  return *input_methods;
}

const std::vector<std::string>& PolishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:pl::pol",  // Polish
  });
  return *input_methods;
}

const std::vector<std::string>& PortugeseInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:br::por",         // Portugese (Brazil)
      "xkb:pt::por",         // Portugese (Portugal)
      "xkb:us:intl_pc:por",  // Portugese with US intl pc keyboard
      "xkb:us:intl:por",     // Portugese with US intl keyboard
  });
  return *input_methods;
}

const std::vector<std::string>& SpanishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:latam::spa",  // Spanish (Latin America)
      "xkb:es::spa",     // Spanish (Spain)
  });
  return *input_methods;
}

const std::vector<std::string>& SwedishInputMethods() {
  static const base::NoDestructor<std::vector<std::string>> input_methods({
      "xkb:se::swe",  // Swedish
  });
  return *input_methods;
}

LanguageCategory InputMethodToLanguageCategory(std::string_view input_method) {
  if (base::Contains(AfrikaansInputMethods(), input_method)) {
    return LanguageCategory::kAfrikaans;
  }
  if (base::Contains(DanishInputMethods(), input_method)) {
    return LanguageCategory::kDanish;
  }
  if (base::Contains(DutchInputMethods(), input_method)) {
    return LanguageCategory::kDutch;
  }
  if (base::Contains(EnglishInputMethods(), input_method)) {
    return LanguageCategory::kEnglish;
  }
  if (base::Contains(FinnishInputMethods(), input_method)) {
    return LanguageCategory::kFinnish;
  }
  if (base::Contains(FrenchInputMethods(), input_method)) {
    return LanguageCategory::kFrench;
  }
  if (base::Contains(GermanInputMethods(), input_method)) {
    return LanguageCategory::kGerman;
  }
  if (base::Contains(ItalianInputMethods(), input_method)) {
    return LanguageCategory::kItalian;
  }
  if (base::Contains(JapaneseInputMethods(), input_method)) {
    return LanguageCategory::kJapanese;
  }
  if (base::Contains(NorwegianInputMethods(), input_method)) {
    return LanguageCategory::kNorwegian;
  }
  if (base::Contains(PolishInputMethods(), input_method)) {
    return LanguageCategory::kPolish;
  }
  if (base::Contains(PortugeseInputMethods(), input_method)) {
    return LanguageCategory::kPortugese;
  }
  if (base::Contains(SpanishInputMethods(), input_method)) {
    return LanguageCategory::kSpanish;
  }
  if (base::Contains(SwedishInputMethods(), input_method)) {
    return LanguageCategory::kSwedish;
  }
  return LanguageCategory::kOther;
}

}  // namespace ash::input_method
