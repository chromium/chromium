// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate_desktop.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegateDesktop::ChromeSafeBrowsingHatsDelegateDesktop(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

void ChromeSafeBrowsingHatsDelegateDesktop::LaunchRedWarningSurvey(
    SurveyStringData product_specific_string_data,
    SurveyBitsData /*product_specific_bits_data*/,
    bool /*is_tab_closed*/) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  // `product_specific_string_data` contains a superset of fields relevant to
  // HaTS surveys on desktop, so extract only the relevant subset for desktop.
  SurveyStringData desktop_string_data;
  for (const char* key :
       {kFlaggedUrl, kMainFrameUrl, kReferrerUrl, kUserActivityWithUrls}) {
    if (auto it = product_specific_string_data.find(key);
        it != product_specific_string_data.end()) {
      desktop_string_data.emplace(key, std::move(it->second));
    }
  }
  // Desktop HaTS red warning surveys do not currently attach any product
  // specific bits data.
  hats_service->LaunchSurvey(kHatsSurveyTriggerRedWarning,
                             /*success_callback=*/base::DoNothing(),
                             /*failure_callback=*/base::DoNothing(),
                             /*product_specific_bits_data=*/{},
                             desktop_string_data);
}

}  // namespace safe_browsing
