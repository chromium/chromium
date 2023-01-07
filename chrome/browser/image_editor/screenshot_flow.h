// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_
#define CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/image_editor/event_capture_mac.h"
#else
#include "base/scoped_observation.h"
#endif

namespace content {
class WebContents;
enum class Visibility;
}  // namespace content

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui {
class EventTarget;
class Layer;
}  // namespace ui

namespace image_editor {

// Class to associate screenshot capture data with Profile across navigation.
class ScreenshotCapturedData : public base::SupportsUserData::Data {
 public:
  ScreenshotCapturedData();
  ~ScreenshotCapturedData() override;
  ScreenshotCapturedData(const ScreenshotCapturedData&) = delete;
  ScreenshotCapturedData& operator=(const ScreenshotCapturedData&) = delete;

  static constexpr char kDataKey[] = "share_hub_screenshot_data";

  base::FilePath screenshot_filepath;
};

// Result codes to distinguish between how the capture mode was closed.
enum class ScreenshotCaptureResultCode {
  // Successful capture.
  SUCCESS = 0,
  // User navigated away from the primary web contents page while the capture
  // mode was active.
  USER_NAVIGATED_EXIT = 1,
  // User exited the capture mode via key press.
  USER_ESCAPE_EXIT = 2,
  kMaxValue = USER_ESCAPE_EXIT
};

// Structure containing image data and any future metadata.
struct ScreenshotCaptureResult {
  // The image obtained from capture. Empty on failure.
  gfx::Image image;
  // The bounds of the screen during which capture took place. Empty on failure.
  gfx::Rect screen_bounds;
  // The result code of the capture describing why the user exited.
  ScreenshotCaptureResultCode result_code;
};

// Callback for obtaining image data.
// OnceCallback: This class currently expects to capture only one screenshot,
// then the caller proceeds to the next steps in its user-facing flow.
typedef base::OnceCallback<void(const ScreenshotCaptureResult&)>
    ScreenshotCaptureCallback;

// ScreenshotFlow allows the user to enter a capture mode where they may drag
// a capture rectangle over a WebContents. The class calls a OnceCallback with
// the screenshot data contained in the region of interest.
class ScreenshotFlow : public content::WebContentsObserver,
                       public ui::LayerDelegate,
                       public ui::EventHandler {
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

  // Runs the screen capture flow, capturing the entire viewport rather than
  // a region selected by the user.
  void StartFullscreenCapture(ScreenshotCaptureCallback flow_callback);

  // Exits capture mode without running any callbacks.
  void CancelCapture();

  // Returns whether the capture mode is open or not.
  bool IsCaptureModeActive();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  class UnderlyingWebContentsObserver;

  // ui:EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

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

  // Checks whether the UI overlay is visible.
  bool IsUIOverlayShown();

  // Resizes the UI overlay. It's used to make the UI overlay responsive to
  // the frame size changes.
  void ResetUIOverlayBounds();

  // Removes the UI overlay and any listeners.
  void RemoveUIOverlay();

  // Captures a new screenshot for the chosen region, and runs the completion
  // callback.
  void CaptureAndRunScreenshotCompleteCallback(
      ScreenshotCaptureResultCode result_code,
      gfx::Rect region);

  // Completes the capture process for |region| and runs the callback provided
  // to Start().
  void CompleteCapture(ScreenshotCaptureResultCode result_code,
                       const gfx::Rect& region);

  // Completes the capture process and runs the |flow_callback| with provided
  // |image| data sourced from |bounds|.
  void RunScreenshotCompleteCallback(ScreenshotCaptureResultCode result_code,
                                     gfx::Rect bounds,
                                     gfx::Image image);

  // Paints the screenshot selection layer. The user's selection is left
  // unpainted to be hollowed out. |invalidation_region| specifies an optional
  // region to clip to for performance; if empty, paints the entire window.
  void PaintSelectionLayer(gfx::Canvas* canvas,
                           const gfx::Rect& selection,
                           const gfx::Rect& invalidation_region);

  // Requests to set the cursor type.
  void SetCursor(ui::mojom::CursorType cursor_type);

  // Attempts to capture the region defined by |drag_start_| and |drag_end_|
  // while also making sure the points are within the web contents view bounds.
  void AttemptRegionCapture(gfx::Rect view_bounds);

  // Setter for |is_dragging_|.
  void SetIsDragging(bool value);

  base::WeakPtr<ScreenshotFlow> weak_this_;

  // Whether we are in drag mode on this layer.
  ScreenshotFlow::CaptureMode capture_mode_ =
      ScreenshotFlow::CaptureMode::NOT_CAPTURING;

  // Web Contents that we are capturing.
  base::WeakPtr<content::WebContents> web_contents_;

  // Observer for |web_contents_|.
  std::unique_ptr<UnderlyingWebContentsObserver> web_contents_observer_;

  // Callback provided to Start().
  ScreenshotCaptureCallback flow_callback_;

  // Mac-specific
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<EventCaptureMac> event_capture_mac_;
#else
  base::ScopedObservation<ui::EventTarget, ui::EventHandler> event_capture_{
      this};
#endif

  // Selection rectangle coordinates.
  gfx::Point drag_start_;
  gfx::Point drag_end_;

  // Whether the user is currently dragging on the capture UI.
  bool is_dragging_ = false;

  // Invalidation area; empty for entire region.
  gfx::Rect paint_invalidation_;

  // Our top-level layer that is superimposed over the browser window's root
  // layer while screen capture mode is active.
  std::unique_ptr<ui::Layer> screen_capture_layer_;

  base::WeakPtrFactory<ScreenshotFlow> weak_factory_{this};
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_IMAGE_EDITOR_SCREENSHOT_FLOW_H_
