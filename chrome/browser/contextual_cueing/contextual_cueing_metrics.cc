// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"

namespace contextual_cueing {

namespace {

std::string InteractionTypeToString(ContextualCueingInteraction interaction) {
  switch (interaction) {
    case ContextualCueingInteraction::kCueClicked:
      return "Clicked";
    case ContextualCueingInteraction::kCueDismissed:
      return "Dismissed";
    case ContextualCueingInteraction::kCueEditPrompt:
      return "EditPrompt";
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      return "Settings";
  }
}

}  // namespace

void RecordContextualCueingInteraction(
    ContextualCueingInteraction contextual_cueing_interaction,
    const std::string& cuj) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.CueInteraction",
                                contextual_cueing_interaction);

  std::string histogram_name =
      "ContextualCueing.V2.CueInteraction." +
      InteractionTypeToString(contextual_cueing_interaction);
  base::UmaHistogramSparse(histogram_name, base::HashMetricName(cuj));
}

}  // namespace contextual_cueing
