// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/chrome_safe_browsing_hats_delegate_android.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

ChromeSafeBrowsingHatsDelegateAndroid::ChromeSafeBrowsingHatsDelegateAndroid(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}

ChromeSafeBrowsingHatsDelegateAndroid::
    ~ChromeSafeBrowsingHatsDelegateAndroid() = default;

void ChromeSafeBrowsingHatsDelegateAndroid::LaunchRedWarningSurvey(
    SurveyStringData product_specific_string_data,
    SurveyBitsData product_specific_bits_data,
    bool is_tab_closed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // When a tab is closed, TabModelList has not yet updated its active tab state
  // synchronously. Post a task to allow the next tab activation to complete
  // before querying TabModelList.
  if (is_tab_closed) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ChromeSafeBrowsingHatsDelegateAndroid::
                                      LaunchRedWarningSurveyInternal,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(product_specific_string_data),
                                  std::move(product_specific_bits_data),
                                  /*is_tab_closed=*/true));
    return;
  }

  LaunchRedWarningSurveyInternal(std::move(product_specific_string_data),
                                 std::move(product_specific_bits_data),
                                 /*is_tab_closed=*/false);
}

void ChromeSafeBrowsingHatsDelegateAndroid::LaunchRedWarningSurveyInternal(
    SurveyStringData product_specific_string_data,
    SurveyBitsData product_specific_bits_data,
    bool is_tab_closed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // On Android, HaTS survey prompt banner is presented as a native UI banner
  // attached to an active WebContents. Because the original interstitial tab
  // may have been closed or navigated away, find the currently active tab
  // to anchor the survey.
  content::WebContents* anchor_web_contents = nullptr;
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_ || !tab_model->IsActiveModel()) {
      continue;
    }
    content::WebContents* active_contents = tab_model->GetActiveWebContents();
    if (active_contents && !active_contents->IsBeingDestroyed()) {
      anchor_web_contents = active_contents;
      break;
    }
  }

  if (!anchor_web_contents) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  // Only query anchor_web_contents for referring app if the tab was NOT closed.
  // When a tab is closed, anchor_web_contents is an adjacent tab whose
  // referring app may be completely unrelated to the interstitial tab.
  if (!is_tab_closed) {
    internal::ReferringAppInfo referring_app_info =
        GetReferringAppInfo(anchor_web_contents);
    if (referring_app_info.has_referring_app()) {
      product_specific_string_data[safe_browsing::kReferringApp] =
          std::move(referring_app_info.referring_app_name);
    }
  }

  std::string trigger_id =
      safe_browsing::kRedWarningSurveyAndroidTriggerId.Get();
  if (trigger_id.empty()) {
    auto user_action_it =
        product_specific_string_data.find(safe_browsing::kUserAction);
    bool did_proceed =
        user_action_it != product_specific_string_data.end() &&
        user_action_it->second == safe_browsing::kUserActionProceed;
    trigger_id =
        did_proceed
            ? safe_browsing::kRedWarningSurveyAndroidProceedTriggerId.Get()
            : safe_browsing::kRedWarningSurveyAndroidHeedTriggerId.Get();
  }

  hats_service->LaunchSurveyForWebContents(
      kHatsSurveyTriggerRedWarningAndroid, anchor_web_contents,
      std::move(product_specific_bits_data),
      std::move(product_specific_string_data),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      trigger_id.empty() ? std::nullopt : std::optional(std::move(trigger_id)),
      HatsService::SurveyOptions(
          /*custom_invitation=*/l10n_util::GetStringUTF16(
              IDS_SAFE_BROWSING_HATS_CUSTOM_INVITATION)));
}

internal::ReferringAppInfo
ChromeSafeBrowsingHatsDelegateAndroid::GetReferringAppInfo(
    content::WebContents* web_contents) {
  if (referring_app_name_for_testing_.has_value()) {
    internal::ReferringAppInfo info;
    info.referring_app_name = *referring_app_name_for_testing_;
    return info;
  }
  return safe_browsing::GetReferringAppInfo(web_contents,
                                            /*get_webapk_info=*/false);
}

}  // namespace safe_browsing
