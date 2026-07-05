// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/metrics.h"

#include "base/metrics/histogram_functions.h"

namespace dictation {

void RecordDictationIsEnabledOnProfileInit(bool is_enabled) {
  base::UmaHistogramBoolean(kIsEnabledOnProfileInitHistogramName, is_enabled);
}

void RecordDictationSessionStartSource(DictationSessionEntryPoint entry_point) {
  base::UmaHistogramEnumeration(kSessionStartSourceHistogramName, entry_point);
}

void RecordDictationStreamStartTrigger(DictationStreamStartTrigger trigger) {
  base::UmaHistogramEnumeration(kStreamStartTriggerHistogramName, trigger);
}

}  // namespace dictation
