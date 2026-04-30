// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"

namespace contextual_cueing {

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueInteraction",
                                contextual_cueing_interaction);
}

}  // namespace contextual_cueing
