// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_safe_browsing_hats_delegate.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegate::ChromeSafeBrowsingHatsDelegate(Profile* profile)
    : profile_(profile) {}

void ChromeSafeBrowsingHatsDelegate::LaunchRedWarningSurvey(
    const SurveyStringData& product_specific_string_data,
    const SurveyBitsData& product_specific_bits_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!profile_ || profile_->IsOffTheRecord()) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // On Android, HaTS survey prompt banner is presented as a native UI banner
  // attached to an active WebContents. Because the original interstitial tab
  // may have been closed or navigated away, find the currently active,
  // non-incognito tab to anchor the survey.
  content::WebContents* anchor_web_contents = nullptr;
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() == profile_ && !tab_model->IsOffTheRecord()) {
      if (content::WebContents* active_contents =
              tab_model->GetActiveWebContents()) {
        if (!active_contents->IsBeingDestroyed() &&
            !active_contents->GetBrowserContext()->IsOffTheRecord()) {
          anchor_web_contents = active_contents;
          break;
        }
      }
    }
  }

  if (!anchor_web_contents) {
    return;
  }

  SurveyStringData android_string_data = product_specific_string_data;
  internal::ReferringAppInfo referring_app_info =
      GetReferringAppInfo(anchor_web_contents, /*get_webapk_info=*/false);
  if (referring_app_info.has_referring_app()) {
    android_string_data[safe_browsing::kReferringApp] =
        referring_app_info.referring_app_name;
  }

  std::string trigger_id =
      safe_browsing::kRedWarningSurveyAndroidTriggerId.Get();
  if (trigger_id.empty()) {
    auto user_action_it = android_string_data.find(safe_browsing::kUserAction);
    bool did_proceed =
        user_action_it != android_string_data.end() &&
        user_action_it->second == safe_browsing::kUserActionProceed;
    trigger_id =
        did_proceed
            ? safe_browsing::kRedWarningSurveyAndroidProceedTriggerId.Get()
            : safe_browsing::kRedWarningSurveyAndroidHeedTriggerId.Get();
  }

  hats_service->LaunchSurveyForWebContents(
      kHatsSurveyTriggerRedWarningAndroid, anchor_web_contents,
      product_specific_bits_data, android_string_data,
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      trigger_id.empty() ? std::nullopt : std::optional(trigger_id),
      HatsService::SurveyOptions(
          /*custom_invitation=*/l10n_util::GetStringUTF16(
              IDS_SAFE_BROWSING_HATS_CUSTOM_INVITATION)));
#else
  // `product_specific_string_data` contains a superset of fields relevant to
  // HaTS surveys on desktop, so extract only the relevant subset for desktop.
  SurveyStringData desktop_string_data;
  for (const char* key :
       {safe_browsing::kFlaggedUrl, safe_browsing::kMainFrameUrl,
        safe_browsing::kReferrerUrl, safe_browsing::kUserActivityWithUrls}) {
    if (auto it = product_specific_string_data.find(key);
        it != product_specific_string_data.end()) {
      desktop_string_data.emplace(key, it->second);
    }
  }
  // Desktop HaTS red warning surveys do not currently attach any product
  // specific bits data.
  SurveyBitsData desktop_bits_data;
  hats_service->LaunchSurvey(kHatsSurveyTriggerRedWarning,
                             /*success_callback=*/base::DoNothing(),
                             /*failure_callback=*/base::DoNothing(),
                             desktop_bits_data, desktop_string_data);
#endif
}

}  // namespace safe_browsing
