// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
#define ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/annotator/annotator_metrics.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/annotator/annotator_controller_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

struct AnnotatorTool;
class AnnotatorClient;
class AnnotationsOverlayView;
class AnnotationsOverlayController;
class AnnotationSourceWatcher;

// The controller in charge of annotator UI.
class ASH_EXPORT AnnotatorController : public AnnotatorControllerBase {
 public:
  AnnotatorController();
  AnnotatorController(const AnnotatorController&) = delete;
  AnnotatorController& operator=(const AnnotatorController&) = delete;
  ~AnnotatorController() override;

  bool is_annotator_enabled() const { return annotator_enabled_; }
  AnnotationSourceWatcher* annotation_source_watcher() {
    return annotation_source_watcher_.get();
  }

  // Sets the annotator tool.
  virtual void SetAnnotatorTool(const AnnotatorTool& tool);
  // Resets annotator tools and clears the canvas.
  void ResetTools();
  // Shows annotation tray for `new_root`.
  void RegisterView(aura::Window* new_root);
  // Hides annotation tray for `current_root` if it was previously registered
  // with the controller.
  void UnregisterView(aura::Window* current_root);
  // Updates `current_root_` to be the given `new_root`. Updates annotation
  // tray's enabled state.
  void UpdateRootView(aura::Window* new_root);
  // Invoked when marker button is pressed.
  void EnableAnnotatorTool();
  // Resets the canvas and disables the annotator functionality.
  void DisableAnnotator();
  // Creates `annotations_overlay_controller_` for the `window`. This is
  // necessary, as it adds a view for annotating on top of the `window` and
  // loads the Ink library in the view. After the call, annotating is still
  // disabled, until the EnableAnnotatorTool() method is called.
  // TODO(b/342104047): Once the markup pod is implemented, make this method
  // private, called from EnableAnnotatorTool(). Add handling for multiple
  // displays.
  void CreateAnnotationOverlayForWindow(
      aura::Window* window,
      std::optional<gfx::Rect> partial_region_bounds);

  // AnnotatorControllerBase:
  void SetToolClient(AnnotatorClient* client) override;
  bool GetAnnotatorAvailability() const override;
  void OnCanvasInitialized(bool success) override;
  void ToggleAnnotationTray() override;
  void OnUndoRedoAvailabilityChanged(bool undo_available,
                                     bool redo_available) override;

  // Returns a new instance of the concrete view that will be used as the
  // content view of the annotations overlay widget.
  std::unique_ptr<AnnotationsOverlayView> CreateAnnotationsOverlayView() const;

  void set_canvas_initialized_callback_for_test(base::OnceClosure callback) {
    on_canvas_initialized_callback_for_test_ = std::move(callback);
  }

 private:
  friend class CaptureModeTestApi;

  // Updates the tray based on annotator availability.
  void UpdateTrayEnabledState();
  // Toggles annotation overlay widget on or off. When on, the annotations
  // overlay widget's window will be shown and can consume all the events
  // targeting the underlying window. Otherwise, it's hidden and cannot accept
  // any events.
  void ToggleAnnotatorCanvas();
  raw_ptr<AnnotatorClient> client_ = nullptr;
  // True if the canvas is initialized successfully, false if it failed to
  // initialize. An absent value indicates that the initialization has not
  // completed.
  std::optional<bool> canvas_initialized_state_;
  bool annotator_enabled_ = false;
  // The current root window for showing annotations.
  raw_ptr<aura::Window> current_root_ = nullptr;
  // If set, will be called when the canvas is initialized.
  base::OnceClosure on_canvas_initialized_callback_for_test_;
  // Controls and owns the overlay widget, which is used to host annotations.
  std::unique_ptr<AnnotationsOverlayController> annotations_overlay_controller_;
  std::unique_ptr<AnnotationSourceWatcher> annotation_source_watcher_;
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
