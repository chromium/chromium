// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_notification_result.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#else
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog_manager.h"
#endif

namespace safe_browsing {

#if BUILDFLAG(IS_ANDROID)
// Names for if the observer-based recovery mechanism is triggered.
const bool kRetryMechanismTriggered = true;
const bool kRetryMechanismNotTriggered = false;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
content::WebContents* GetWebContentsForProfile(Profile* profile) {
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile) {
      continue;
    }

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents) {
        return web_contents;
      }
    }
  }
  return nullptr;
}
#endif

}  // namespace

ChromeTailoredSecurityService::ChromeTailoredSecurityService(Profile* profile)
    : TailoredSecurityService(IdentityManagerFactory::GetForProfile(profile),
                              SyncServiceFactory::GetForProfile(profile),
                              profile->GetPrefs()),
      profile_(profile) {
  AddObserver(this);
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityRetryForSyncUsers)) {
    if (HistorySyncEnabledForUser() &&
        !SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
            prefs())) {
      retry_timer_.Start(
          FROM_HERE, kRetryAttemptStartupDelay, this,
          &ChromeTailoredSecurityService::MaybeRetryForSyncUsers);
    }
  }
}

ChromeTailoredSecurityService::~ChromeTailoredSecurityService() {
  RemoveObserver(this);
#if BUILDFLAG(IS_ANDROID)
  RemoveTabModelObserver();
  RemoveTabModelListObserver();
#endif
}

void ChromeTailoredSecurityService::OnSyncNotificationMessageRequest(
    bool is_enabled) {
#if BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents = GetWebContentsForProfile(profile_);
  if (!web_contents) {
    if (base::FeatureList::IsEnabled(
            safe_browsing::kTailoredSecurityObserverRetries)) {
      RegisterObserver();
      base::UmaHistogramBoolean(
          "SafeBrowsing.TailoredSecurity.IsRecoveryTriggered",
          kRetryMechanismTriggered);
      return;
    }
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoWebContentsAvailable);
    }
    return;
  }
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityObserverRetries)) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.TailoredSecurity.IsRecoveryTriggered",
        kRetryMechanismNotTriggered);
  }

  // Since the Android UX is a notice, we simply set Safe Browsing state.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       is_enabled ? SafeBrowsingState::ENHANCED_PROTECTION
                                  : SafeBrowsingState::STANDARD_PROTECTION,
                       /*is_esb_enabled_in_sync=*/is_enabled);
  message_ = std::make_unique<TailoredSecurityConsentedModalAndroid>(
      web_contents, is_enabled,
      base::BindOnce(&ChromeTailoredSecurityService::MessageDismissed,
                     // Unretained is safe because |this| owns |message_|.
                     base::Unretained(this)));
#else
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoBrowserAvailable);
    }
    return;
  }
  if (!browser->window()) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoBrowserWindowAvailable);
    }
    return;
  }
  SetSafeBrowsingState(profile_->GetPrefs(),
                       is_enabled ? SafeBrowsingState::ENHANCED_PROTECTION
                                  : SafeBrowsingState::STANDARD_PROTECTION,
                       /*is_esb_enabled_in_sync=*/is_enabled);
  DisplayDesktopDialog(browser, is_enabled);
#endif
  SaveRetryState(TailoredSecurityRetryState::NO_RETRY_NEEDED);
  if (is_enabled) {
    RecordEnabledNotificationResult(TailoredSecurityNotificationResult::kShown);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeTailoredSecurityService::DidAddTab(TabAndroid* tab,
                                              TabModel::TabLaunchType type) {
  // Stop observing because we can rely on the callback to start observing later
  // if it is needed.
  RemoveTabModelObserver();
  RemoveTabModelListObserver();
  TailoredSecurityTimestampUpdateCallback();
}

void ChromeTailoredSecurityService::OnTabModelAdded() {
  if (observed_tab_model_) {
    return;
  }

  AddTabModelObserver();
}

void ChromeTailoredSecurityService::OnTabModelRemoved() {
  if (!observed_tab_model_) {
    return;
  }

  for (const TabModel* remaining_model : TabModelList::models()) {
    // We want to make sure our tab model is still not in the
    // tab model list, because we don't want to delete it
    // prematurely.
    if (observed_tab_model_ == remaining_model) {
      return;
    }
  }
  RemoveTabModelObserver();
}

void ChromeTailoredSecurityService::RegisterObserver() {
  AddTabModelObserver();
  AddTabModelListObserver();
}

void ChromeTailoredSecurityService::AddTabModelListObserver() {
  if (observing_tab_model_list_) {
    return;
  }
  TabModelList::AddObserver(this);
  observing_tab_model_list_ = true;
}

void ChromeTailoredSecurityService::AddTabModelObserver() {
  if (observed_tab_model_) {
    return;
  }
  for (TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_) {
      continue;
    }
    tab_model->AddObserver(this);
    // Saving the tab_model so we can stop observing the tab
    // model after we start a new tailored security logic sequence.
    observed_tab_model_ = tab_model;
    return;
  }
}

