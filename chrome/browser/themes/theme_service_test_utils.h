// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_

#include <ostream>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace theme_service::test {

// Struct to distinguish SkColor (aliased to uint32_t) for printing.
struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const;
  bool operator!=(const PrintableSkColor& other) const;

  const SkColor color;
};

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color);

std::string ColorIdToString(ui::ColorId id);

}  // namespace theme_service::test

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_TEST_UTILS_H_
