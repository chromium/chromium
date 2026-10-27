// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_fre_impression_cui_tracker.h"

namespace glic {

GlicFreImpressionCuiTracker::GlicFreImpressionCuiTracker()
    : GlicCuiTracker("Glic.CUI.FreImpression") {}

GlicFreImpressionCuiTracker::~GlicFreImpressionCuiTracker() = default;

std::optional<GlicCuiOutcome> GlicFreImpressionCuiTracker::GetEventOutcome(
    GlicInstanceEvent event) const {
  switch (event) {
    case GlicInstanceEvent::kOptInImpression:
      return GlicCuiOutcome::kSuccess;
    case GlicInstanceEvent::kInputFocused:
    case GlicInstanceEvent::kUserInputSubmitted:
      // The web client deferred the FRE and presented the prompt input box
      // first (e.g., Message-First FRE). Resolve as kUnknownCancel so user
      // typing time does not pollute upfront FRE rendering latency.
      return GlicCuiOutcome::kUnknownCancel;
    case GlicInstanceEvent::kWebUiStateError:
    case GlicInstanceEvent::kWebUiStateOffline:
    case GlicInstanceEvent::kWebUiStateUnavailable:
    case GlicInstanceEvent::kWebUiStateUnresponsive:
    case GlicInstanceEvent::kWebUiStateGuestError:
    case GlicInstanceEvent::kWebUiStateDisabledByAdmin:
    case GlicInstanceEvent::kWebUiStateLocationMismatch:
    case GlicInstanceEvent::kWebUiStateIneligibleAccount:
      return GlicCuiOutcome::kFailed;
    case GlicInstanceEvent::kClose:
    case GlicInstanceEvent::kInstanceHidden:
      return GlicCuiOutcome::kAbandoned;
    default:
      return std::nullopt;
  }
}

}  // namespace glic
