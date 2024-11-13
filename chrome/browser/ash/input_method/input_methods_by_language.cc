// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_methods_by_language.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/span.h"

namespace ash::input_method {

base::span<const std::string_view> AfrikaansInputMethods() {
  // Afrikaans does not have a separate IME.
  return {};
}

base::span<const std::string_view> DanishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:dk::dan",  // Danish
  });
  return kInputMethods;
}

base::span<const std::string_view> DutchInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:be::nld",         // Dutch (Belgium)
      "xkb:us:intl_pc:nld",  // Dutch (Netherlands) with US intl pc keyboard
      "xkb:us:intl:nld",     // Dutch (Netherlands)
  });
  return kInputMethods;
}

base::span<const std::string_view> FinnishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:fi::fin",  // Finnish
  });
  return kInputMethods;
}

base::span<const std::string_view> EnglishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
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
  return kInputMethods;
}

base::span<const std::string_view> FrenchInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:be::fra",        // French (Belgium)
      "xkb:ca::fra",        // French (Canada)
      "xkb:ca:multix:fra",  // French (Canada) with multilingual keyboard
      "xkb:fr::fra",        // French (France)
      "xkb:fr:bepo:fra",    // French (France) with bepo keyboard
      "xkb:ch:fr:fra",      // French (Switzerland)
  });
  return kInputMethods;
}

base::span<const std::string_view> GermanInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:be::ger",     // German (Belgium)
      "xkb:de::ger",     // German (Germany)
      "xkb:de:neo:ger",  // German (Germany) with neo keyboard
      "xkb:ch::ger",     // German (Switzerland)
  });
  return kInputMethods;
}

base::span<const std::string_view> ItalianInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:it::ita",  // Italian
  });
  return kInputMethods;
}

base::span<const std::string_view> JapaneseInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:jp::jpn",   // Alphanumeric with Japanese keyboard
      "nacl_mozc_us",  // Japanese with US keyboard
      "nacl_mozc_jp",  // Japanese
  });
  return kInputMethods;
}

base::span<const std::string_view> NorwegianInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:no::nob",  // Norwegian
  });
  return kInputMethods;
}

base::span<const std::string_view> PolishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:pl::pol",  // Polish
  });
  return kInputMethods;
}

base::span<const std::string_view> PortugeseInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:br::por",         // Portugese (Brazil)
      "xkb:pt::por",         // Portugese (Portugal)
      "xkb:us:intl_pc:por",  // Portugese with US intl pc keyboard
      "xkb:us:intl:por",     // Portugese with US intl keyboard
  });
  return kInputMethods;
}

base::span<const std::string_view> SpanishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:latam::spa",  // Spanish (Latin America)
      "xkb:es::spa",     // Spanish (Spain)
  });
  return kInputMethods;
}

base::span<const std::string_view> SwedishInputMethods() {
  static constexpr auto kInputMethods = std::to_array<std::string_view>({
      "xkb:se::swe",  // Swedish
  });
  return kInputMethods;
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
