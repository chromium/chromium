// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_VIEW_H_
#define CHROME_BROWSER_GLIC_GLIC_VIEW_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

class BrowserFrameBoundsChangeAnimation;
class Profile;

namespace glic {

class GlicView : public views::View {
  METADATA_HEADER(GlicView, views::View)

 public:
  GlicView(Profile* profile, const gfx::Size& initial_size);
  GlicView(const GlicView&) = delete;
  GlicView& operator=(const GlicView&) = delete;
  ~GlicView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewElementIdForTesting);

  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  bool IsPointWithinDraggableArea(const gfx::Point& point);

  views::WebView* web_view() { return web_view_; }

 private:
  raw_ptr<views::WebView> web_view_;

  // Defines the areas of the view from which it can be dragged. These areas can
  // be updated by the glic web client.
  std::vector<gfx::Rect> draggable_areas_;

  // Animates programmatic changes to bounds (e.g. via `resizeTo()`
  // `resizeBy()` and `setContentsSize()` calls).
  std::unique_ptr<BrowserFrameBoundsChangeAnimation> bounds_change_animation_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_VIEW_H_
