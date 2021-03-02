// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
const char kUserNeedsNotification[] =
    "previews.litepage.user-needs-notification";

// This WebContentsObserver watches the rest of the current navigation shows a
// notification to the user that this preview now exists and will be used on
// future eligible page loads. This is only done if the navigations finishes
// on the same URL as the one when it began. After finishing the navigation,
// |this| will be removed as an observer.
class UserNotificationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<UserNotificationWebContentsObserver> {
 public:
  void SetUIShownCallback(base::OnceClosure callback) {
    ui_shown_callback_ = std::move(callback);
  }

 private:
  friend class content::WebContentsUserData<
      UserNotificationWebContentsObserver>;

  explicit UserNotificationWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DestroySelf() {
    content::WebContents* old_web_contents = web_contents();
    Observe(nullptr);
    old_web_contents->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override {
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (ui_shown_callback_ && handle->GetNetErrorCode() == net::OK) {
      PreviewsLitePageInfoBarDelegate::Create(web_contents());
      std::move(ui_shown_callback_).Run();
    }
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  base::OnceClosure ui_shown_callback_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNotificationWebContentsObserver)

}  // namespace

PreviewsHTTPSNotificationInfoBarDecider::
    PreviewsHTTPSNotificationInfoBarDecider(
        content::BrowserContext* browser_context) {
  if (!browser_context)
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  if (!drp_settings)
    return;

  DCHECK(!browser_context->IsOffTheRecord());

  pref_service_ = profile->GetPrefs();

  // Add |this| as an observer to DRP, but if DRP is already initialized, check
  // the prefs now.
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
  if (drp_settings_->is_initialized()) {
    OnSettingsInitialized();
  }
}

void PreviewsHTTPSNotificationInfoBarDecider::Shutdown() {
  if (drp_settings_)
    drp_settings_->RemoveDataReductionProxySettingsObserver(this);
}

PreviewsHTTPSNotificationInfoBarDecider::
    ~PreviewsHTTPSNotificationInfoBarDecider() = default;

// static
void PreviewsHTTPSNotificationInfoBarDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kUserNeedsNotification, true);
}

void PreviewsHTTPSNotificationInfoBarDecider::OnSettingsInitialized() {
  // The notification only needs to be shown if the user has never seen it
  // before, and is an existing Data Saver user.
  if (!pref_service_->GetBoolean(kUserNeedsNotification)) {
    need_to_show_notification_ = false;
  } else if (drp_settings_->IsDataReductionProxyEnabled()) {
    need_to_show_notification_ = true;
  } else {
    need_to_show_notification_ = false;
    pref_service_->SetBoolean(kUserNeedsNotification, false);
  }
}

bool PreviewsHTTPSNotificationInfoBarDecider::NeedsToNotifyUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar)) {
    return false;
  }
  return need_to_show_notification_;
}

// Prompts |this| to display the required UI notifications to the user.
void PreviewsHTTPSNotificationInfoBarDecider::NotifyUser(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(need_to_show_notification_);
  DCHECK(!UserNotificationWebContentsObserver::FromWebContents(web_contents));

  UserNotificationWebContentsObserver::CreateForWebContents(web_contents);
  UserNotificationWebContentsObserver* observer =
      UserNotificationWebContentsObserver::FromWebContents(web_contents);

  // base::Unretained is safe here because |this| outlives |web_contents|.
  observer->SetUIShownCallback(base::BindOnce(
      &PreviewsHTTPSNotificationInfoBarDecider::SetUserHasSeenUINotification,
      base::Unretained(this)));
}

void PreviewsHTTPSNotificationInfoBarDecider::SetUserHasSeenUINotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  need_to_show_notification_ = false;
  pref_service_->SetBoolean(kUserNeedsNotification, false);
}
