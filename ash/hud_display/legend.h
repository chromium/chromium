// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_LEGEND_H_
#define ASH_HUD_DISPLAY_LEGEND_H_

#include <string>
#include <vector>

#include "base/callback.h"

#include "ui/views/view.h"

namespace ash {
namespace hud_display {

class Graph;

// Draws legend view.
class Legend : public views::View {
 public:
  using Formatter = base::RepeatingCallback<std::u16string(float)>;

  METADATA_HEADER(Legend);

  struct Entry {
    Entry(const Graph& graph,
          std::u16string label,
          std::u16string tooltip,
          Formatter formatter);
    Entry(const Entry&);
    ~Entry();

    const Graph& graph;
    std::u16string label;
    std::u16string tooltip;
    Formatter formatter;  // formatting function
  };

  explicit Legend(const std::vector<Entry>& contents);

  Legend(const Legend&) = delete;
  Legend& operator=(const Legend&) = delete;

  ~Legend() override;

  // views::View:
  void Layout() override;

  // Display values for the given index. |index| is always interpreted as
  // "negative", i.e. "0" - current data, "1" - previous graph data, 2 - two
  // steps ago. I.e. it's number of graph points from the right graph edge.
  void SetValuesIndex(size_t index);

  // Update displayed values after data was updated.
  void RefreshValues();
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_LEGEND_H_
