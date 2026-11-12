// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_wayfinding_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {

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
  if (!base::FeatureList::IsEnabled(features::kGlicAskGeminiWayfinding)) {
    return;
  }
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

}  // namespace glic