void ChromeTailoredSecurityService::RemoveTabModelListObserver() {
  observing_tab_model_list_ = false;
  TabModelList::RemoveObserver(this);
}

void ChromeTailoredSecurityService::RemoveTabModelObserver() {
  if (!observed_tab_model_) {
    return;
  }
  observed_tab_model_->RemoveObserver(this);
  observed_tab_model_ = nullptr;
}

void ChromeTailoredSecurityService::MessageDismissed() {
  message_.reset();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
void ChromeTailoredSecurityService::DisplayDesktopDialog(
    Browser* browser,
    bool show_enable_modal) {
  if (show_enable_modal) {
    dialog_manager_.ShowEnabledDialogForBrowser(browser);
  } else {
    dialog_manager_.ShowDisabledDialogForBrowser(browser);
  }
}
#endif

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

void ChromeTailoredSecurityService::SaveRetryState(
    TailoredSecurityRetryState state) {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityRetryForSyncUsers)) {
    profile_->GetPrefs()->SetInteger(prefs::kTailoredSecuritySyncFlowRetryState,
                                     state);
  }
}

void ChromeTailoredSecurityService::MaybeRetryForSyncUsers() {
  if (ShouldRetryForSyncUsers()) {
    TailoredSecurityTimestampUpdateCallback();
  }
}

bool ChromeTailoredSecurityService::ShouldRetryForSyncUsers() {
  PrefService* prefs = profile_->GetPrefs();
  if (prefs->GetTime(prefs::kAccountTailoredSecurityUpdateTimestamp) ==
      base::Time()) {
    // Do nothing because we can still rely on the user setting the tailored
    // security bit on their account settings in the future.
    return false;
  }
  if (prefs->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState) ==
      safe_browsing::NO_RETRY_NEEDED) {
    return false;
  }
  if (prefs->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState) ==
      safe_browsing::RETRY_NEEDED) {
    if (base::Time::Now() >=
        prefs->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp)) {
      // Set the next attempt time to a future point in time so that if this
      // retry attempt fails, enough time passes before retrying again.
      prefs->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                     base::Time::Now() + kRetryNextAttemptDelay);
      LogShouldRetryOutcome(
          TailoredSecurityShouldRetryOutcome::kRetryNeededDoRetry);
      return true;
    }
    LogShouldRetryOutcome(
        TailoredSecurityShouldRetryOutcome::kRetryNeededKeepWaiting);
    return false;
  }
  if (prefs->GetInteger(prefs::kTailoredSecuritySyncFlowRetryState) ==
          safe_browsing::UNSET &&
      !prefs->GetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
    // The stateful version of `ChromeTailoredSecurityService` has not run yet,
    // and we don't know if a previous version of the service showed the dialog
    // to the user in the past (pre-M106), so we need to wait long enough before
    // retrying.
    //
    // Chrome M106+ sets kEnhancedProtectionEnabledViaTailoredSecurity on
    // successfully showing a notification to the user, so we can guard this
    // logic based on that.
    if (prefs->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp) ==
        base::Time()) {
      prefs->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                     base::Time::Now() + kWaitingPeriodInterval);
      LogShouldRetryOutcome(
          TailoredSecurityShouldRetryOutcome::kUnsetInitializeWaitingPeriod);
      return false;
    } else if (base::Time::Now() >=
               prefs->GetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp)) {
      // Set the next attempt time to a future point in time so that if this
      // retry attempt fails, enough time passes before retrying again.
      prefs->SetTime(prefs::kTailoredSecurityNextSyncFlowTimestamp,
                     base::Time::Now() + kRetryNextAttemptDelay);
      LogShouldRetryOutcome(
          TailoredSecurityShouldRetryOutcome::kUnsetRetryBecauseDoneWaiting);
      return true;
    } else {
      LogShouldRetryOutcome(
          TailoredSecurityShouldRetryOutcome::kUnsetStillWaiting);
    }
  }
  return false;
}

void LogShouldRetryOutcome(
    ChromeTailoredSecurityService::TailoredSecurityShouldRetryOutcome outcome) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome", outcome);
}

}  // namespace safe_browsing
