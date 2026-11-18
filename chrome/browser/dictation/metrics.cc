// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace dictation {

void RecordDictationFirstRunExitStatus(DictationFirstRunExitStatus status) {
  base::UmaHistogramEnumeration(kFirstRunExitStatusHistogramName, status);
  switch (status) {
    case DictationFirstRunExitStatus::kCompleted:
      base::RecordAction(
          base::UserMetricsAction("VoiceTyping.FirstRunExperience.Accepted"));
      break;
    case DictationFirstRunExitStatus::kCancelled:
      base::RecordAction(
          base::UserMetricsAction("VoiceTyping.FirstRunExperience.Dismissed"));
      break;
    case DictationFirstRunExitStatus::kAbandoned:
      break;
  }
}

void RecordDictationIsEnabledOnProfileInit(bool is_enabled) {
  base::UmaHistogramBoolean(kIsEnabledOnProfileInitHistogramName, is_enabled);
}

void RecordDictationPageContextEligibility(
    DictationPageContextEligibility status) {
  base::UmaHistogramEnumeration(kPageContextEligibilityHistogramName, status);
}

void RecordDictationSessionStartSource(DictationSessionEntryPoint entry_point) {
  base::UmaHistogramEnumeration(kSessionStartSourceHistogramName, entry_point);
  switch (entry_point) {
    case DictationSessionEntryPoint::kContextMenu:
      base::RecordAction(
          base::UserMetricsAction("VoiceTyping.ContextMenuInvocation"));
      break;
    case DictationSessionEntryPoint::kHotkeyToggle:
      base::RecordAction(
          base::UserMetricsAction("VoiceTyping.HotkeyInvocation"));
      break;
  }
}

void RecordDictationSessionUrlCategory(DictationUrlCategory category) {
  base::UmaHistogramEnumeration(kSessionUrlCategoryHistogramName, category);
}

void RecordDictationStreamStartTrigger(DictationStreamStartTrigger trigger) {
  base::UmaHistogramEnumeration(kStreamStartTriggerHistogramName, trigger);
}

void RecordDictationStreamExitStatus(DictationStreamExitStatus status) {
  base::UmaHistogramEnumeration(kStreamExitReasonHistogramName, status);
}

void RecordDictationStreamStartLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes(kStreamStartLatencyHistogramName, latency);
  base::RecordAction(base::UserMetricsAction("VoiceTyping.Stream.Listening"));
}

void RecordDictationStreamAbandonedBeforeListening(base::TimeDelta elapsed) {
  base::UmaHistogramMediumTimes(kStreamAbandonedBeforeListeningHistogramName,
                                elapsed);
  base::RecordAction(base::UserMetricsAction("VoiceTyping.Stream.Cancelled"));
}

void RecordDictationTextCommitted() {
  base::RecordAction(base::UserMetricsAction("VoiceTyping.TextCommitted"));
}

void RecordDictationTextNone() {
  base::RecordAction(base::UserMetricsAction("VoiceTyping.TextNone"));
}

}  // namespace dictation
