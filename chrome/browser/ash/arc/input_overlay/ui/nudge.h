// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace arc::input_overlay {

class DisplayOverlayController;

// This shows education nudge for Beta version with a dot indicator. It observes
// the its `anchor_view` widget.
// +---------------+
// |  |Dot|        |
// |---------------|
// ||icon|  |text| |
// +---------------+
class Nudge : public views::View, public views::WidgetObserver {
  METADATA_HEADER(Nudge, views::View)

 public:
  Nudge(DisplayOverlayController* controller,
        views::View* anchor_view,
        const std::u16string& text);

  Nudge(const Nudge&) = delete;
  Nudge& operator=(const Nudge) = delete;
  ~Nudge() override;

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  void UpdateBounds();

  const raw_ptr<DisplayOverlayController> controller_;
  const raw_ptr<views::View> anchor_view_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NUDGE_H_
