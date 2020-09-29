// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
#define ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/legend.h"
#include "base/sequence_checker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}

namespace ash {
namespace hud_display {

class Grid;
class Legend;

// Interface for a single graph page.
class GraphPageViewBase : public views::View, public views::ButtonListener {
 public:
  METADATA_HEADER(GraphPageViewBase);

  GraphPageViewBase();
  GraphPageViewBase(const GraphPageViewBase&) = delete;
  GraphPageViewBase& operator=(const GraphPageViewBase&) = delete;
  ~GraphPageViewBase() override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Update page data from the new snapshot.
  virtual void UpdateData(const DataSource::Snapshot& snapshot) = 0;

  // Adds default legend.
  void CreateLegend(const std::vector<Legend::Entry>& entries);

  // Put grid in its dedicated container.
  Grid* CreateGrid(float left,
                   float top,
                   float right,
                   float bottom,
                   const base::string16& x_unit,
                   const base::string16& y_unit,
                   int horizontal_points_number,
                   int horizontal_ticks_interval);

 protected:
  void RefreshLegendValues();

 private:
  // Container for the Grid object.
  views::View* grid_container_ = nullptr;  // not owned

  // Container for the legend object.
  views::View* legend_container_ = nullptr;              // not owned
  views::ImageButton* legend_min_max_button_ = nullptr;  // not owned
  Legend* legend_ = nullptr;                             // not owned

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
