// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace ash::app_time {

// static
void WebTimeNavigationObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (!base::FeatureList::IsEnabled(
          features::kUnicornChromeActivityReporting)) {
    return;
  }

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
  if (!web_app::AreWebAppsEnabled(profile)) {
    return false;
  }
  return web_app::WebAppTabHelper::GetAppId(web_contents()) != nullptr;
}

void WebTimeNavigationObserver::PrimaryPageChanged(content::Page& page) {
  NavigationInfo info;
  info.navigation_finish_time = base::Time::Now();
  info.is_error = page.GetMainDocument().IsErrorDocument();
  info.is_web_app = IsWebApp();
  info.url = page.GetMainDocument().GetLastCommittedURL();
  info.web_contents = web_contents();

  for (auto& listener : listeners_) {
    listener.OnWebActivityChanged(info);
  }
}

void WebTimeNavigationObserver::WebContentsDestroyed() {
  for (auto& listener : listeners_) {
    listener.WebTimeNavigationObserverDestroyed(this);
  }
}

WebTimeNavigationObserver::WebTimeNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsUserData<WebTimeNavigationObserver>(*web_contents),
      content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebTimeNavigationObserver);

}  // namespace ash::app_time
