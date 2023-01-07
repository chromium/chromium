// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/assistant_field_trial_util_chrome.h"

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

using ::base::StringPiece;

namespace autofill_assistant {

bool AssistantFieldTrialUtilChrome::RegisterSyntheticFieldTrial(
    StringPiece trial_name,
    StringPiece group_name) const {
  return ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                                   group_name);
}

}  // namespace autofill_assistant
