// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotation_source_watcher.h"

#include "ash/annotator/annotator_controller.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

AnnotationSourceWatcher::AnnotationSourceWatcher(
    AnnotatorController* annotator_controller)
    : annotator_controller_(annotator_controller) {
  capture_mode_controller_ = CaptureModeController::Get();
  capture_mode_observation_.Observe(capture_mode_controller_);
  ProjectorControllerImpl* projector_controller =
      Shell::Get()->projector_controller();
  projector_session_observation_.Observe(
      projector_controller->projector_session());
}

AnnotationSourceWatcher::~AnnotationSourceWatcher() {
  projector_session_observation_.Reset();
  capture_mode_observation_.Reset();
  annotator_controller_ = nullptr;
  capture_mode_controller_ = nullptr;
}

void AnnotationSourceWatcher::NotifyMarkerEnabled(aura::Window* current_root) {
  annotator_controller_->RegisterView(current_root);
  annotator_controller_->CreateAnnotationOverlayForMarkerMode(current_root);
}
void AnnotationSourceWatcher::NotifyMarkerDisabled() {
  annotator_controller_->DisableAnnotator();
}

void AnnotationSourceWatcher::OnRecordingStarted(aura::Window* current_root) {
  if (!capture_mode_controller_->ShouldAllowAnnotating()) {
    return;
  }

  // TODO(b/342104047): Remove this check once the annotator is always enabled
  // in capture mode.
  if (!is_projector_session_active_) {
    return;
  }

  annotator_controller_->RegisterView(current_root);
}

void AnnotationSourceWatcher::OnRecordingEnded() {
  if (!capture_mode_controller_->IsAnnotatingSupported()) {
    return;
  }

  // TODO(b/342104047): Remove this check once the annotator is always enabled
  // in capture mode.
  if (!is_projector_session_active_) {
    return;
  }

  annotator_controller_->DisableAnnotator();
}

void AnnotationSourceWatcher::OnVideoFileFinalized(
    bool user_deleted_video_file,
    const gfx::ImageSkia& thumbnail) {}

void AnnotationSourceWatcher::OnRecordedWindowChangingRoot(
    aura::Window* new_root) {
  if (!capture_mode_controller_->ShouldAllowAnnotating()) {
    return;
  }

  // TODO(b/342104047): Remove this check once the annotator is always enabled
  // in capture mode.
  if (!is_projector_session_active_) {
    return;
  }

  annotator_controller_->UpdateRootView(new_root);
}

void AnnotationSourceWatcher::OnRecordingStartAborted() {
  if (!capture_mode_controller_->IsAnnotatingSupported()) {
    return;
  }

  // TODO(b/342104047): Remove this check once the annotator is always enabled
  // in capture mode.
  if (!is_projector_session_active_) {
    return;
  }

  annotator_controller_->DisableAnnotator();
}

void AnnotationSourceWatcher::OnProjectorSessionActiveStateChanged(
    bool active) {
  is_projector_session_active_ = active;
  if (!is_projector_session_active_) {
    annotator_controller_->DisableAnnotator();
  }
}

}  // namespace ash
