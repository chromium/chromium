// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATION_SOURCE_WATCHER_H_
#define ASH_ANNOTATOR_ANNOTATION_SOURCE_WATCHER_H_

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {
class AnnotatorController;

// Observes different sources for annotating in the OS and notifies the
// AnnotatorController to display the annotation tray on the source window. For
// example, the tray should be displayed when the user is video capturing the
// screen or when the user is in a meeting. Currently, only the capture mode as
// a source is supported.
class ASH_EXPORT AnnotationSourceWatcher : public CaptureModeObserver,
                                           public ProjectorSessionObserver {
 public:
  explicit AnnotationSourceWatcher(AnnotatorController* annotator_controller);
  AnnotationSourceWatcher(const AnnotationSourceWatcher&) = delete;
  AnnotationSourceWatcher& operator=(const AnnotationSourceWatcher&) = delete;
  ~AnnotationSourceWatcher() override;

  void NotifyMarkerClicked(aura::Window* current_root);
  void NotifyMarkerEnabled(aura::Window* current_root);
  void NotifyMarkerDisabled();

  // CaptureModeObserver:
  void OnRecordingStarted(aura::Window* current_root) override;
  void OnRecordingEnded() override;
  void OnVideoFileFinalized(bool user_deleted_video_file,
                            const gfx::ImageSkia& thumbnail) override;
  void OnRecordedWindowChangingRoot(aura::Window* new_root) override;
  void OnRecordingStartAborted() override;

  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

 private:
  raw_ptr<AnnotatorController> annotator_controller_ = nullptr;
  raw_ptr<CaptureModeController> capture_mode_controller_ = nullptr;
  bool is_projector_session_active_ = false;
  base::ScopedObservation<CaptureModeController, CaptureModeObserver>
      capture_mode_observation_{this};
  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};
};
}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATION_SOURCE_WATCHER_H_
