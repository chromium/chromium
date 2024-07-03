// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_controller.h"

#include "ash/annotator/annotator_metrics.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/annotator/annotations_overlay_view.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "base/check.h"
#include "ui/aura/window.h"

namespace ash {
namespace {
AnnotatorMarkerColor GetMarkerColorForMetrics(SkColor color) {
  // TODO(b/342104047): Rename colors to remove projector from the name.
  switch (color) {
    case kProjectorMagentaPenColor:
      return AnnotatorMarkerColor::kMagenta;
    case kProjectorBluePenColor:
      return AnnotatorMarkerColor::kBlue;
    case kProjectorRedPenColor:
      return AnnotatorMarkerColor::kRed;
    case kProjectorYellowPenColor:
      return AnnotatorMarkerColor::kYellow;
  }
  return AnnotatorMarkerColor::kMaxValue;
}

ProjectorAnnotationTray* GetAnnotationTrayForRoot(aura::Window* root) {
  // It may happen that root is nullptr. This may happen in the event that
  // the annotation tray is hidden before the canvas finishes its
  // initialization.
  if (!root) {
    return nullptr;
  }

  DCHECK(root->IsRootWindow());

  // Annotating can end when a display being fullscreen-captured gets removed,
  // in this case, we don't need to hide the button.
  if (root->is_destroying()) {
    return nullptr;
  }

  // Can be null while shutting down.
  auto* root_window_controller = RootWindowController::ForWindow(root);
  if (!root_window_controller) {
    return nullptr;
  }

  auto* projector_annotation_tray =
      root_window_controller->GetStatusAreaWidget()
          ->projector_annotation_tray();
  DCHECK(projector_annotation_tray);
  return projector_annotation_tray;
}

void SetProjectorAnnotationTrayVisibility(aura::Window* root, bool visible) {
  if (auto* projector_annotation_tray = GetAnnotationTrayForRoot(root)) {
    projector_annotation_tray->SetVisiblePreferred(visible);
  }
}

void ToggleAnnotatorCanvas() {
  auto* capture_mode_controller = CaptureModeController::Get();
  // TODO(b/200292852): This check should not be necessary, but because
  // several Projector unit tests that rely on mocking and don't test the real
  // code path, we can end up calling |ToggleAnnotationsOverlayEnabled()|
  // without ever starting a Projector recording session.
  // |CaptureModeController| asserts all invariants via DCHECKs, and those
  // tests would crash. Remove any unnecessary mocks and test the real thing
  // if possible.
  if (capture_mode_controller->is_recording_in_progress()) {
    capture_mode_controller->ToggleAnnotationsOverlayEnabled();
  }
}
}  // namespace

AnnotatorController::AnnotatorController() = default;

AnnotatorController::~AnnotatorController() {
  client_ = nullptr;
  current_root_ = nullptr;
}

void AnnotatorController::SetAnnotatorTool(const AnnotatorTool& tool) {
  DCHECK(client_);
  client_->SetTool(tool);
  RecordMarkerColorMetrics(GetMarkerColorForMetrics(tool.color));
}

void AnnotatorController::ResetTools() {
  if (annotator_enabled_) {
    DCHECK(client_);
    ToggleAnnotatorCanvas();
    annotator_enabled_ = false;
    client_->Clear();
  }
}

void AnnotatorController::SetToolClient(AnnotatorClient* client) {
  client_ = client;
}

void AnnotatorController::RegisterView(aura::Window* current_root) {
  current_root_ = current_root;
  // Show the tray icon.
  SetProjectorAnnotationTrayVisibility(current_root_, /*visible=*/true);
}

void AnnotatorController::UnregisterView(aura::Window* window) {
  DCHECK_EQ(current_root_, window);
  if (auto* projector_annotation_tray =
          GetAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->HideAnnotationTray();
  }
  current_root_ = nullptr;
}

void AnnotatorController::EnableAnnotatorTool() {
  if (!annotator_enabled_) {
    ToggleAnnotatorCanvas();
    annotator_enabled_ = !annotator_enabled_;
    // TODO(b/342104047): Decouple from projector metrics.
    RecordToolbarMetrics(ProjectorToolbar::kMarkerTool);
  }
}

void AnnotatorController::DisableAnnotator() {
  ResetTools();
  if (current_root_) {
    UnregisterView(current_root_);
  }

  canvas_initialized_state_.reset();
}

bool AnnotatorController::GetAnnotatorAvailability() const {
  return canvas_initialized_state_.value_or(false);
}

void AnnotatorController::OnCanvasInitialized(bool success) {
  canvas_initialized_state_ = success;
  UpdateTrayEnabledState();
  if (on_canvas_initialized_callback_for_test_) {
    std::move(on_canvas_initialized_callback_for_test_).Run();
  }
}

void AnnotatorController::ToggleAnnotationTray() {
  if (auto* projector_annotation_tray =
          GetAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->ToggleAnnotator();
  }
}

// Callback indicating availability of undo and redo functionalities.
void AnnotatorController::OnUndoRedoAvailabilityChanged(bool undo_available,
                                                        bool redo_available) {
  // TODO(b/198184362): Reflect undo and redo buttons availability
  // on the annotator tray.
}

void AnnotatorController::UpdateTrayEnabledState() {
  if (auto* projector_annotation_tray =
          GetAnnotationTrayForRoot(current_root_)) {
    projector_annotation_tray->SetTrayEnabled(GetAnnotatorAvailability());
  }
}

std::unique_ptr<AnnotationsOverlayView>
AnnotatorController::CreateAnnotationsOverlayView() const {
  return client_->CreateAnnotationsOverlayView();
}

}  // namespace ash
