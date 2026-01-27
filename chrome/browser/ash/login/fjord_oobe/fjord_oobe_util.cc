// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_util.h"

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

namespace ash::fjord_util {

namespace {
static constexpr base::fixed_flat_set<std::string_view, 7>
    kFjordOobeAllowedLanguages = base::MakeFixedFlatSet<std::string_view>(
        {"en", "en-GB", "de", "fr", "ja", "fr-CA", "es"});
}

bool ShouldShowFjordOobe() {
  return features::IsFjordOobeForceEnabled() ||
         (policy::EnrollmentRequisitionManager::IsCuttlefishDevice() &&
          features::IsFjordOobeEnabled());
}

bool IsAllowlistedLanguage(std::string_view language_code) {
  return kFjordOobeAllowedLanguages.contains(language_code.data());
}

const base::fixed_flat_set<std::string_view, 7>&
GetAllowlistedLanguagesForTesting() {
  return kFjordOobeAllowedLanguages;
}

}  // namespace ash::fjord_util
