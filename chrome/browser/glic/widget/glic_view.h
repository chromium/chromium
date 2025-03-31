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
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

class Profile;

namespace glic {

inline constexpr static float kGlicDesiredCornerRadius = 12;
// Actual corner radius used by views.
// This is set to 0 on Windows because:
// 1. Views-drawn rounded corners require window transparency
//    (Widget::InitParams::opacity set to kTranslucent).
// 2. Translucent windows are not supported for resizable window on Windows.
//    (see WidgetDelegate::CanResize).
// Windows 10 will not have rounded corners. Windows 11 will draw system default
// rounded corners of 8px radius (via Widget::InitParams::corner_radius).
// These corners could be suppressed by incompatible graphics drivers or local
// settings.
inline constexpr static float kGlicViewCornerRadius =
    BUILDFLAG(IS_WIN) ? 0 : kGlicDesiredCornerRadius;

class GlicView : public views::View {
  METADATA_HEADER(GlicView, views::View)

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

  void SetWebContents(content::WebContents* web_contents);

  // Try to get the background color from the web UI and use it as this view's
  // background color. Only call after the client is initialized.
  void UpdateBackgroundColor();

  views::WebView* web_view() { return web_view_; }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

 private:
  std::optional<SkColor> GetClientBackgroundColor();

  base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate_;
  raw_ptr<views::WebView> web_view_;
  // Defines the areas of the view from which it can be dragged. These areas can
  // be updated by the glic web client.
  std::vector<gfx::Rect> draggable_areas_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_
