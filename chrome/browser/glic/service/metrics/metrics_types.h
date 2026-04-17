// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_

#include <string>

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ResponseSegmentation)
enum class ResponseSegmentation {
  kUnknown = 0,
  kOsButtonAttachedText = 1,
  kOsButtonAttachedAudio = 2,
  kOsButtonDetachedText = 3,
  kOsButtonDetachedAudio = 4,
  kOsButtonMenuAttachedText = 5,
  kOsButtonMenuAttachedAudio = 6,
  kOsButtonMenuDetachedText = 7,
  kOsButtonMenuDetachedAudio = 8,
  kOsButtonHotkeyAttachedText = 9,
  kOsButtonHotkeyAttachedAudio = 10,
  kOsButtonHotkeyDetachedText = 11,
  kOsButtonHotkeyDetachedAudio = 12,
  kButtonTopChromeAttachedText = 13,
  kButtonTopChromeAttachedAudio = 14,
  kButtonTopChromeDetachedText = 15,
  kButtonTopChromeDetachedAudio = 16,
  kFreAttachedText = 17,
  kFreAttachedAudio = 18,
  kFreDetachedText = 19,
  kFreDetachedAudio = 20,
  kProfilePickerAttachedText = 21,
  kProfilePickerAttachedAudio = 22,
  kProfilePickerDetachedText = 23,
  kProfilePickerDetachedAudio = 24,
  kNudgeAttachedText = 25,
  kNudgeAttachedAudio = 26,
  kNudgeDetachedText = 27,
  kNudgeDetachedAudio = 28,
  kThreeDotsMenuAttachedText = 29,
  kThreeDotsMenuAttachedAudio = 30,
  kThreeDotsMenuDetachedText = 31,
  kThreeDotsMenuDetachedAudio = 32,
  kUnsupportedAttachedText = 33,
  kUnsupportedAttachedAudio = 34,
  kUnsupportedDetachedText = 35,
  kUnsupportedDetachedAudio = 36,
  kWhatsNewAttachedText = 37,
  kWhatsNewAttachedAudio = 38,
  kWhatsNewDetachedText = 39,
  kWhatsNewDetachedAudio = 40,
  kAfterSignInAttachedText = 41,
  kAfterSignInAttachedAudio = 42,
  kAfterSignInDetachedText = 43,
  kAfterSignInDetachedAudio = 44,
  kSharedTabAttachedText = 45,
  kSharedTabAttachedAudio = 46,
  kSharedTabDetachedText = 47,
  kSharedTabDetachedAudio = 48,
  kActorTaskIconAttachedText = 49,
  kActorTaskIconAttachedAudio = 50,
  kActorTaskIconDetachedText = 51,
  kActorTaskIconDetachedAudio = 52,
  kSharedImageAttachedText = 53,
  kSharedImageAttachedAudio = 54,
  kSharedImageDetachedText = 55,
  kSharedImageDetachedAudio = 56,
  kHandoffButtonAttachedText = 57,
  kHandoffButtonAttachedAudio = 58,
  kHandoffButtonDetachedText = 59,
  kHandoffButtonDetachedAudio = 60,
  kSkillsAttachedText = 61,
  kSkillsAttachedAudio = 62,
  kSkillsDetachedText = 63,
  kSkillsDetachedAudio = 64,
  kAutoOpenedByContextualCueAttachedText = 65,
  kAutoOpenedByContextualCueAttachedAudio = 66,
  kAutoOpenedByContextualCueDetachedText = 67,
  kAutoOpenedByContextualCueDetachedAudio = 68,
  kPdfSummarizeButtonAttachedText = 69,
  kPdfSummarizeButtonAttachedAudio = 70,
  kPdfSummarizeButtonDetachedText = 71,
  kPdfSummarizeButtonDetachedAudio = 72,
  kNavigationCaptureAttachedText = 73,
  kNavigationCaptureAttachedAudio = 74,
  kNavigationCaptureDetachedText = 75,
  kNavigationCaptureDetachedAudio = 76,
  kAutoOpenedForPdfAttachedText = 77,
  kAutoOpenedForPdfAttachedAudio = 78,
  kAutoOpenedForPdfDetachedText = 79,
  kAutoOpenedForPdfDetachedAudio = 80,
  kCaptureRegionHotkeyAttachedText = 81,
  kCaptureRegionHotkeyAttachedAudio = 82,
  kCaptureRegionHotkeyDetachedText = 83,
  kCaptureRegionHotkeyDetachedAudio = 84,
  kIphAttachedText = 85,
  kIphAttachedAudio = 86,
  kIphDetachedText = 87,
  kIphDetachedAudio = 88,
  kAnchoredContextualCueAttachedText = 89,
  kAnchoredContextualCueAttachedAudio = 90,
  kAnchoredContextualCueDetachedText = 91,
  kAnchoredContextualCueDetachedAudio = 92,
  kWebContentsContextMenuAttachedText = 93,
  kWebContentsContextMenuAttachedAudio = 94,
  kWebContentsContextMenuDetachedText = 95,
  kWebContentsContextMenuDetachedAudio = 96,
  kTextSelectionNudgeAttachedText = 97,
  kTextSelectionNudgeAttachedAudio = 98,
  kTextSelectionNudgeDetachedText = 99,
  kTextSelectionNudgeDetachedAudio = 100,
  kTextSelectionWidgetAttachedText = 101,
  kTextSelectionWidgetAttachedAudio = 102,
  kTextSelectionWidgetDetachedText = 103,
  kTextSelectionWidgetDetachedAudio = 104,
  kZeroStateAutoSummarizeAttachedText = 105,
  kZeroStateAutoSummarizeAttachedAudio = 106,
  kZeroStateAutoSummarizeDetachedText = 107,
  kZeroStateAutoSummarizeDetachedAudio = 108,
  kMaxValue = kZeroStateAutoSummarizeDetachedAudio,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicResponseSegmentation)

