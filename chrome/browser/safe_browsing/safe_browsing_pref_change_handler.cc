// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/service/sync_service.h"
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
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#endif

namespace safe_browsing {

SafeBrowsingPrefChangeHandler::SafeBrowsingPrefChangeHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
#if BUILDFLAG(IS_ANDROID)
  retry_handler_ = std::make_unique<MessageRetryHandler>(
      profile_, prefs::kSafeBrowsingSyncedEnhancedProtectionRetryState,
      prefs::kSafeBrowsingSyncedEnhancedProtectionNextRetryTimestamp,
      kRetryAttemptStartupDelay, kRetryNextAttemptDelay, kWaitingPeriodInterval,
      base::BindOnce(&SafeBrowsingPrefChangeHandler::RetryStateCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      "SafeBrowsing.EnhancedProtection.ShouldRetryOutcome",
      prefs::kSafeBrowsingSyncedEnhancedProtectionUpdateTimestamp,
      prefs::kEnhancedProtectionEnabledViaTailoredSecurity);
  retry_handler_->StartRetryTimer();
#endif
}

SafeBrowsingPrefChangeHandler::~SafeBrowsingPrefChangeHandler() {
#if BUILDFLAG(IS_ANDROID)
  RemoveTabModelListObserver();
  RemoveTabModelObserver();
#endif
}

// TODO(crbug.com/378888301): Add tests for Chrome Toast and Android modal
// logic.
void SafeBrowsingPrefChangeHandler::
    MaybeShowEnhancedProtectionSettingChangeNotification() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  if (!profile_ ||
      !base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    return;
  }

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service && sync_service->IsSyncFeatureEnabled()) {
    base::Time tailored_security_update_time = profile_->GetPrefs()->GetTime(
        prefs::kAccountTailoredSecurityUpdateTimestamp);
    if (tailored_security_update_time.is_null()) {
      // The TailoredSecurityService has never run. Suppress this
      // toast, which is reacting to stale data. When a user enables ESB through
      // the Tailored Security flow, the
      // `kEnhancedProtectionEnabledViaTailoredSecurity` pref is set to true.
      // This allows us to distinguish between changes made by Tailored Security
      // and changes that are synced from other devices. But if the update time
      // is null, this means the Tailored Security service hasn't run yet.
      return;
    }
  }

  // Do not show a notification toast if the setting is managed by enterprise
  // policy.
  if (safe_browsing::IsSafeBrowsingPolicyManaged(*profile_->GetPrefs())) {
    return;
  }
  Browser* const browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    return;
  }

  // TODO(crbug.com/447592206): The correct long term solution is to refactor
  // ToastController to use the UnownedUserData factory pattern for testability,
  // which will be handled in a follow-up CL.
  ToastController* const controller =
      toast_controller_for_testing_
          ? static_cast<ToastController*>(toast_controller_for_testing_)
          : browser->browser_window_features()->toast_controller();
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
  bool is_enhanced_enabled = IsEnhancedProtectionEnabled(*profile_->GetPrefs());

  if (is_enhanced_enabled &&
      profile_->GetPrefs()->GetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
    // The TailoredSecurityService just ran and showed its modal. Suppress this
    // toast.
    return;
  }

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

