// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace ash::app_time {

DEFINE_USER_DATA(WebTimeNavigationObserver);

// static
std::unique_ptr<WebTimeNavigationObserver>
WebTimeNavigationObserver::MaybeCreate(tabs::TabInterface& tab,
                                       content::WebContents* web_contents) {
  CHECK(web_contents, base::NotFatalUntil::M160);
  if (!base::FeatureList::IsEnabled(
          ash::features::kUnicornChromeActivityReporting)) {
    return nullptr;
  }

  return base::WrapUnique(new WebTimeNavigationObserver(tab, web_contents));
}

// static
WebTimeNavigationObserver* WebTimeNavigationObserver::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
WebTimeNavigationObserver* WebTimeNavigationObserver::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  return From(tabs::TabInterface::MaybeGetFromContents(web_contents));
}

// static
const WebTimeNavigationObserver* WebTimeNavigationObserver::FromWebContents(
    const content::WebContents* web_contents) {
  return FromWebContents(const_cast<content::WebContents*>(web_contents));
}

WebTimeNavigationObserver::~WebTimeNavigationObserver() {
  if (web_contents()) {
    for (auto& listener : listeners_) {
      listener.WebTimeNavigationObserverDestroyed(this);
    }
  }
}

void WebTimeNavigationObserver::OnDiscardContents(
    content::WebContents* new_contents) {
  Observe(new_contents);
}

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
  Observe(nullptr);
}

WebTimeNavigationObserver::WebTimeNavigationObserver(
    tabs::TabInterface& tab,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

}  // namespace ash::app_time
