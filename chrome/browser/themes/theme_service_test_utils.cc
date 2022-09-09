// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_test_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
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

std::string ColorIdToString(ui::ColorId id) {
#define E_CPONLY(color_id, ...) {color_id, #color_id},

  static constexpr const auto kMap =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});

#undef E_CPONLY
  return kMap.find(id)->second;
}

}  // namespace theme_service::test
