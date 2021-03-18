// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_navigation_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {
namespace app_time {

// static
void WebTimeNavigationObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!WebTimeLimitEnforcer::IsEnabled())
    return;

  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(new WebTimeNavigationObserver(web_contents)));
  }
}

WebTimeNavigationObserver::~WebTimeNavigationObserver() = default;

void WebTimeNavigationObserver::AddObserver(
    WebTimeNavigationObserver::EventListener* listener) {
  listeners_.AddObserver(listener);
}

void WebTimeNavigationObserver::RemoveObserver(
    WebTimeNavigationObserver::EventListener* listener) {
  listeners_.RemoveObserver(listener);
}

bool WebTimeNavigationObserver::IsWebApp() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!web_app::AreWebAppsEnabled(profile))
    return false;
  const web_app::WebAppTabHelperBase* web_app_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents());
  return !web_app_helper->GetAppId().empty();
}

void WebTimeNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(yilkal): Handle case when navigation didn't happen in the main frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!last_navigation_info_.has_value())
    last_navigation_info_ = NavigationInfo();

  last_navigation_info_->navigation_finish_time = base::Time::Now();
  last_navigation_info_->is_error = navigation_handle->IsErrorPage();
  last_navigation_info_->is_web_app = IsWebApp();
  last_navigation_info_->url = navigation_handle->GetURL();
  last_navigation_info_->web_contents = web_contents();

  for (auto& listener : listeners_)
    listener.OnWebActivityChanged(last_navigation_info_.value());
}

void WebTimeNavigationObserver::WebContentsDestroyed() {
  for (auto& listener : listeners_)
    listener.WebTimeNavigationObserverDestroyed(this);
}

void WebTimeNavigationObserver::TitleWasSet(content::NavigationEntry* entry) {
  previous_title_ = web_contents()->GetTitle();
}

WebTimeNavigationObserver::WebTimeNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebTimeNavigationObserver)

}  // namespace app_time
}  // namespace chromeos
