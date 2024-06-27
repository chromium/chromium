// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
#define ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_

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

namespace ash {

struct AnnotatorTool;
class AnnotatorClient;

// The controller in charge of annotator UI.
class ASH_EXPORT AnnotatorController : public AnnotatorControllerBase {
 public:
  AnnotatorController();
  AnnotatorController(const AnnotatorController&) = delete;
  AnnotatorController& operator=(const AnnotatorController&) = delete;
  ~AnnotatorController() override;

  bool is_annotator_enabled() const { return annotator_enabled_; }

  // Sets the annotator tool.
  virtual void SetAnnotatorTool(const AnnotatorTool& tool);
  // Resets annotator tools and clears the canvas.
  void ResetTools();
  // Sets browser client.
  void SetToolClient(AnnotatorClient* client);
  // Shows annotation tray for `current_root`.
  void RegisterView(aura::Window* current_root);
  // Hides annotation tray for `current_root` if it was previously registered
  // with the controller.
  void UnregisterView(aura::Window* current_root);
  // Invoked when marker button is pressed.
  void EnableAnnotatorTool();
  // Resets the canvas and disables the annotator functionality.
  void DisableAnnotator();

  // AnnotatorControllerBase:
  bool GetAnnotatorAvailability() const override;
  void OnCanvasInitialized(bool success) override;
  void ToggleAnnotationTray() override;
  void OnUndoRedoAvailabilityChanged(bool undo_available,
                                     bool redo_available) override;

  // Updates the tray based on annotator availability.
  void UpdateTrayEnabledState();

  void set_canvas_initialized_callback_for_test(base::OnceClosure callback) {
    on_canvas_initialized_callback_for_test_ = std::move(callback);
  }

 private:
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
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
