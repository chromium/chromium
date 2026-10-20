// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_response_completion_cui_tracker.h"

namespace glic {

GlicResponseCompletionCuiTracker::GlicResponseCompletionCuiTracker()
    : GlicCuiTracker("Glic.CUI.ResponseCompletion") {}

GlicResponseCompletionCuiTracker::~GlicResponseCompletionCuiTracker() = default;

std::optional<GlicCuiOutcome> GlicResponseCompletionCuiTracker::GetEventOutcome(
    GlicInstanceEvent event) const {
  switch (event) {
    case GlicInstanceEvent::kResponseStoppedSuccess:
      // `kResponseStoppedSuccess` is logged for `ResponseStopCause::kUnknown`,
      // which is simply an omitted `cause` (`cause === undefined`) from the web
      // client indicating normal, successful response completion.
      return GlicCuiOutcome::kSuccess;
    case GlicInstanceEvent::kResponseStoppedOther:
    case GlicInstanceEvent::kWebUiStateError:
    case GlicInstanceEvent::kWebUiStateOffline:
    case GlicInstanceEvent::kWebUiStateUnavailable:
    case GlicInstanceEvent::kWebUiStateUnresponsive:
    case GlicInstanceEvent::kWebUiStateGuestError:
    case GlicInstanceEvent::kWebUiStateDisabledByAdmin:
    case GlicInstanceEvent::kWebUiStateLocationMismatch:
    case GlicInstanceEvent::kWebUiStateIneligibleAccount:
      return GlicCuiOutcome::kFailed;
    case GlicInstanceEvent::kResponseStoppedByUser:
    case GlicInstanceEvent::kClose:
    case GlicInstanceEvent::kInstanceHidden:
      return GlicCuiOutcome::kAbandoned;
    default:
      return std::nullopt;
  }
}

}  // namespace glic
