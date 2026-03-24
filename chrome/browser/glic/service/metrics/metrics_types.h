// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
#define CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_

#include <string>

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

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

// GlicEntrypoint defines entrypoints interesting from growth metrics
// perspective. It's a subset of InvocationSource, and more.
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
  kMaxValue = kWebContentsContextMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicEntrypoint)

GlicEntrypoint GetEntrypointFromInvocationSource(
    mojom::InvocationSource source);
std::string GetEntrypointString(GlicEntrypoint entrypoint);
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_METRICS_METRICS_TYPES_H_
