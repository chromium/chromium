// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"

class BrowserWindowInterface;

namespace safe_browsing {

// Test class to help create SafeBrowsingNavigationObservers for each
// WebContents before they are actually installed through AttachTabHelper.
class TestSafeBrowsingNavigationObserverManager
    : public SafeBrowsingNavigationObserverManager {
 public:
  explicit TestSafeBrowsingNavigationObserverManager(
      BrowserWindowInterface* browser);
  TestSafeBrowsingNavigationObserverManager(
      const TestSafeBrowsingNavigationObserverManager&) = delete;
  TestSafeBrowsingNavigationObserverManager& operator=(
      const TestSafeBrowsingNavigationObserverManager&) = delete;

  ~TestSafeBrowsingNavigationObserverManager() override;

  void ObserveContents(content::WebContents* contents);

  NavigationEventList* navigation_event_list() {
    return SafeBrowsingNavigationObserverManager::navigation_event_list();
  }

 private:
  base::CallbackListSubscription web_contents_creation_subscription_;
  std::vector<std::unique_ptr<SafeBrowsingNavigationObserver>> observer_list_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
