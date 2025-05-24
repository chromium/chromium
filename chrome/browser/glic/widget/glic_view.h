// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_

#include <optional>

#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

class Profile;

namespace glic {

class GlicView : public views::WebView {
  METADATA_HEADER(GlicView, views::WebView)

 public:
  GlicView(Profile* profile,
           const gfx::Size& initial_size,
           base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate);
  GlicView(const GlicView&) = delete;
  GlicView& operator=(const GlicView&) = delete;
  ~GlicView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewElementIdForTesting);

  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  bool IsPointWithinDraggableArea(const gfx::Point& point);

  // Try to get the background color from the web UI and use it as this view's
  // background color. Only call after the client is initialized.
  void UpdateBackgroundColor();

  void UpdatePrimaryDraggableAreaOnResize();

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  base::WeakPtr<GlicView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::optional<SkColor> GetClientBackgroundColor();

  base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate_;
  raw_ptr<views::WebView> web_view_;
  // Defines the areas of the view from which it can be dragged. These areas can
  // be updated by the glic web client.
  std::vector<gfx::Rect> draggable_areas_;
  base::WeakPtrFactory<GlicView> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_
