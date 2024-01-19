// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_

#include <optional>
#include <string_view>

#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {

EditorStates ToEditorStatesMetric(EditorBlockedReason reason);

EditorStates ToEditorStatesMetric(orca::mojom::TextQueryErrorCode error_code);

class EditorMetricsRecorder {
 public:
  explicit EditorMetricsRecorder(EditorOpportunityMode mode);

  void SetMode(EditorOpportunityMode mode);
  void SetTone(std::optional<std::string_view> preset_query_id,
               std::optional<std::string_view> freeform_text);
  void SetTone(EditorTone tone);
  void LogEditorNativeUIShowOpportunityState(EditorOpportunityMode mode);

  void LogEditorState(EditorStates state);
  void LogNumberOfCharactersInserted(int number_of_characters);
  void LogNumberOfCharactersSelectedForInsert(int number_of_characters);
  void LogNumberOfResponsesFromServer(int number_of_responses);

 private:
  EditorOpportunityMode mode_;
  EditorTone tone_ = EditorTone::kUnset;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_METRICS_RECORDER_H_
