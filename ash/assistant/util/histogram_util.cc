// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/histogram_util.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash {
namespace assistant {
namespace util {

void IncrementAssistantQueryCountForEntryPoint(
    AssistantEntryPoint entry_point) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.QueryCountPerEntryPoint", entry_point);
}

void RecordAssistantEntryPoint(AssistantEntryPoint entry_point) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.EntryPoint", entry_point);
}

void RecordAssistantExitPoint(AssistantExitPoint exit_point) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.ExitPoint", exit_point);
}

void IncrementAssistantButtonClickCount(AssistantButtonId button_id) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.ButtonClickCount", button_id,
                            AssistantButtonId::kMaxValue);
}

void RecordAssistantQuerySource(AssistantQuerySource source) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.QuerySource", source);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
