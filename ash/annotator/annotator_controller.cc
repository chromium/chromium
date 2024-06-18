// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/annotator/annotator_controller.h"

#include "ash/annotator/annotator_metrics.h"
#include "ash/projector/projector_annotation_tray.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "base/check.h"

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
}  // namespace

AnnotatorController::AnnotatorController() = default;

AnnotatorController::~AnnotatorController() {
  client_ = nullptr;
}

void AnnotatorController::SetAnnotatorTool(const AnnotatorTool& tool) {
  DCHECK(client_);
  client_->SetTool(tool);
  RecordMarkerColorMetrics(GetMarkerColorForMetrics(tool.color));
}

void AnnotatorController::ResetTools() {
  DCHECK(client_);
  client_->Clear();
}

void AnnotatorController::SetToolClient(AnnotatorClient* client) {
  client_ = client;
}
}  // namespace ash
