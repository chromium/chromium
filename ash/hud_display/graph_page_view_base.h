// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
#define ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_

#include "ash/hud_display/data_source.h"
#include "ash/hud_display/legend.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}

namespace ash {
namespace hud_display {

class Legend;
class ReferenceLines;

// Interface for a single graph page.
class GraphPageViewBase : public views::View {
  METADATA_HEADER(GraphPageViewBase, views::View)

 public:
  GraphPageViewBase();
  GraphPageViewBase(const GraphPageViewBase&) = delete;
  GraphPageViewBase& operator=(const GraphPageViewBase&) = delete;
  ~GraphPageViewBase() override;

  // Update page data from the new snapshot.
  virtual void UpdateData(const DataSource::Snapshot& snapshot) = 0;

  // Adds default legend.
  void CreateLegend(const std::vector<Legend::Entry>& entries);

  // Put the |ReferenceLines| object in its dedicated container. See
  // |ReferenceLines| for details.
  ReferenceLines* CreateReferenceLines(float left,
                                       float top,
                                       float right,
                                       float bottom,
                                       const std::u16string& x_unit,
                                       const std::u16string& y_unit,
                                       int horizontal_points_number,
                                       int horizontal_ticks_interval,
                                       float vertical_ticks_interval);

 protected:
  void RefreshLegendValues();

 private:
  void OnButtonPressed();

  // Container for the |ReferenceLines| object.
  raw_ptr<views::View> reference_lines_container_ = nullptr;  // not owned

  // Container for the legend object.
  raw_ptr<views::View> legend_container_ = nullptr;              // not owned
  raw_ptr<views::ImageButton> legend_min_max_button_ = nullptr;  // not owned
  raw_ptr<Legend> legend_ = nullptr;                             // not owned

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_GRAPH_PAGE_VIEW_BASE_H_
