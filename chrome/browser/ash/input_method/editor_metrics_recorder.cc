// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"

namespace ash::input_method {

EditorMetricsRecorder::EditorMetricsRecorder(EditorOpportunityMode mode)
    : mode_(mode) {}

void EditorMetricsRecorder::SetMode(EditorOpportunityMode mode) {
  mode_ = mode;
}

void EditorMetricsRecorder::LogEditorState(EditorStates state) {
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name = "InputMethod.Manta.Orca.States.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name = "InputMethod.Manta.Orca.States.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }

  base::UmaHistogramEnumeration(histogram_name, state);
}

void EditorMetricsRecorder::LogNumberOfCharactersInserted(
    int number_of_characters) {
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }
  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
}

void EditorMetricsRecorder::LogNumberOfCharactersSelectedForInsert(
    int number_of_characters) {
  std::string histogram_name;
  switch (mode_) {
    case EditorOpportunityMode::kWrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Write";
      break;
    case EditorOpportunityMode::kRewrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Rewrite";
      break;
    case EditorOpportunityMode::kNone:
      return;
  }
  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
}

}  // namespace ash::input_method
