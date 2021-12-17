// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/chrome_tailored_security_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#endif

namespace safe_browsing {

namespace {

#if defined(OS_ANDROID)
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

  if (!identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return;

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
#if defined(OS_ANDROID)
  content::WebContents* web_contents = GetWebContentsForProfile(profile_);
  if (!web_contents)
    return;

  // Since the Android UX is a notice, we simply enable Enhanced Protection
  // here.
  SetSafeBrowsingState(profile_->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION,
                       /*is_esb_enabled_in_sync=*/true);

  message_ = std::make_unique<TailoredSecurityConsentedModalAndroid>();
  message_->DisplayMessage(
      web_contents, is_enabled,
      base::BindOnce(&ChromeTailoredSecurityService::MessageDismissed,
                     // Unretained is safe because |this| owns |message_|.
                     base::Unretained(this)));
#else
  DisplayTailoredSecurityConsentedModalDesktop(profile_, is_enabled);
#endif
}

#if defined(OS_ANDROID)
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
