// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "data_protection_navigation_controller.h"

namespace {
const char kWebContentsUserDataKey[] =
    "web_contents_data_protection_navigation_controller";

// The preferred way to fetch the browser pointer is using `FindBrowserWithTab`.
// However, there are some code paths where `TabHelpers` is constructed before
// the `WebContents` instance is attached to the tab. In the implementation
// below, we prioritize using the tab to obtain the `Browser` ptr, but fallback
// to using the profile to do so if that fails. This is a workaround that is
// required as long as the `DataProtectionNavigationController` is constructed
// by `TabHelpers`.
Browser* GetBrowser(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (browser) {
    return browser;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return nullptr;
  }
  return chrome::FindBrowserWithProfile(profile);
}
}  // namespace

namespace enterprise_data_protection {

DataProtectionNavigationController::DataProtectionNavigationController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

// static
void DataProtectionNavigationController::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (static_cast<DataProtectionNavigationController*>(
          web_contents->GetUserData(kWebContentsUserDataKey))) {
    return;
  }

  web_contents->SetUserData(
      kWebContentsUserDataKey,
      std::make_unique<DataProtectionNavigationController>(web_contents));
}

void DataProtectionNavigationController::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  Browser* browser = GetBrowser(web_contents());
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  enterprise_data_protection::DataProtectionNavigationObserver::
      CreateForNavigationIfNeeded(
          browser->profile(), navigation_handle,
          base::BindOnce(&BrowserView::DelayApplyDataProtectionSettingsIfEmpty,
                         browser_view->GetAsWeakPtr(),
                         web_contents()->GetWeakPtr()));
}

}  // namespace enterprise_data_protection
