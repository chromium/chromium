// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

namespace actor {

// Records metrics when an autofill suggestion form is presented in the dialog.
void RecordOnSuggestionPresentedMetrics(
    int form_index,
    AttemptFormFillingToolRequest::RequestedData requested_data);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_METRICS_H_
