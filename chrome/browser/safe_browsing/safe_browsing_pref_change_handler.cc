// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#endif

namespace safe_browsing {

SafeBrowsingPrefChangeHandler::SafeBrowsingPrefChangeHandler() = default;
SafeBrowsingPrefChangeHandler::~SafeBrowsingPrefChangeHandler() = default;

// TODO(crbug.com/378888301): Add tests for Chrome Toast and Android modal
// logic.
void SafeBrowsingPrefChangeHandler::
    MaybeShowEnhancedProtectionSettingChangeNotification(Profile* profile) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  if (!base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting) ||
      !profile) {
    return;
  }
  Browser* const browser = chrome::FindBrowserWithProfile(profile);
  if (!browser) {
    return;
  }
  ToastController* const controller =
      browser->browser_window_features()->toast_controller();
  if (!controller) {
    return;
  }
  // We need to handle the toast for the security settings page differently:
  // 1. If the user has turned on ESB and is on the security page, we show
  // the toast but without the action button that directs users to the
  // settings page.
  // 2. If the user has turned off ESB and is on the security page, we do
  // not show a toast at all.
  TabStripModel* tab_strip_model = browser->GetTabStripModel();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  bool is_security_page =
      web_contents ? web_contents->GetLastCommittedURL().spec().starts_with(
                         "chrome://settings/security")
                   : false;

  // Extract the enhanced protection pref value.
  bool is_enhanced_enabled = IsEnhancedProtectionEnabled(*profile->GetPrefs());

  // The enhanced protection setting has been updated. To reflect this
  // change, we will show toasts to the user, taking into account both the
  // new setting value and whether they are currently on the settings page.
  if (is_enhanced_enabled) {
    // When the user is currently on the security settings page, show a
    // toast without the action button to go to the settings page.
    // Otherwise, we should a button that takes user to the settings page to
    // change the enhanced protection settings.
    controller->MaybeShowToast(
        ToastParams(is_security_page ? ToastId::kSyncEsbOnWithoutActionButton
                                     : ToastId::kSyncEsbOn));
  } else if (!is_security_page) {
    // Toast messages are not displayed on the security page when a user
    // disables a security setting. This applies whether the user disables
    // the setting on the current device or the change is synced from
    // another device.
    controller->MaybeShowToast(ToastParams(ToastId::kSyncEsbOff));
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting) ||
      !profile) {
    return;
  }
  content::WebContents* web_contents;
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile) {
      continue;
    }
    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents) {
        break;
      }
    }
  }

  if (!web_contents) {
    // TODO(crbug.com/389978258): Implement retry logic.
    return;
  } else {
    // Extract the enhanced protection pref value.
    bool is_enhanced_enabled =
        IsEnhancedProtectionEnabled(*profile->GetPrefs());
    message_ = std::make_unique<TailoredSecurityConsentedModalAndroid>(
        web_contents, is_enhanced_enabled,
        base::BindOnce(
            &SafeBrowsingPrefChangeHandler::ConsentedMessageDismissed,
            // Unretained is safe because |this| owns |message_|.
            base::Unretained(this)));
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingPrefChangeHandler::ConsentedMessageDismissed() {
  message_.reset();
}
#endif
}  // namespace safe_browsing
