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

GlicEntrypoint GetEntrypointFromInvocationSource(
    mojom::InvocationSource source) {
  switch (source) {
    // OsButton sources bundles as one.
    case glic::mojom::InvocationSource::kOsButton:
    case glic::mojom::InvocationSource::kOsButtonMenu:
      return GlicEntrypoint::kOsButton;
    case glic::mojom::InvocationSource::kOsHotkey:
      return GlicEntrypoint::kOsHotkey;
    case glic::mojom::InvocationSource::kTopChromeButton:
      return GlicEntrypoint::kTopChromeButton;
    case glic::mojom::InvocationSource::kNudge:
      return GlicEntrypoint::kNudge;
    case glic::mojom::InvocationSource::kThreeDotsMenu:
      return GlicEntrypoint::kThreeDotsMenu;
    case glic::mojom::InvocationSource::kWhatsNew:
      return GlicEntrypoint::kWhatsNew;
    case glic::mojom::InvocationSource::kSharedTab:
      return GlicEntrypoint::kSharedTab;
    case glic::mojom::InvocationSource::kSharedImage:
      return GlicEntrypoint::kSharedImage;
    case glic::mojom::InvocationSource::kSkills:
      return GlicEntrypoint::kSkills;
    case glic::mojom::InvocationSource::kAutoOpenedByContextualCue:
      return GlicEntrypoint::kAutoOpenedByContextualCue;
    case glic::mojom::InvocationSource::kPdfSummarizeButton:
      return GlicEntrypoint::kPdfSummarizeButton;
    case glic::mojom::InvocationSource::kNavigationCapture:
      return GlicEntrypoint::kNavigationCapture;
    case glic::mojom::InvocationSource::kAutoOpenedForPdf:
      return GlicEntrypoint::kAutoOpenedForPdf;
    case glic::mojom::InvocationSource::kIph:
      return GlicEntrypoint::kIph;
    case glic::mojom::InvocationSource::kWebContentsContextMenu:
      return GlicEntrypoint::kWebContentsContextMenu;
    case glic::mojom::InvocationSource::kTextSelectionNudge:
      return GlicEntrypoint::kTextSelectionNudge;
    case glic::mojom::InvocationSource::kTextSelectionWidget:
      return GlicEntrypoint::kTextSelectionWidget;
    case glic::mojom::InvocationSource::kZeroStateAutoSummarize:
      return GlicEntrypoint::kZeroStateAutoSummarize;
    case glic::mojom::InvocationSource::kFre:
    case glic::mojom::InvocationSource::kProfilePicker:
    case glic::mojom::InvocationSource::kUnsupported:
    case glic::mojom::InvocationSource::kAfterSignIn:
    case glic::mojom::InvocationSource::kActorTaskIcon:
    case glic::mojom::InvocationSource::kHandoffButton:
    case glic::mojom::InvocationSource::kCaptureRegionHotkey:
    case glic::mojom::InvocationSource::kAnchoredContextualCue:
      return GlicEntrypoint::kOther;
  }
}

std::string GetEntrypointString(GlicEntrypoint entrypoint) {
  switch (entrypoint) {
    case GlicEntrypoint::kAutoOpenedByContextualCue:
      return "AutoOpenedByContextualCue";
    case GlicEntrypoint::kAutoOpenedForPdf:
      return "AutoOpenedForPdf";
    case GlicEntrypoint::kWebContentsContextMenu:
      return "WebContentsContextMenu";
    case GlicEntrypoint::kZeroStateAutoSummarize:
      return "ZeroStateAutoSummarize";
    case GlicEntrypoint::kIph:
      return "Iph";
    case GlicEntrypoint::kNavigationCapture:
      return "NavigationCapture";
    case GlicEntrypoint::kNudge:
      return "Nudge";
    case GlicEntrypoint::kOsButton:
      return "OsButton";
    case GlicEntrypoint::kOsHotkey:
      return "OsHotkey";
    case GlicEntrypoint::kOther:
      return "Other";
    case GlicEntrypoint::kPdfSummarizeButton:
      return "PdfSummarizeButton";
    case GlicEntrypoint::kSharedImage:
      return "SharedImage";
    case GlicEntrypoint::kSharedTab:
      return "SharedTab";
    case GlicEntrypoint::kSkills:
      return "Skills";
    case GlicEntrypoint::kThreeDotsMenu:
      return "ThreeDotsMenu";
    case GlicEntrypoint::kTopChromeButton:
      return "TopChromeButton";
    case GlicEntrypoint::kWhatsNew:
      return "WhatsNew";
    case GlicEntrypoint::kTextSelectionNudge:
      return "TextSelectionNudge";
    case GlicEntrypoint::kTextSelectionWidget:
      return "TextSelectionWidget";
  }
}
}  // namespace glic
