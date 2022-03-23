// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/tailored_security_url_observer.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/tailored_security/notification_handler_desktop.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/render_widget_host_view.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/tailored_security/unconsented_message_android.h"
#else
#include "chrome/browser/ui/views/safe_browsing/tailored_security_unconsented_modal.h"
#endif

namespace safe_browsing {

namespace {

const int kThresholdForInFlowNotificationMinutes = 5;

bool CanQueryTailoredSecurityForUrl(GURL url) {
  return url.DomainIs("google.com") || url.DomainIs("youtube.com");
}

bool CanQueryTailoredSecurity(Profile* profile) {
  if (IsEnhancedProtectionEnabled(*profile->GetPrefs()))
    return false;

  // We should only trigger the unconsented UX if the user is not consented to
  // sync. Syncing users have different UX, handled by the
  // `ChromeTailoredSecurityService`.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kAccountTailoredSecurityShownNotification)) {
    return false;
  }

  if (SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          profile->GetPrefs())) {
    return false;
  }

  return true;
}

}  // namespace

TailoredSecurityUrlObserver::~TailoredSecurityUrlObserver() {
  if (service_) {
    service_->RemoveObserver(this);
    if (has_query_request_) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }
}

// content::WebContentsObserver:
void TailoredSecurityUrlObserver::PrimaryPageChanged(content::Page& page) {
  UpdateFocusAndURL(true, page.GetMainDocument().GetLastCommittedURL());
}

void TailoredSecurityUrlObserver::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  UpdateFocusAndURL(true, last_url_);
}

void TailoredSecurityUrlObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  UpdateFocusAndURL(false, last_url_);
}

void TailoredSecurityUrlObserver::OnTailoredSecurityBitChanged(
    bool enabled,
    base::Time previous_update) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!enabled || IsEnhancedProtectionEnabled(*profile->GetPrefs()))
    return;

  // We should only trigger the unconsented UX if the user is not consented to
  // sync. Syncing users have different UX, handled by the
  // `ChromeTailoredSecurityService`.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kAccountTailoredSecurityShownNotification)) {
    return;
  }

  if (SafeBrowsingPolicyHandler::IsSafeBrowsingProtectionLevelSetByPolicy(
          profile->GetPrefs())) {
    return;
  }

  profile->GetPrefs()->SetBoolean(
      prefs::kAccountTailoredSecurityShownNotification, true);

  if (base::Time::Now() - previous_update <=
      base::Minutes(kThresholdForInFlowNotificationMinutes)) {
#if BUILDFLAG(IS_ANDROID)
    message_ = std::make_unique<TailoredSecurityUnconsentedMessageAndroid>(
        web_contents(),
        base::BindOnce(&TailoredSecurityUrlObserver::MessageDismissed,
                       // Unretained is safe because |this| owns |message_|.
                       base::Unretained(this)),
        /*is_in_flow=*/true);
#else
      TailoredSecurityUnconsentedModal::ShowForWebContents(web_contents());
#endif
  } else {
#if BUILDFLAG(IS_ANDROID)
    message_ = std::make_unique<TailoredSecurityUnconsentedMessageAndroid>(
        web_contents(),
        base::BindOnce(&TailoredSecurityUrlObserver::MessageDismissed,
                       // Unretained is safe because |this| owns |message_|.
                       base::Unretained(this)),
        /*is_in_flow=*/false);
#else
    DisplayTailoredSecurityUnconsentedPromotionNotification(profile);
#endif
  }
}

void TailoredSecurityUrlObserver::OnTailoredSecurityServiceDestroyed() {
  service_->RemoveObserver(this);
  service_ = nullptr;
}

TailoredSecurityUrlObserver::TailoredSecurityUrlObserver(
    content::WebContents* web_contents,
    TailoredSecurityService* service)
    : WebContentsObserver(web_contents),
      WebContentsUserData(*web_contents),
      service_(service) {
  bool focused = false;

  if (service_) {
    service_->AddObserver(this);
  }

  if (web_contents && web_contents->GetMainFrame() &&
      web_contents->GetMainFrame()->GetView()) {
    focused = web_contents->GetMainFrame()->GetView()->HasFocus();
  }
  UpdateFocusAndURL(focused, web_contents->GetLastCommittedURL());
}

void TailoredSecurityUrlObserver::UpdateFocusAndURL(bool focused,
                                                    const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!CanQueryTailoredSecurity(profile)) {
    return;
  }

  if (service_) {
    bool should_query = focused && CanQueryTailoredSecurityForUrl(url);
    bool old_should_query =
        focused_ && CanQueryTailoredSecurityForUrl(last_url_);
    if (should_query && !old_should_query) {
      service_->AddQueryRequest();
      has_query_request_ = true;
    }
    if (!should_query && old_should_query) {
      service_->RemoveQueryRequest();
      has_query_request_ = false;
    }
  }

  focused_ = focused;
  last_url_ = url;
}

#if BUILDFLAG(IS_ANDROID)
void TailoredSecurityUrlObserver::MessageDismissed() {
  message_.reset();
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(TailoredSecurityUrlObserver);

}  // namespace safe_browsing
