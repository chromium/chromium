// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_navigation_observer_manager.h"

#include <memory>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TestSafeBrowsingNavigationObserverManager::
    TestSafeBrowsingNavigationObserverManager(BrowserWindowInterface* browser)
    : SafeBrowsingNavigationObserverManager(browser->GetProfile()->GetPrefs(),
                                            browser->GetProfile()
                                                ->GetDefaultStoragePartition()
                                                ->GetServiceWorkerContext()) {
  // Observe WebContents creation directly so that new WebContents (including
  // new tabs opened via `Navigate()`, e.g. Ctrl+click, as well as inner
  // WebContents) have a test `SafeBrowsingNavigationObserver` attached before
  // `DidStartNavigation()` fires.
  web_contents_creation_subscription_ =
      content::RegisterWebContentsCreationCallback(base::BindRepeating(
          &TestSafeBrowsingNavigationObserverManager::ObserveContents,
          base::Unretained(this)));
}
TestSafeBrowsingNavigationObserverManager::
    ~TestSafeBrowsingNavigationObserverManager() = default;

void TestSafeBrowsingNavigationObserverManager::ObserveContents(
    content::WebContents* contents) {
  ASSERT_TRUE(contents);
  // Ignore WebUI WebContents (such as the omnibox popup's
  // `chrome://omnibox-popup.top-chrome/` WebContents created on demand when the
  // omnibox is used in browser tests).
  if (contents->GetSiteInstance()->GetSecurityPrincipal().IsWebUI()) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto observer = std::make_unique<SafeBrowsingNavigationObserver>(
      contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile));
  observer->SetObserverManagerForTesting(this);
  observer_list_.push_back(std::move(observer));
}

}  // namespace safe_browsing
