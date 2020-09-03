// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/https_image_compression_infobar_decider.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#endif

namespace {
// Pref key that stores whether the user has already seen the infobar. The pref
// is initialized as false, and updated to true when LiteMode is enabled and
// infobar has been shown to user.
constexpr char kHasSeenInfoBar[] =
    "litemode.https-image-compression.user-has-seen-infobar";

// The time used to compare and identify recent LiteMode users. Users who
// enabled LiteMode before this time are treated as non-recent and the one-time
// https image compression InfoBar is shown for them. Set approximate as M85
// release date, which is the target for https image compression feature.
constexpr char kRecentLiteModeUserEnableTime[] = "2020-08-25T00:00:01Z";

}  // namespace

HttpsImageCompressionInfoBarDecider::HttpsImageCompressionInfoBarDecider(
    PrefService* pref_service,
    data_reduction_proxy::DataReductionProxySettings* drp_settings)
    : pref_service_(pref_service) {
  if (!pref_service_ || !drp_settings)
    return;
  // The infobar only needs to be shown if the user has never seen it before,
  // is an existing LiteMode user, and did not recently enable LiteMode.
  need_to_show_infobar_ =
      base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
      drp_settings->IsDataReductionProxyEnabled() &&
      !pref_service_->GetBoolean(kHasSeenInfoBar);
  if (need_to_show_infobar_) {
    const auto last_enabled_time = drp_settings->GetLastEnabledTime();
    if (!last_enabled_time.is_null()) {
      base::Time recent_lite_mode_user_enable_time;
      bool success = base::Time::FromUTCString(
          kRecentLiteModeUserEnableTime, &recent_lite_mode_user_enable_time);
      DCHECK(success);
      need_to_show_infobar_ =
          last_enabled_time < recent_lite_mode_user_enable_time;
    }
  }
}

// static
void HttpsImageCompressionInfoBarDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kHasSeenInfoBar, false);
}

bool HttpsImageCompressionInfoBarDecider::NeedToShowInfoBar() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kOverrideHttpsImageCompressionInfobar)) {
    return false;
  }
  return need_to_show_infobar_;
}

bool HttpsImageCompressionInfoBarDecider::CanShowInfoBar(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect));
  if (!navigation_handle->GetURL().SchemeIs(url::kHttpsScheme))
    return false;
#if defined(OS_ANDROID)
  auto* tab_android =
      TabAndroid::FromWebContents(navigation_handle->GetWebContents());
  if (!tab_android || tab_android->IsCustomTab())
    return false;
#endif
  return true;
}

void HttpsImageCompressionInfoBarDecider::SetUserHasSeenInfoBar() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  need_to_show_infobar_ = false;
  pref_service_->SetBoolean(kHasSeenInfoBar, true);
}
