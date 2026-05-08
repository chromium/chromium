// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

std::string GetDaisyChainSourceString(DaisyChainSource source) {
  switch (source) {
    case DaisyChainSource::kGlicContents:
      return "GlicContents";
    case DaisyChainSource::kTabContents:
      return "TabContents";
    case DaisyChainSource::kActorAddTab:
      return "ActorAddTab";
    case DaisyChainSource::kNewTab:
      return "NewTab";
    case DaisyChainSource::kWebHandoff:
      return "WebHandoff";
    case DaisyChainSource::kAutoOpenPdf:
      return "AutoOpenPdf";
    case DaisyChainSource::kLastActiveInstance:
      return "LastActiveInstance";
    case DaisyChainSource::kBookmark:
      return "Bookmark";
    case DaisyChainSource::kUnknown:
      return "Unknown";
  }
}

ResponseSegmentation GetResponseSegmentation(bool attached,
                                             mojom::WebClientMode mode,
                                             mojom::InvocationSource source) {
  if (mode == mojom::WebClientMode::kUnknown) {
    return ResponseSegmentation::kUnknown;
  }

  ModeOffset modeOffset;
  if (mode == mojom::WebClientMode::kText && attached) {
    modeOffset = ModeOffset::kTextAttached;
  } else if (mode == mojom::WebClientMode::kAudio && attached) {
    modeOffset = ModeOffset::kAudioAttached;
  } else if (mode == mojom::WebClientMode::kText && !attached) {
    modeOffset = ModeOffset::kTextDetached;
  } else {
    modeOffset = ModeOffset::kAudioDetached;
  }

  int baseIndex =
      static_cast<int>(source) * (static_cast<int>(ModeOffset::kMaxValue));
  int offset = static_cast<int>(modeOffset);

  return static_cast<ResponseSegmentation>(baseIndex + offset);
}

std::string GetInvocationSourceString(mojom::InvocationSource source) {
  switch (source) {
    case mojom::InvocationSource::kActorTaskIcon:
      return "ActorTaskIcon";
    case mojom::InvocationSource::kAfterSignIn:
      return "AfterSignIn";
    case mojom::InvocationSource::kAnchoredContextualCue:
      return "AnchoredContextualCue";
    case mojom::InvocationSource::kAutoOpenedByContextualCue:
      return "AutoOpenedByContextualCue";
    case mojom::InvocationSource::kAutoOpenedForPdf:
      return "AutoOpenedForPdf";
    case mojom::InvocationSource::kCaptureRegionHotkey:
      return "CaptureRegionHotkey";
    case mojom::InvocationSource::kFre:
      return "Fre";
    case mojom::InvocationSource::kHandoffButton:
      return "HandoffButton";
    case mojom::InvocationSource::kIph:
      return "Iph";
    case mojom::InvocationSource::kNavigationCapture:
      return "NavigationCapture";
    case mojom::InvocationSource::kNudge:
      return "Nudge";
    case mojom::InvocationSource::kOsButton:
      return "OsButton";
    case mojom::InvocationSource::kOsButtonMenu:
      return "OsButtonMenu";
    case mojom::InvocationSource::kOsHotkey:
      return "OsHotkey";
    case mojom::InvocationSource::kPasswordChange:
      return "PasswordChange";
    case mojom::InvocationSource::kPdfSummarizeButton:
      return "PdfSummarizeButton";
    case mojom::InvocationSource::kProfilePicker:
      return "ProfilePicker";
    case mojom::InvocationSource::kSharedImage:
      return "SharedImage";
    case mojom::InvocationSource::kSharedTab:
      return "SharedTab";
    case mojom::InvocationSource::kSkills:
      return "Skills";
    case mojom::InvocationSource::kTextSelectionNudge:
      return "TextSelectionNudge";
    case mojom::InvocationSource::kTextSelectionWidget:
      return "TextSelectionWidget";
    case mojom::InvocationSource::kThreeDotsMenu:
      return "ThreeDotsMenu";
    case mojom::InvocationSource::kTopChromeButton:
      return "TopChromeButton";
    case mojom::InvocationSource::kUniversalCart:
      return "UniversalCart";
    case mojom::InvocationSource::kExperimentalTriggering:
      return "ExperimentalTriggering";
    case mojom::InvocationSource::kUnsupported:
      return "Unsupported";
    case mojom::InvocationSource::kWebContentsContextMenu:
      return "WebContentsContextMenu";
    case mojom::InvocationSource::kWhatsNew:
      return "WhatsNew";
    case mojom::InvocationSource::kAutofill:
      return "Autofill";
    case mojom::InvocationSource::kZeroStateAutoSummarize:
      return "ZeroStateAutoSummarize";
  }
}
}  // namespace glic
