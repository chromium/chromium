// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"

namespace ash::input_method {

void LogEditorState(EditorStates state,
                    EditorMode mode,
                    int quantity_to_increment) {
  std::string histogram_name;
  if (mode == EditorMode::kWrite) {
    histogram_name = "InputMethod.Manta.Orca.States.Write";
  } else if (mode == EditorMode::kRewrite) {
    histogram_name = "InputMethod.Manta.Orca.States.Rewrite";
  } else {
    return;
  }

  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, static_cast<int>(EditorStates::kMaxValue),
      static_cast<size_t>(static_cast<int>(EditorStates::kMaxValue) + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddCount(static_cast<int>(state), quantity_to_increment);
}

}  // namespace ash::input_method
