// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_window_invocation_tracker.h"

#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

GlicWindowInvocationTracker::GlicWindowInvocationTracker() = default;

GlicWindowInvocationTracker::~GlicWindowInvocationTracker() {
  if (!IsResolved()) {
    Resolve(GlicCuiOutcome::kUnknownCancel);
  }
}

const char* GlicWindowInvocationTracker::GetMetricName() const {
  return "Glic.CUI.WindowEntryPointInvocation";
}

std::optional<GlicCuiOutcome> GlicWindowInvocationTracker::GetEventOutcome(
    GlicInstanceEvent event) const {
  if (event == GlicInstanceEvent::kClientReady) {
    return GlicCuiOutcome::kSuccess;
  }

  if (event == GlicInstanceEvent::kClose) {
    return GlicCuiOutcome::kAbandoned;
  }

  if (event == GlicInstanceEvent::kWebUiStateError ||
      event == GlicInstanceEvent::kWebUiStateGuestError ||
      event == GlicInstanceEvent::kWebUiStateDisabledByAdmin ||
      event == GlicInstanceEvent::kWebUiStateOffline ||
      event == GlicInstanceEvent::kWebUiStateUnavailable ||
      event == GlicInstanceEvent::kWebUiStateIneligibleAccount ||
      event == GlicInstanceEvent::kWebUiStateLocationMismatch ||
      event == GlicInstanceEvent::kWebUiStateUnresponsive) {
    return GlicCuiOutcome::kFailed;
  }

  return std::nullopt;
}

}  // namespace glic
