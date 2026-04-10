// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/security_settings_bundle_toast_helper.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

// static
const void* const SecuritySettingsBundleToastHelper::kUserDataKey =
    &SecuritySettingsBundleToastHelper::kUserDataKey;

SecuritySettingsBundleToastHelper::SecuritySettingsBundleToastHelper(
    Profile* profile)
    : profile_(profile) {}

SecuritySettingsBundleToastHelper::~SecuritySettingsBundleToastHelper() =
    default;

// static
SecuritySettingsBundleToastHelper*
SecuritySettingsBundleToastHelper::GetForProfile(Profile* profile) {
  if (!profile->GetUserData(kUserDataKey)) {
    profile->SetUserData(
        kUserDataKey,
        std::make_unique<SecuritySettingsBundleToastHelper>(profile));
  }
  return static_cast<SecuritySettingsBundleToastHelper*>(
      profile->GetUserData(kUserDataKey));
}

void SecuritySettingsBundleToastHelper::TriggerIfNeeded() {
  if (profile_->GetPrefs()->GetInteger(
          prefs::kSecuritySettingsBundleMigrationToastState) !=
      static_cast<int>(SecuritySettingsBundleToastState::kPending)) {
    return;
  }

  SecuritySettingsBundleSetting bundle_setting =
      GetSecurityBundleSetting(*profile_->GetPrefs());
  base::UmaHistogramEnumeration(
      "SafeBrowsing.SecuritySettingsBundle."
      "BundleStateBeforeShowingEnhancedToast",
      bundle_setting);

  if (bundle_setting != SecuritySettingsBundleSetting::ENHANCED) {
    return;
  }
  TryShowToast();
}

void SecuritySettingsBundleToastHelper::SetToastControllerForTesting(
    ToastController* controller) {
  toast_controller_for_testing_ = controller;
}

void SecuritySettingsBundleToastHelper::TryShowToast() {
  ToastController* controller = GetToastController();
  if (controller && controller->MaybeShowToast(ToastParams(
                        ToastId::kEnhancedBundledSecuritySettings))) {
    profile_->GetPrefs()->SetInteger(
        prefs::kSecuritySettingsBundleMigrationToastState,
        static_cast<int>(SecuritySettingsBundleToastState::kShown));
    toast_retry_timer_.Stop();
    return;
  }

  if (retry_count_ >= kMaxRetries) {
    return;
  }

  retry_count_++;
  toast_retry_timer_.Start(
      FROM_HERE, kRetryDelay,
      base::BindOnce(&SecuritySettingsBundleToastHelper::TryShowToast,
                     base::Unretained(this)));
}

ToastController* SecuritySettingsBundleToastHelper::GetToastController() {
  if (toast_controller_for_testing_) {
    return toast_controller_for_testing_;
  }
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    return nullptr;
  }
  return browser->browser_window_features()->toast_controller();
}

}  // namespace safe_browsing
