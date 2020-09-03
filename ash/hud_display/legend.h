// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_LEGEND_H_
#define ASH_HUD_DISPLAY_LEGEND_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace ash {
namespace hud_display {

// Draws legend view.
class Legend : public views::View {
 public:
  METADATA_HEADER(Legend);

  struct Entry {
    SkColor color;
    base::string16 label;
    base::string16 tooltip;
  };

  Legend(const std::vector<Entry>& contents);

  Legend(const Legend&) = delete;
  Legend& operator=(const Legend&) = delete;

  ~Legend() override;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_LEGEND_H_
