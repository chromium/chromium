// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_utils.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/promos/promos_features.h"
#include "chrome/browser/promos/promos_pref_names.h"

// IsActivationCriteriaOverridenIOSPasswordPromo returns true if the activation
// method of the flag is set to overridden (always show).
bool IsActivationCriteriaOverridenIOSPasswordPromo() {
  return base::FeatureList::IsEnabled(
             promos_features::kIOSPromoPasswordBubble) &&
         (promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kAlwaysShowWithPasswordBubbleDirect ||
          promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kAlwaysShowWithPasswordBubbleIndirect);
}

namespace promos_utils {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      promos_prefs::kiOSPasswordPromoLastImpressionTimestamp, base::Time(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      promos_prefs::kiOSPasswordPromoImpressionsCounter, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      promos_prefs::kiOSPasswordPromoOptOut, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

bool ShouldShowIOSPasswordPromo() {
  if (IsActivationCriteriaOverridenIOSPasswordPromo()) {
    return true;
  }

  // TODO: add business logic, feature is currently disabled by default

  return false;
}

bool IsDirectVariantIOSPasswordPromo() {
  return base::FeatureList::IsEnabled(
             promos_features::kIOSPromoPasswordBubble) &&
         (promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kContextualDirect ||
          promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kNonContextualDirect ||
          promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kAlwaysShowWithPasswordBubbleDirect);
}

bool IsIndirectVariantIOSPasswordPromo() {
  return base::FeatureList::IsEnabled(
             promos_features::kIOSPromoPasswordBubble) &&
         (promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kContextualIndirect ||
          promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kNonContextualIndirect ||
          promos_features::kIOSPromoPasswordBubbleActivationParam.Get() ==
              promos_features::IOSPromoPasswordBubbleActivation::
                  kAlwaysShowWithPasswordBubbleIndirect);
}
}  // namespace promos_utils
