// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_HIGHLIGHT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_HIGHLIGHT_H_

#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace gfx {
class canvas;
}  // namespace gfx

namespace arc::input_overlay {

class ActionView;
class DisplayOverlayController;

// ActionHighlight is a highlight circle drawn behind an action when it is
// highlighted.
class ActionHighlight : public views::View, public views::ViewObserver {
  METADATA_HEADER(ActionHighlight, views::View)

 public:
  ActionHighlight(DisplayOverlayController* controller,
                  ActionView* anchor_view);
  ActionHighlight(const ActionHighlight&) = delete;
  ActionHighlight& operator=(const ActionHighlight&) = delete;

  ~ActionHighlight() override;

  void UpdateAnchorView(ActionView* anchor_view);

  // views::ViewObserver:
  void OnViewRemovedFromWidget(views::View*) override;

  ActionView* anchor_view() { return anchor_view_; }

 private:
  void UpdateWidgetBounds();
  int GetCircleRadius() const;
  // Gets the radius including circle border thickness.
  int GetOverallRadius() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  raw_ptr<DisplayOverlayController> controller_;

  raw_ptr<ActionView> anchor_view_;

  // Watches for the anchor view to be destroyed or removed from its widget.
  // Prevents the action highlight from lingering after its anchor is
  // invalid, which can cause strange behavior.
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_HIGHLIGHT_H_
