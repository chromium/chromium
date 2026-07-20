// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_submit_query_cui_tracker.h"

namespace glic {

GlicSubmitQueryCuiTracker::GlicSubmitQueryCuiTracker()
    : GlicCuiTracker("Glic.CUI.SubmitQuery") {}

GlicSubmitQueryCuiTracker::~GlicSubmitQueryCuiTracker() = default;

std::optional<GlicCuiOutcome> GlicSubmitQueryCuiTracker::GetEventOutcome(
    GlicInstanceEvent event) const {
  switch (event) {
    case GlicInstanceEvent::kResponseStarted:
      return GlicCuiOutcome::kSuccess;
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