enum class ModeOffset : int {
  kTextAttached = 1,
  kAudioAttached = 2,
  kTextDetached = 3,
  kAudioDetached = 4,
  kMaxValue = kAudioDetached,
};

enum class DaisyChainSource {
  kUnknown = 0,
  kGlicContents = 1,
  kTabContents = 2,
  kActorAddTab = 3,
  kNewTab = 4,
  kWebHandoff = 5,
  kAutoOpenPdf = 6,
  kMaxValue = kAutoOpenPdf,
};

std::string GetDaisyChainSourceString(DaisyChainSource source);

ResponseSegmentation GetResponseSegmentation(bool attached,
                                             mojom::WebClientMode mode,
                                             mojom::InvocationSource source);

// GlicEntrypoint defines entrypoints interesting from growth metrics
// perspective. It's a subset of InvocationSource, and more. When adding a new
// invocation source, consider if a new entry should be added to the existing
// enum (and add new mapping in GetEntrypointFromInvocationSource if so), or if
// it can be mapped to an existing entry. By default each new InvocationSource
// is mapped to kOther.
// LINT.IfChange(GlicEntrypoint)
enum class GlicEntrypoint {
  kOsButton = 0,
  kOsHotkey = 1,
  kTopChromeButton = 2,
  kNudge = 3,
  kThreeDotsMenu = 4,
  kWhatsNew = 5,
  kSharedTab = 6,
  kSharedImage = 7,
  kSkills = 8,
  kAutoOpenedByContextualCue = 9,
  kPdfSummarizeButton = 10,
  kNavigationCapture = 11,
  kAutoOpenedForPdf = 12,
  kIph = 13,
  kOther = 14,
  kWebContentsContextMenu = 15,
  kTextSelectionNudge = 16,
  kTextSelectionWidget = 17,
  kZeroStateAutoSummarize = 18,
  kMaxValue = kZeroStateAutoSummarize,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicEntrypoint)

GlicEntrypoint GetEntrypointFromInvocationSource(
    mojom::InvocationSource source);
std::string GetEntrypointString(GlicEntrypoint entrypoint);
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
