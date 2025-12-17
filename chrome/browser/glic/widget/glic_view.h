// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

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
  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  // views::WebView:
  void SetWebContents(content::WebContents* web_contents) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  bool IsPointWithinDraggableArea(const gfx::Point& point);

  // Try to get the background color from the web UI and use it as this view's
  // background color. Only call after the client is initialized.
  void UpdateBackgroundColor();

  void SetBackgroundRoundedCorners(const gfx::RoundedCornersF& radii);
  const gfx::RoundedCornersF& background_rounded_corners() const {
    return background_radii_;
  }

  void UpdatePrimaryDraggableAreaOnResize();

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  base::WeakPtr<GlicView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void SetDraggableRegion(const SkRegion& region, bool for_webview);

  std::optional<SkColor> GetClientBackgroundColor();

  base::WeakPtr<ui::AcceleratorTarget> accelerator_delegate_;
  raw_ptr<views::WebView> web_view_;
  gfx::RoundedCornersF background_radii_;

  // Defines the areas of the view from which it can be dragged. These areas can
  // be updated by the glic web client.
  std::vector<gfx::Rect> draggable_areas_;

  SkRegion draggable_region_;
  SkRegion webview_draggable_region_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  base::WeakPtrFactory<GlicView> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_VIEW_H_
