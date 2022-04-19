// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_test_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace theme_service::test {

// Struct to distinguish SkColor (aliased to uint32_t) for printing.
bool PrintableSkColor::operator==(const PrintableSkColor& other) const {
  return color == other.color;
}

bool PrintableSkColor::operator!=(const PrintableSkColor& other) const {
  return !operator==(other);
}

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color) {
  SkColor color = printable_color.color;
  return os << base::StringPrintf("#%02x%02x%02x%02x", SkColorGetA(color),
                                  SkColorGetR(color), SkColorGetG(color),
                                  SkColorGetB(color));
}

std::string ColorIdToString(int id) {
#define E(color_id, theme_property_id, ...) \
  {theme_property_id, #theme_property_id},
#define E_CPONLY(color_id, ...)

  static constexpr const auto kMap =
      base::MakeFixedFlatMap<int, const char*>({CHROME_COLOR_IDS});

#undef E
#undef E_CPONLY
  constexpr char kPrefix[] = "ThemeProperties::";

  std::string id_str = kMap.find(id)->second;
  return base::StartsWith(id_str, kPrefix) ? id_str.substr(strlen(kPrefix))
                                           : id_str;
}

std::pair<PrintableSkColor, PrintableSkColor> GetOriginalAndRedirected(
    const ui::ThemeProvider& theme_provider,
    int color_id) {
  PrintableSkColor original{theme_provider.GetColor(color_id)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kColorProviderRedirectionForThemeProvider);
  PrintableSkColor redirected{theme_provider.GetColor(color_id)};

  return {original, redirected};
}

void TestOriginalAndRedirectedColorMatched(
    const ui::ThemeProvider& theme_provider,
    int color_id,
    const std::string& error_message) {
  auto get_tolerance = [](int id) {
    auto* it = kColorTolerances.find(id);
    if (it != kColorTolerances.end())
      return it->second;
    return 0;
  };

  // Verifies that colors with and without the ColorProvider are the same.
  auto [original, redirected] =
      GetOriginalAndRedirected(theme_provider, color_id);
  auto tolerance = get_tolerance(color_id);
  if (!tolerance) {
    EXPECT_EQ(original, redirected) << error_message;
  } else {
    EXPECT_LE(std::abs(static_cast<int>(SkColorGetA(original.color) -
                                        SkColorGetA(redirected.color))),
              tolerance)
        << error_message;
    EXPECT_LE(std::abs(static_cast<int>(SkColorGetR(original.color) -
                                        SkColorGetR(redirected.color))),
              tolerance)
        << error_message;
    EXPECT_LE(std::abs(static_cast<int>(SkColorGetG(original.color) -
                                        SkColorGetG(redirected.color))),
              tolerance)
        << error_message;
    EXPECT_LE(std::abs(static_cast<int>(SkColorGetB(original.color) -
                                        SkColorGetB(redirected.color))),
              tolerance)
        << error_message;
  }
}

}  // namespace theme_service::test
