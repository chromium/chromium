// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_AREA_OF_INTEREST_H_
#define CHROME_BROWSER_SELECTION_AREA_OF_INTEREST_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {

// Selection-specific area of interest within a page.
struct AreaOfInterest {
  AreaOfInterest();
  AreaOfInterest(const AreaOfInterest&);
  AreaOfInterest& operator=(const AreaOfInterest&);
  AreaOfInterest(AreaOfInterest&&);
  AreaOfInterest& operator=(AreaOfInterest&&);
  ~AreaOfInterest();

  std::variant<gfx::Rect, std::vector<gfx::Point>> bounds;
  std::optional<std::u16string> selected_text;
  std::optional<std::u16string> text_surrounding_selection;
  // Whether `bounds` corresponds to a text selection rather than a manually
  // drawn spatial region.
  bool is_text_selection = false;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_AREA_OF_INTEREST_H_
