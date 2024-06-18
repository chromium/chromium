// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
#define ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_

#include "ash/annotator/annotator_metrics.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

struct AnnotatorTool;
class AnnotatorClient;

// The controller in charge of annotator UI.
class ASH_EXPORT AnnotatorController {
 public:
  AnnotatorController();
  AnnotatorController(const AnnotatorController&) = delete;
  AnnotatorController& operator=(const AnnotatorController&) = delete;
  virtual ~AnnotatorController();

  // Sets the annotator tool.
  virtual void SetAnnotatorTool(const AnnotatorTool& tool);
  // Resets annotator tools and clears the canvas.
  void ResetTools();
  // Sets browser client.
  void SetToolClient(AnnotatorClient* client);

 private:
  raw_ptr<AnnotatorClient> client_ = nullptr;
};

}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATOR_CONTROLLER_H_
