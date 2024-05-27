// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

struct AnnotatorTool;

// This controller provides an interface to control the annotator tools.
class ASH_PUBLIC_EXPORT AnnotatorToolController {
 public:

  AnnotatorToolController();
  AnnotatorToolController(const AnnotatorToolController&) = delete;
  AnnotatorToolController& operator=(const AnnotatorToolController&) =
      delete;
  virtual ~AnnotatorToolController();

  static AnnotatorToolController* Get();

  // ProjectorController will use the following functions to manipulate the
  // annotator.

  // Sets the tool inside the annotator WebUI.
  virtual void SetTool(const AnnotatorTool& tool) = 0;
  // Undoes the last stroke in the annotator content.
  virtual void Undo() = 0;
  // Redoes the undone stroke in the annotator content.
  virtual void Redo() = 0;
  // Clears the contents of the annotator canvas.
  virtual void Clear() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATOR_TOOL_CONTROLLER_H_
