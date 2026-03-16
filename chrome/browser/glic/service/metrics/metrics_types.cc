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
    default:
      return "Unknown";
  }
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
    default:
      // All other ones, including mojom::InvocationSource::kUnsupported.
      return GlicEntrypoint::kOther;
  }
}

}  // namespace glic
