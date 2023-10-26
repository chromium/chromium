// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"

namespace ash::input_method {

void LogEditorState(EditorStates state, EditorMode mode) {
  std::string histogram_name;
  if (mode == EditorMode::kWrite) {
    histogram_name = "InputMethod.Manta.Orca.States.Write";
  } else if (mode == EditorMode::kRewrite) {
    histogram_name = "InputMethod.Manta.Orca.States.Rewrite";
  } else {
    return;
  }

  base::UmaHistogramEnumeration(histogram_name, state);
}

void LogEditorNativeUIShowOpportunityState(EditorOpportunityMode mode) {
  switch (mode) {
    case EditorOpportunityMode::kRewrite:
      LogEditorState(EditorStates::kNativeUIShowOpportunity,
                     EditorMode::kRewrite);
      return;
    case EditorOpportunityMode::kWrite:
      LogEditorState(EditorStates::kNativeUIShowOpportunity,
                     EditorMode::kWrite);
      return;
    case EditorOpportunityMode::kNone:
      return;
  }
}

void LogNumberOfCharactersInserted(EditorMode mode, int number_of_characters) {
  std::string histogram_name;
  switch (mode) {
    case EditorMode::kWrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Write";
      break;
    case EditorMode::kRewrite:
      histogram_name = "InputMethod.Manta.Orca.CharactersInserted.Rewrite";
      break;
    case EditorMode::kConsentNeeded:
    case EditorMode::kBlocked:
      return;
  }
  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
}

void LogNumberOfCharactersSelectedForInsert(EditorMode mode,
                                            int number_of_characters) {
  std::string histogram_name;
  switch (mode) {
    case EditorMode::kWrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Write";
      break;
    case EditorMode::kRewrite:
      histogram_name =
          "InputMethod.Manta.Orca.CharactersSelectedForInsert.Rewrite";
      break;
    case EditorMode::kConsentNeeded:
    case EditorMode::kBlocked:
      return;
  }
  base::UmaHistogramCounts100000(histogram_name, number_of_characters);
}

}  // namespace ash::input_method
