// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_

#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"

namespace ash::input_method {

class EditorMetricsRecorder {
 public:
  explicit EditorMetricsRecorder(EditorOpportunityMode mode);

  void SetMode(EditorOpportunityMode mode);
  void LogEditorNativeUIShowOpportunityState(EditorOpportunityMode mode);

  void LogEditorState(EditorStates state);
  void LogNumberOfCharactersInserted(int number_of_characters);
  void LogNumberOfCharactersSelectedForInsert(int number_of_characters);

 private:
  EditorOpportunityMode mode_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_
