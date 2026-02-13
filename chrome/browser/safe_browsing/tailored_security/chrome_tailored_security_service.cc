// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/generated_security_settings_bundle_pref.h"
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
#include "chrome/browser/user_education/user_education_service_factory.h"
#endif

namespace safe_browsing {

#if BUILDFLAG(IS_ANDROID)
// Names for if the observer-based recovery mechanism is triggered.
const bool kRetryMechanismTriggered = true;
const bool kRetryMechanismNotTriggered = false;
#endif

namespace {
#if !BUILDFLAG(IS_ANDROID)
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kEnabledEnhancedBrowsingNotice);
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kDisabledEnhancedBrowsingNotice);
#endif

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
      profile_(profile),
      retry_handler_(std::make_unique<MessageRetryHandler>(
          profile_,
          prefs::kTailoredSecuritySyncFlowRetryState,
          prefs::kTailoredSecurityNextSyncFlowTimestamp,
          kRetryAttemptStartupDelay,
          kRetryNextAttemptDelay,
          kWaitingPeriodInterval,
          base::BindOnce(&ChromeTailoredSecurityService::
                             TailoredSecurityTimestampUpdateCallback,
                         base::Unretained(this)),
          "SafeBrowsing.TailoredSecurity.ShouldRetryOutcome",
          prefs::kAccountTailoredSecurityUpdateTimestamp,
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
  AddObserver(this);
  if (HistorySyncEnabledForUser() &&
      !SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          prefs())) {
    retry_handler_->StartRetryTimer();
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
    RegisterObserver();
    base::UmaHistogramBoolean(
        "SafeBrowsing.TailoredSecurity.IsRecoveryTriggered",
        kRetryMechanismTriggered);
    return;
  }
  base::UmaHistogramBoolean("SafeBrowsing.TailoredSecurity.IsRecoveryTriggered",
                            kRetryMechanismNotTriggered);

  // Since the Android UX is a notice, we simply set Safe Browsing state.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       is_enabled ? SafeBrowsingState::ENHANCED_PROTECTION
                                  : SafeBrowsingState::STANDARD_PROTECTION,
                       /*is_esb_enabled_by_account_integration=*/is_enabled);
  message_ = std::make_unique<TailoredSecurityConsentedModalAndroid>(
      web_contents, is_enabled,
      base::BindOnce(&ChromeTailoredSecurityService::MessageDismissed,
                     // Unretained is safe because |this| owns |message_|.
                     base::Unretained(this)),
      /*is_requested_by_synced_esb=*/false);
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
  // TODO(crbug.com/483786422): Register preference change handlers in each
  // relevant generated.*pref class that acts whenever the settings bundle
  // setting changes.
  if (base::FeatureList::IsEnabled(safe_browsing::kBundledSecuritySettings)) {
    PrefService* profile_pref = profile_->GetPrefs();
    bool tailored_security_pref_registered = profile_pref->FindPreference(
        prefs::kEnhancedProtectionEnabledViaTailoredSecurity);
    if (tailored_security_pref_registered) {
      profile_pref->SetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity, is_enabled);

      auto generated_pref =
          std::make_unique<GeneratedSecuritySettingsBundlePref>(profile_);
      generated_pref->SetPref(
          std::make_unique<base::Value>(
              static_cast<int>((is_enabled
                                    ? SecuritySettingsBundleSetting::ENHANCED
                                    : SecuritySettingsBundleSetting::STANDARD)))
              .get());
    }
  } else {
    SetSafeBrowsingState(profile_->GetPrefs(),
                         is_enabled ? SafeBrowsingState::ENHANCED_PROTECTION
                                    : SafeBrowsingState::STANDARD_PROTECTION,
                         /*is_esb_enabled_by_account_integration=*/is_enabled);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kNoticeQueueForEsb)) {
    QueueNotice(is_enabled);
  } else {
    DisplayDesktopDialog(browser, is_enabled);
  }

#endif
  retry_handler_->SaveRetryState(
      MessageRetryHandler::RetryState::NO_RETRY_NEEDED);

  if (is_enabled) {
    RecordEnabledNotificationResult(TailoredSecurityNotificationResult::kShown);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeTailoredSecurityService::TriggerDialogDisplay(
    bool is_enabled,
    user_education::RequiredNoticePriorityHandle messaging_priority_handle) {
  if (is_enabled) {
    enabled_notice_handle_ = std::move(messaging_priority_handle);
  } else {
    disabled_notice_handle_ = std::move(messaging_priority_handle);
  }
  DisplayDesktopDialog(chrome::FindBrowserWithProfile(profile_), is_enabled);
}

void ChromeTailoredSecurityService::ReleaseEnabledQueueHandle() {
  enabled_notice_handle_.Release();
}

void ChromeTailoredSecurityService::ReleaseDisabledQueueHandle() {
  disabled_notice_handle_.Release();
}

void ChromeTailoredSecurityService::QueueNotice(bool is_enabled) {
  // When we want to display the dialog, add it to the queue. More likely than
  // not, there will not be other items in the queue, so it was display
  // immediately. In edge cases, it will display after other entries in the
  // queue have processed.
  auto& product_messaging_controller =
      UserEducationServiceFactory::GetForBrowserContext(profile_)
          ->product_messaging_controller();

  const auto& notice_to_queue = is_enabled ? kEnabledEnhancedBrowsingNotice
                                           : kDisabledEnhancedBrowsingNotice;

  // We reference the handle of the opposite state (e.g., if enabling, we look
  // at the disabled handle).
  auto& other_notice_handle =
      is_enabled ? disabled_notice_handle_ : enabled_notice_handle_;

  if (!product_messaging_controller.IsNoticeQueued(notice_to_queue)) {
    // If the conflicting notice is currently held, release it so the new one
    // can process.
    if (other_notice_handle) {
      other_notice_handle.Release();
    }

    product_messaging_controller.QueueRequiredNotice(
        notice_to_queue,
        base::BindOnce(&ChromeTailoredSecurityService::TriggerDialogDisplay,
                       weak_factory_.GetWeakPtr(), is_enabled),
        {});
  }
}
#endif

#if BUILDFLAG(IS_ANDROID)
void ChromeTailoredSecurityService::DidAddTab(TabAndroid* tab,
                                              TabModel::TabLaunchType type) {
  // Stop observing because we can rely on the callback to start observing later
  // if it is needed.
  RemoveTabModelObserver();
  RemoveTabModelListObserver();
  TailoredSecurityTimestampUpdateCallback();
}

void ChromeTailoredSecurityService::OnTabModelAdded(TabModel* tab_model) {
  if (observed_tab_model_) {
    return;
  }

  AddTabModelObserver();
}

void ChromeTailoredSecurityService::OnTabModelRemoved(TabModel* tab_model) {
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
    dialog_manager_.ShowEnabledDialogForBrowser(
        browser, base::BindOnce(
                     &ChromeTailoredSecurityService::ReleaseEnabledQueueHandle,
                     weak_factory_.GetWeakPtr()));
  } else {
    dialog_manager_.ShowDisabledDialogForBrowser(
        browser, base::BindOnce(
                     &ChromeTailoredSecurityService::ReleaseDisabledQueueHandle,
                     weak_factory_.GetWeakPtr()));
  }
}
#endif

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace safe_browsing
