// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_notification_result.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#endif

namespace safe_browsing {

namespace {

#if BUILDFLAG(IS_ANDROID)
content::WebContents* GetWebContentsForProfile(Profile* profile) {
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile)
      continue;

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents)
        return web_contents;
    }
  }
  return nullptr;
}
#endif

}  // namespace

// Records an UMA Histogram value to count the result of trying to notify a sync
// user about enhanced protection for the enable case.
void RecordEnabledNotificationResult(
    TailoredSecurityNotificationResult result) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.TailoredSecurity.SyncPromptEnabledNotificationResult",
      result);
}

ChromeTailoredSecurityService::ChromeTailoredSecurityService(Profile* profile)
    : TailoredSecurityService(IdentityManagerFactory::GetForProfile(profile),
                              profile->GetPrefs()),
      profile_(profile) {}

ChromeTailoredSecurityService::~ChromeTailoredSecurityService() = default;

void ChromeTailoredSecurityService::MaybeNotifySyncUser(
    bool is_enabled,
    base::Time previous_update) {
  if (!base::FeatureList::IsEnabled(kTailoredSecurityIntegration))
    return;

  if (!identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kAccountNotConsented);
    }
    return;
  }

  if (SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          profile_->GetPrefs())) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kSafeBrowsingControlledByPolicy);
    }
    return;
  }

  if (is_enabled && IsEnhancedProtectionEnabled(*prefs())) {
    RecordEnabledNotificationResult(
        TailoredSecurityNotificationResult::kEnhancedProtectionAlreadyEnabled);
  }

  if (is_enabled && !IsEnhancedProtectionEnabled(*prefs())) {
    ShowSyncNotification(true);
  }

  if (!is_enabled && IsEnhancedProtectionEnabled(*prefs()) &&
      prefs()->GetBoolean(
          prefs::kEnhancedProtectionEnabledViaTailoredSecurity)) {
    ShowSyncNotification(false);
  }
}

void ChromeTailoredSecurityService::ShowSyncNotification(bool is_enabled) {
#if BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents = GetWebContentsForProfile(profile_);
  if (!web_contents) {
    if (is_enabled) {
      RecordEnabledNotificationResult(
          TailoredSecurityNotificationResult::kNoWebContentsAvailable);
    }
    return;
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
  DisplayTailoredSecurityConsentedModalDesktop(profile_, is_enabled);
#endif
  if (is_enabled) {
    RecordEnabledNotificationResult(TailoredSecurityNotificationResult::kShown);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeTailoredSecurityService::MessageDismissed() {
  message_.reset();
}
#endif

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

}  // namespace safe_browsing
