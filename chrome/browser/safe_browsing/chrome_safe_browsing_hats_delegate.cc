// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/hats/hats_service.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate() = default;
ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate(
    HatsService* hats_service)
    : hats_service_(hats_service) {}

void ChromeSafeBrowsingHatsDelegate::LaunchRedWarningSurvey(
    const std::map<std::string, std::string>& product_specific_string_data) {
  if (!hats_service_) {
    return;
  }
  hats_service_->LaunchSurvey(
      kHatsSurveyTriggerRedWarning, /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(), /*product_specific_bits_data=*/{},
      product_specific_string_data);
}

}  // namespace safe_browsing
