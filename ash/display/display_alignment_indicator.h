// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_
#define ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace display {
class Display;
}  // namespace display

namespace ash {

class IndicatorHighlightView;
class IndicatorPillView;

// DisplayAlignmentIndicator is a container for indicator highlighting a shared
// edge between two displays and a pill that contains an arrow and target
// display's name.
class ASH_EXPORT DisplayAlignmentIndicator {
 public:
  // Construct and show indicator highlight without a pill.
  // |src_display| is the display that the indicator is shown in.
  // |bounds| is the position and size of the 1px thick shared edge between
  // |src_display| and target display.
  static std::unique_ptr<DisplayAlignmentIndicator> Create(
      const display::Display& src_display,
      const gfx::Rect& bounds);

  // Construct and show indicator highlight with a pill.
  // |src_display| is the display that the indicator is shown in.
  // |bounds| is the position and size of the 1px thick shared edge between
  // |src_display| and target display. |target_name| is the name of the adjacent
  // display that is displayed in the pill.
  static std::unique_ptr<DisplayAlignmentIndicator> CreateWithPill(
      const display::Display& src_display,
      const gfx::Rect& bounds,
      const std::string& target_name);

  DisplayAlignmentIndicator(const DisplayAlignmentIndicator&) = delete;
  DisplayAlignmentIndicator& operator=(const DisplayAlignmentIndicator&) =
      delete;
  ~DisplayAlignmentIndicator();

  int64_t display_id() const { return display_id_; }

  // Shows/Hides the indicator.
  void Show();
  void Hide();

  // Updates the position of the indicator according to |bounds|. Used to move
  // around preview indicators during dragging. The indicator must NOT have a
  // pill.
  void Update(const display::Display& display, gfx::Rect bounds);

 private:
  friend class DisplayAlignmentIndicatorTest;
  friend class DisplayAlignmentControllerTest;

  // Pill does not render if |target_name| is an empty string.
  DisplayAlignmentIndicator(const display::Display& src_display,
                            const gfx::Rect& bounds,
                            const std::string& target_name);

  // The ID of the display that the indicator is shown on.
  const int64_t display_id_;

  // View and Widget for showing the blue indicator highlights on the edge of
  // the display.
  raw_ptr<IndicatorHighlightView, DanglingUntriaged> indicator_view_ =
      nullptr;  // NOT OWNED
  views::Widget indicator_widget_;

  // View and Widget for showing a pill with name of the neighboring display and
  // an arrow pointing towards it. May not be initialized if ctor without
  // |target_name| is used (for preview indicator).
  raw_ptr<IndicatorPillView, DanglingUntriaged> pill_view_ =
      nullptr;  // NOT OWNED
  std::unique_ptr<views::Widget> pill_widget_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ALIGNMENT_INDICATOR_H_
