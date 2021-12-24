// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_H_

#include "base/strings/string_piece.h"

namespace autofill_assistant {

class AssistantFieldTrialUtil {
 public:
  virtual ~AssistantFieldTrialUtil() = default;

  virtual bool RegisterSyntheticFieldTrial(
      base::StringPiece trial_name,
      base::StringPiece group_name) const = 0;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_ASSISTANT_FIELD_TRIAL_UTIL_H_
