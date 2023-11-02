// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_CHROME_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_CHROME_H_

#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"

namespace autofill_assistant {

// Provides field trial utils for Chrome.
class AssistantFieldTrialUtilChrome : public AssistantFieldTrialUtil {
  bool RegisterSyntheticFieldTrial(base::StringPiece trial_name,
                                   base::StringPiece group_name) const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_CHROME_H_
