// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_util.h"

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

namespace ash::fjord_util {

namespace {
static constexpr base::fixed_flat_set<std::string_view, 15>
    kFjordOobeAllowedLanguages = base::MakeFixedFlatSet<std::string_view>({
        "da",     // Danish
        "de",     // German
        "en-GB",  // English (UK)
        "en-US",  // English (US)
        "es",     // Spanish
        "fr",     // French
        "fr-CA",  // French (Canada)
        "it",     // Italian
        "ja",     // Japanese
        "ko",     // Korean
        "nb",     // Norwegian (Bokmal)
        "nl",     // Dutch
        "pl",     // Polish
        "sv",     // Swedish
        "zh-TW",  // Chinese (Traditional)
    });
}  // namespace

bool ShouldShowFjordOobe() {
  return features::IsFjordOobeForceEnabled() ||
         (policy::EnrollmentRequisitionManager::IsCuttlefishDevice() &&
          features::IsFjordOobeEnabled()) ||
         (policy::EnrollmentRequisitionManager::IsSquidDevice() &&
          features::IsFjordOobeForSquidEnabled());
}

bool ShouldShowFjordOobeImageSwitch() {
  return ShouldShowFjordOobe() && features::IsFjordOobeImageSwitchEnabled();
}

bool IsAllowlistedLanguage(std::string_view language_code) {
  return kFjordOobeAllowedLanguages.contains(language_code);
}

const base::fixed_flat_set<std::string_view, 15>&
GetAllowlistedLanguagesForTesting() {
  return kFjordOobeAllowedLanguages;
}

}  // namespace ash::fjord_util
