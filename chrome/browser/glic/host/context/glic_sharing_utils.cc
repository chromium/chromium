// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace glic {

bool IsBrowserValidForSharingInProfile(
    BrowserWindowInterface* browser_interface,
    Profile* profile) {
  return browser_interface && profile &&
         browser_interface->GetProfile() == profile &&
         !profile->IsOffTheRecord();
}

bool IsTabValidForSharing(content::WebContents* web_contents) {
  // We allow allow blank pages to avoid flicker during transitions.
  static const base::NoDestructor<std::vector<GURL>> kUrlAllowList{
      {GURL(), GURL(url::kAboutBlankURL),
       GURL(chrome::kChromeUINewTabPageThirdPartyURL),
       GURL(chrome::kChromeUINewTabPageURL), GURL(chrome::kChromeUINewTabURL),
#if !BUILDFLAG(IS_CHROMEOS)
       // "What's New" does not exist in the form of a tab on ChromeOS.
       GURL(chrome::kChromeUIWhatsNewURL)
#endif
      }};
  if (!web_contents) {
    return false;
  }
  const GURL& url = web_contents->GetLastCommittedURL();
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile() ||
         base::Contains(*kUrlAllowList, url);
}

GlicPinEvent GetEmptyPinEvent() {
  return GlicPinEvent(GlicPinTrigger::kUnknown, base::TimeTicks::Now());
}

GlicPinnedTabUsage GetEmptyPinnedTabUsage() {
  return GlicPinnedTabUsage(GetEmptyPinEvent());
}

GlicUnpinEvent GetEmptyUnpinEvent() {
  return GlicUnpinEvent(GlicUnpinTrigger::kUnknown, GetEmptyPinnedTabUsage(),
                        base::TimeTicks::Now());
}

GlicActiveTabForProfileTracker::GlicActiveTabForProfileTracker(Profile* profile)
    : active_tab_changed_callback_list_(), profile_(profile) {
  BrowserList::AddObserver(this);
  // If we already have an active browser, set up active tab subscription.
  UpdateActiveTabSubscription(
      GetLastActiveBrowserWindowInterfaceWithAnyProfile());

  // Trigger an update now, even though we have no subscribers, so that
  // GetActiveTab works correctly.
  UpdateActiveTab();
}

GlicActiveTabForProfileTracker::~GlicActiveTabForProfileTracker() {
  BrowserList::RemoveObserver(this);
}

bool GlicActiveTabForProfileTracker::IsBrowserActiveForProfile(
    BrowserWindowInterface* browser) {
  return browser && browser->GetProfile() == profile_ && browser->IsActive();
}

void GlicActiveTabForProfileTracker::UpdateActiveTabSubscription(
    BrowserWindowInterface* browser) {
  if (IsBrowserActiveForProfile(browser)) {
    active_tab_subscription_ = browser->RegisterActiveTabDidChange(
        base::BindRepeating(&GlicActiveTabForProfileTracker::OnActiveTabChanged,
                            base::Unretained(this)));
  } else {
    active_tab_subscription_ = {};
  }
}

void GlicActiveTabForProfileTracker::OnBrowserSetLastActive(Browser* browser) {
  UpdateActiveTabSubscription(browser);
  UpdateActiveTab();
}

void GlicActiveTabForProfileTracker::OnBrowserNoLongerActive(Browser* browser) {
  active_tab_subscription_ = {};

  UpdateActiveTab();
}

void GlicActiveTabForProfileTracker::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  UpdateActiveTab();
}

void GlicActiveTabForProfileTracker::UpdateActiveTab() {
  tabs::TabInterface* active_tab = nullptr;

  BrowserWindowInterface* const browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (IsBrowserActiveForProfile(browser)) {
    active_tab = browser->GetActiveTabInterface();
  }

  if (last_notified_tab_.WasInvalidated() ||
      last_notified_tab_.get() != active_tab) {
    last_notified_tab_ = active_tab ? active_tab->GetWeakPtr()
                                    : base::WeakPtr<tabs::TabInterface>();
    NotifyActiveTabChanged(last_notified_tab_.get());
  }
}

base::CallbackListSubscription
GlicActiveTabForProfileTracker::AddActiveTabChangedCallback(
    base::RepeatingCallback<void(tabs::TabInterface*)> callback) {
  return active_tab_changed_callback_list_.Add(std::move(callback));
}

tabs::TabInterface* GlicActiveTabForProfileTracker::GetActiveTab() const {
  return last_notified_tab_.get();
}

void GlicActiveTabForProfileTracker::NotifyActiveTabChanged(
    tabs::TabInterface* active_tab) {
  active_tab_changed_callback_list_.Notify(active_tab);
}

}  // namespace glic
