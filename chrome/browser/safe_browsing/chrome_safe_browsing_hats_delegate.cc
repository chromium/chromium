// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate(Profile* profile)
    : profile_(profile) {}

void ChromeSafeBrowsingHatsDelegate::LaunchRedWarningSurvey(
    const SurveyStringData& product_specific_string_data,
    const SurveyBitsData& product_specific_bits_data) {
  if (!profile_ || profile_->IsOffTheRecord()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  hats_service->LaunchSurvey(
      kHatsSurveyTriggerRedWarning, /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), product_specific_bits_data,
      product_specific_string_data);
}

}  // namespace safe_browsing