// TODO(crbug.com/397966486): Add tests in the android test file.
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting) ||
      !profile_) {
    return;
  }
  content::WebContents* web_contents = nullptr;
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_) {
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
    // Instantiate the retry handler here, if it hasn't been already
    profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingSyncedEnhancedProtectionRetryState,
        static_cast<int>(MessageRetryHandler::RetryState::RETRY_NEEDED));
    if (!retry_handler_) {
      retry_handler_ = std::make_unique<MessageRetryHandler>(
          profile_, prefs::kSafeBrowsingSyncedEnhancedProtectionRetryState,
          prefs::kSafeBrowsingSyncedEnhancedProtectionNextRetryTimestamp,
          kRetryAttemptStartupDelay, kRetryNextAttemptDelay,
          kWaitingPeriodInterval,
          base::BindOnce(&SafeBrowsingPrefChangeHandler::RetryStateCallback,
                         weak_ptr_factory_.GetWeakPtr()),
          "SafeBrowsing.EnhancedProtection.ShouldRetryOutcome",
          prefs::kSafeBrowsingSyncedEnhancedProtectionUpdateTimestamp,
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity);
      retry_handler_->StartRetryTimer();
    }
    RegisterObserver();
  } else {
    // Do not show the notification modal if the user set the setting locally on
    // this device.
    if (profile_->GetPrefs()->GetBoolean(
            prefs::kSafeBrowsingSyncedEnhancedProtectionSetLocally)) {
      profile_->GetPrefs()->SetBoolean(
          prefs::kSafeBrowsingSyncedEnhancedProtectionSetLocally, false);
      retry_handler_->SaveRetryState(
          MessageRetryHandler::RetryState::NO_RETRY_NEEDED);
      return;
    }
    // Extract the enhanced protection pref value.
    bool is_enhanced_enabled =
        IsEnhancedProtectionEnabled(*profile_->GetPrefs());
    message_ = std::make_unique<TailoredSecurityConsentedModalAndroid>(
        web_contents, is_enhanced_enabled,
        base::BindOnce(
            &SafeBrowsingPrefChangeHandler::ConsentedMessageDismissed,
            weak_ptr_factory_.GetWeakPtr()),
        /*is_requested_by_synced_esb=*/true);
    if (retry_handler_) {
      retry_handler_->SaveRetryState(
          MessageRetryHandler::RetryState::NO_RETRY_NEEDED);
    }
  }
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
void SafeBrowsingPrefChangeHandler::SetToastControllerForTesting(
    ToastController* controller) {
  toast_controller_for_testing_ = controller;
}
#endif

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingPrefChangeHandler::RetryStateCallback() {
  profile_->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingSyncedEnhancedProtectionRetryState,
      static_cast<int>(MessageRetryHandler::RetryState::RETRY_NEEDED));
  MaybeShowEnhancedProtectionSettingChangeNotification();
}

void SafeBrowsingPrefChangeHandler::SetTabModelForTesting(TabModel* tab_model) {
  observed_tab_model_ = tab_model;
}

bool SafeBrowsingPrefChangeHandler::IsObservingTabModelListForTesting() const {
  return observing_tab_model_list_;
}

bool SafeBrowsingPrefChangeHandler::IsObservingTabModelForTesting() const {
  return observed_tab_model_ != nullptr;
}

void SafeBrowsingPrefChangeHandler::DidAddTab(TabAndroid* tab,
                                              TabModel::TabLaunchType type) {
  RemoveTabModelObserver();
  RemoveTabModelListObserver();
  // Get the Profile from the TabAndroid
  if (!tab || !tab->web_contents()) {
    return;
  }
  RetryStateCallback();
}

void SafeBrowsingPrefChangeHandler::OnTabModelAdded(TabModel* tab_model) {
  if (observed_tab_model_) {
    return;
  }
  if (TabModelList::models().empty()) {
    return;
  }
  AddTabModelObserver();
}

void SafeBrowsingPrefChangeHandler::OnTabModelRemoved(TabModel* tab_model) {
  RemoveTabModelObserver();
}

void SafeBrowsingPrefChangeHandler::RegisterObserver() {
  AddTabModelListObserver();
  AddTabModelObserver();
}

void SafeBrowsingPrefChangeHandler::AddTabModelListObserver() {
  if (observing_tab_model_list_) {
    return;
  }
  TabModelList::AddObserver(this);
  observing_tab_model_list_ = true;
}

void SafeBrowsingPrefChangeHandler::AddTabModelObserver() {
  if (observed_tab_model_) {
    return;
  }
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_) {
      continue;
    }
    tab_model->AddObserver(this);
    // Saving the tab_model so we can stop observing the tab model after a
    // tabmodel is added.
    observed_tab_model_ = tab_model;
    return;
  }
}

void SafeBrowsingPrefChangeHandler::RemoveTabModelListObserver() {
  observing_tab_model_list_ = false;
  TabModelList::RemoveObserver(this);
}

void SafeBrowsingPrefChangeHandler::RemoveTabModelObserver() {
  if (!observed_tab_model_) {
    return;
  }
  observed_tab_model_->RemoveObserver(this);
  observed_tab_model_ = nullptr;
}

void SafeBrowsingPrefChangeHandler::ConsentedMessageDismissed() {
  message_.reset();
}
#endif
}  // namespace safe_browsing
