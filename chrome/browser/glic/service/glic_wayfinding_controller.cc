// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_wayfinding_controller.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {

// At most one cue a day, and three per profile.
constexpr base::TimeDelta kMinTimeBetweenCues = base::Days(1);
constexpr int kMaxCuesPerProfile = 3;

// Right-click opens, the entry points version 1 of Wayfinding teaches.
bool IsRightClickOpen(mojom::InvocationSource source) {
  switch (source) {
    case mojom::InvocationSource::kWebContentsContextMenuLink:
    case mojom::InvocationSource::kWebContentsContextMenuPage:
    case mojom::InvocationSource::kWebContentsContextMenuTextSelection:
      return true;
    default:
      return false;
  }
}

// Opens that show the user already knows how to reach Glic on their own.
bool IsSelfStartedOpen(mojom::InvocationSource source) {
  switch (source) {
    case mojom::InvocationSource::kTopChromeButton:
    case mojom::InvocationSource::kToolbarButton:
    case mojom::InvocationSource::kNudge:
    case mojom::InvocationSource::kOsHotkey:
      return true;
    default:
      return false;
  }
}

}  // namespace

GlicWayfindingController::GlicWayfindingController(PrefService* pref_service,
                                                   bool was_opened_before)
    : pref_service_(pref_service), was_opened_before_(was_opened_before) {}

GlicWayfindingController::~GlicWayfindingController() = default;

void GlicWayfindingController::OnInvoke(mojom::InvocationSource source) {
  if (!was_opened_before_ &&
      pref_service_->GetInteger(prefs::kGlicWayfindingFirstInvocationSource) ==
          std::to_underlying(mojom::InvocationSource::kUnsupported)) {
    pref_service_->SetInteger(prefs::kGlicWayfindingFirstInvocationSource,
                              std::to_underlying(source));
  }
  if (IsSelfStartedOpen(source)) {
    pref_service_->SetBoolean(prefs::kGlicWayfindingUsedSelfStartedOpen, true);
  }
}

void GlicWayfindingController::OnUserLeftPanel(
    const GlicMetricReporter& reporter) {
  reporter.RecordEnumeration("Wayfinding.CueResult", MaybeCountCue());
}

bool GlicWayfindingController::IsEligible() const {
  if (pref_service_->GetBoolean(prefs::kGlicWayfindingUsedSelfStartedOpen)) {
    return false;
  }
  return IsRightClickOpen(static_cast<mojom::InvocationSource>(
      pref_service_->GetInteger(prefs::kGlicWayfindingFirstInvocationSource)));
}

WayfindingCueResult GlicWayfindingController::MaybeCountCue() {
  if (!IsEligible()) {
    return WayfindingCueResult::kNotEligible;
  }
  int cue_count = pref_service_->GetInteger(prefs::kGlicWayfindingCueCount);
  if (cue_count >= kMaxCuesPerProfile) {
    return WayfindingCueResult::kProfileCapReached;
  }
  const base::Time now = base::Time::Now();
  if (now - pref_service_->GetTime(prefs::kGlicWayfindingLastCueTime) <
      kMinTimeBetweenCues) {
    return WayfindingCueResult::kNotEnoughTimeBetweenCues;
  }
  pref_service_->SetInteger(prefs::kGlicWayfindingCueCount, cue_count + 1);
  pref_service_->SetTime(prefs::kGlicWayfindingLastCueTime, now);
  return WayfindingCueResult::kWouldCue;
}

}  // namespace glic
