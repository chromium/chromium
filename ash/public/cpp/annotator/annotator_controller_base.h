// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_CONTROLLER_BASE_H_
#define ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_CONTROLLER_BASE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
class AnnotatorClient;

// This controller provides an interface for ash annotator controller.
class ASH_PUBLIC_EXPORT AnnotatorControllerBase {
 public:
  AnnotatorControllerBase();
  AnnotatorControllerBase(const AnnotatorControllerBase&) = delete;
  AnnotatorControllerBase& operator=(const AnnotatorControllerBase&) = delete;
  virtual ~AnnotatorControllerBase();

  static AnnotatorControllerBase* Get();

  // Sets browser client.
  virtual void SetToolClient(AnnotatorClient* client) = 0;
  // Returns if the annotatotion canvas is available.
  virtual bool GetAnnotatorAvailability() const = 0;
  // Called when the ink canvas has either succeeded or failed in initializing.
  virtual void OnCanvasInitialized(bool success) = 0;
  // Toggles the annotation tray UI and marker enabled state.
  virtual void ToggleAnnotationTray() = 0;
  // Callback indicating availability of undo and redo functionalities.
  virtual void OnUndoRedoAvailabilityChanged(bool undo_available,
                                             bool redo_available) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_CONTROLLER_BASE_H_
