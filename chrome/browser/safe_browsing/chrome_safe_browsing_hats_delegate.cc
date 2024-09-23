// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate() = default;
ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate(Profile* profile)
    : profile_(profile) {}

void ChromeSafeBrowsingHatsDelegate::LaunchRedWarningSurvey(
    const std::map<std::string, std::string>& product_specific_string_data) {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
  hats_service->LaunchSurvey(
      kHatsSurveyTriggerRedWarning, /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), /*product_specific_bits_data=*/{},
      product_specific_string_data);
}

}  // namespace safe_browsing
