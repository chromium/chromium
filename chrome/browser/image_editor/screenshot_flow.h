// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_
#define CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

namespace content {
class WebContents;
}

namespace gfx {
class Canvas;
}

namespace ui {
class Layer;
}

namespace image_editor {

// Structure containing image data and any future metadata.
struct ScreenshotCaptureResult {
  // The image obtained from capture. Empty on failure.
  gfx::Image image;
  // The bounds of the screen during which capture took place. Empty on failure.
  gfx::Rect screen_bounds;
};

// Callback for obtaining image data.
// OnceCallback: This class currently expects to capture only one screenshot,
// then the caller proceeds to the next steps in its user-facing flow.
typedef base::OnceCallback<void(const ScreenshotCaptureResult&)>
    ScreenshotCaptureCallback;

// ScreenshotFlow allows the user to enter a capture mode where they may drag
// a capture rectangle over a WebContents. The class calls a OnceCallback with
// the screenshot data contained in the region of interest.
class ScreenshotFlow : public ui::LayerDelegate, public ui::EventHandler {
 public:
  enum class CaptureMode {
    // Default, capture is not active.
    NOT_CAPTURING = 0,
    // User can drag a rectangle to select a region.
    SELECTION_RECTANGLE,
    // User can hover over the DOM to select individual elements.
    SELECTION_ELEMENT
  };

  explicit ScreenshotFlow(content::WebContents* web_contents);
  ScreenshotFlow(const ScreenshotFlow&) = delete;
  ScreenshotFlow& operator=(const ScreenshotFlow&) = delete;
  ~ScreenshotFlow() override;

  // Runs the entire screen capture and optional image editing flow:
  // Enters screenshot capture mode, allowing the user to choose a region of
  // the browser window to capture. Shows postprocessing options including
  // copying to the clipboard or saving.
  void Start(ScreenshotCaptureCallback flow_callback);

 private:
  // ui:EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // Requests a repaint of the selection layer.
  // |region| should contain the invalidation area.
  void RequestRepaint(gfx::Rect region);

  // Creates and adds the overlay over the webcontnts to handle selection.
  // Adds mouse listeners.
  void CreateAndAddUIOverlay();

  // Removes the UI overlay and any listeners.
  void RemoveUIOverlay();

  // Callback for initial internal screenshot capture;
  void OnSnapshotComplete(gfx::Image snapshot);

  // Completes the capture process for |region| and runs the callback provided
  // to Start().
  void CompleteCapture(const gfx::Rect& region);

  // Paints the screenshot selection layer. The user's selection is left
  // unpainted to be hollowed out. |invalidation_region| specifies an optional
  // region to clip to for performance; if empty, paints the entire window.
  void PaintSelectionLayer(gfx::Canvas* canvas,
                           const gfx::Rect& selection,
                           const gfx::Rect& invalidation_region);

  base::WeakPtr<ScreenshotFlow> weak_this_;

  // Whether we are in drag mode on this layer.
  ScreenshotFlow::CaptureMode capture_mode_ =
      ScreenshotFlow::CaptureMode::NOT_CAPTURING;

  // Web Contents that we are capturing.
  base::WeakPtr<content::WebContents> web_contents_;

  // Callback provided to Start().
  ScreenshotCaptureCallback flow_callback_;

  // Selection rectangle coordinates.
  gfx::Point drag_start_;
  gfx::Point drag_end_;

  // Invalidation area; empty for entire region.
  gfx::Rect paint_invalidation_;

  // Our top-level layer that is superimposed over the browser window's root
  // layer while screen capture mode is active.
  std::unique_ptr<ui::Layer> screen_capture_layer_;

  // Original window capture data providing the active capture area.
  gfx::ImageSkia image_foreground_;

  // Desaturated window capture data providing the background of region
  // selection.
  gfx::ImageSkia image_background_;

  base::WeakPtrFactory<ScreenshotFlow> weak_factory_{this};
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_
