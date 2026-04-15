// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/security_settings_bundle_pref_change_handler.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#include "chrome/browser/safe_browsing/security_settings_bundle_toast_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

namespace safe_browsing {

SecuritySettingsBundlePrefChangeHandler::
    SecuritySettingsBundlePrefChangeHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}
SecuritySettingsBundlePrefChangeHandler::
    ~SecuritySettingsBundlePrefChangeHandler() = default;

void SecuritySettingsBundlePrefChangeHandler::
    MaybeShowEnhancedBundleSettingChangeNotification() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  if (!profile_ ||
      !base::FeatureList::IsEnabled(safe_browsing::kBundledSecuritySettings)) {
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  if (GetSecurityBundleSetting(*prefs) !=
      SecuritySettingsBundleSetting::ENHANCED) {
    return;
  }

  if (prefs->IsManagedPreference(prefs::kSecuritySettingsBundle)) {
    return;
  }

  ToastController* const controller = GetToastController();
  if (!controller) {
    return;
  }
  controller->MaybeShowToast(
      ToastParams(ToastId::kSyncEsbOnWithoutActionButton));
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
void SecuritySettingsBundlePrefChangeHandler::SetToastControllerForTesting(
    ToastController* controller) {
  toast_controller_for_testing_ = controller;
}

ToastController* SecuritySettingsBundlePrefChangeHandler::GetToastController() {
  if (toast_controller_for_testing_) {
    return toast_controller_for_testing_;
  }
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    return nullptr;
  }
  return browser->browser_window_features()->toast_controller();
}
#endif
}  // namespace safe_browsing
