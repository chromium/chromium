// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
#define ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_

#include "base/component_export.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"

namespace ash {

enum class AssistantButtonId;
enum class AssistantEntryPoint;
enum class AssistantExitPoint;

namespace assistant {
namespace util {

// Increment number of queries fired for each entry point.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void IncrementAssistantQueryCountForEntryPoint(AssistantEntryPoint entry_point);

// Record the entry point where Assistant UI becomes visible.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void RecordAssistantEntryPoint(AssistantEntryPoint entry_point);

// Record the exit point where Assistant UI becomes invisible.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void RecordAssistantExitPoint(AssistantExitPoint exit_point);

// Count the number of times buttons are clicked on Assistant UI.
COMPONENT_EXPORT(ASSISTANT_UTIL)
void IncrementAssistantButtonClickCount(AssistantButtonId button_id);

// Record the input source of each query (e.g. voice, typing).
COMPONENT_EXPORT(ASSISTANT_UTIL)
void RecordAssistantQuerySource(
    chromeos::assistant::mojom::AssistantQuerySource source);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
