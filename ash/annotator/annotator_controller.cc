// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_controller.h"

#include "ash/annotator/annotation_source_watcher.h"
#include "ash/annotator/annotation_tray.h"
#include "ash/annotator/annotations_overlay_controller.h"
#include "ash/annotator/annotator_metrics.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/annotator/annotations_overlay_view.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {
namespace {
AnnotatorMarkerColor GetMarkerColorForMetrics(SkColor color) {
  switch (color) {
    case kAnnotatorMagentaPenColor:
      return AnnotatorMarkerColor::kMagenta;
    case kAnnotatorBluePenColor:
      return AnnotatorMarkerColor::kBlue;
    case kAnnotatorRedPenColor:
      return AnnotatorMarkerColor::kRed;
    case kAnnotatorYellowPenColor:
      return AnnotatorMarkerColor::kYellow;
  }
  return AnnotatorMarkerColor::kMaxValue;
}

AnnotationTray* GetAnnotationTrayForRoot(aura::Window* root) {
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

  auto* annotation_tray =
      root_window_controller->GetStatusAreaWidget()->annotation_tray();
  DCHECK(annotation_tray);
  return annotation_tray;
}

void SetAnnotationTrayVisibility(aura::Window* root, bool visible) {
  if (auto* annotation_tray = GetAnnotationTrayForRoot(root)) {
    annotation_tray->SetVisiblePreferred(visible);
  }
}
}  // namespace

AnnotatorController::AnnotatorController() {
  annotation_source_watcher_ = std::make_unique<AnnotationSourceWatcher>(this);
}

AnnotatorController::~AnnotatorController() {
  annotation_source_watcher_.reset();
  annotations_overlay_controller_.reset();
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

void AnnotatorController::RegisterView(aura::Window* new_root) {
  // Make sure the annotator tray is only visible on one root window.
  // TODO(b/342104047): Remove this check when annotator starts being used
  // outside of the capture mode.
  if (current_root_) {
    UnregisterView(current_root_);
  }
  current_root_ = new_root;
  // Show the tray icon.
  SetAnnotationTrayVisibility(current_root_, /*visible=*/true);
}

void AnnotatorController::UnregisterView(aura::Window* window) {
  DCHECK_EQ(current_root_, window);
  if (auto* annotation_tray = GetAnnotationTrayForRoot(current_root_)) {
    annotation_tray->HideAnnotationTray();
  }
  current_root_ = nullptr;
}

void AnnotatorController::UpdateRootView(aura::Window* new_root) {
  // Do nothing if the root window is the same.
  if (new_root == current_root_) {
    return;
  }
  UnregisterView(current_root_);
  RegisterView(new_root);
  current_root_ = new_root;
  if (GetAnnotatorAvailability()) {
    UpdateTrayEnabledState();
  }
}

void AnnotatorController::EnableAnnotatorTool() {
  if (!annotator_enabled_ && annotations_overlay_controller_) {
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
  annotations_overlay_controller_.reset();
  canvas_initialized_state_.reset();
}

void AnnotatorController::CreateAnnotationOverlayForWindow(
    aura::Window* window,
    std::optional<gfx::Rect> partial_region_bounds) {
  annotations_overlay_controller_ =
      std::make_unique<AnnotationsOverlayController>(window,
                                                     partial_region_bounds);
}

void AnnotatorController::SetToolClient(AnnotatorClient* client) {
  client_ = client;
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
  if (auto* annotation_tray = GetAnnotationTrayForRoot(current_root_)) {
    annotation_tray->ToggleAnnotator();
  }
}

// Callback indicating availability of undo and redo functionalities.
void AnnotatorController::OnUndoRedoAvailabilityChanged(bool undo_available,
                                                        bool redo_available) {
  // TODO(b/198184362): Reflect undo and redo buttons availability
  // on the annotator tray.
}

std::unique_ptr<AnnotationsOverlayView>
AnnotatorController::CreateAnnotationsOverlayView() const {
  return client_->CreateAnnotationsOverlayView();
}

void AnnotatorController::UpdateTrayEnabledState() {
  if (auto* annotation_tray = GetAnnotationTrayForRoot(current_root_)) {
    annotation_tray->SetTrayEnabled(GetAnnotatorAvailability());
  }
}

void AnnotatorController::ToggleAnnotatorCanvas() {
  auto* capture_mode_controller = CaptureModeController::Get();
  // TODO(b/342104047): This check is necessary as long as we only toggle
  // annotator from Projector. Once we start using the annotator outside of
  // Projector, we should remove the check.
  if (capture_mode_controller->is_recording_in_progress()) {
    annotations_overlay_controller_->Toggle();
  }
}

}  // namespace ash
