// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"

class Browser;

namespace safe_browsing {

class InnerContentsCreationObserver : public content::WebContentsObserver {
 public:
  InnerContentsCreationObserver(
      content::WebContents* web_contents,
      base::RepeatingCallback<void(content::WebContents*)>
          on_inner_contents_created);

  ~InnerContentsCreationObserver() override;

  // WebContentsObserver:
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;

 private:
  base::RepeatingCallback<void(content::WebContents*)>
      on_inner_contents_created_;
};

// Test class to help create SafeBrowsingNavigationObservers for each
// WebContents before they are actually installed through AttachTabHelper.
class TestSafeBrowsingNavigationObserverManager
    : public SafeBrowsingNavigationObserverManager,
      public TabStripModelObserver {
 public:
  explicit TestSafeBrowsingNavigationObserverManager(Browser* browser);
  TestSafeBrowsingNavigationObserverManager(
      const TestSafeBrowsingNavigationObserverManager&) = delete;
  TestSafeBrowsingNavigationObserverManager& operator=(
      const TestSafeBrowsingNavigationObserverManager&) = delete;

  ~TestSafeBrowsingNavigationObserverManager() override;

  void ObserveContents(content::WebContents* contents);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  NavigationEventList* navigation_event_list() {
    return SafeBrowsingNavigationObserverManager::navigation_event_list();
  }

 private:
  std::vector<std::unique_ptr<SafeBrowsingNavigationObserver>> observer_list_;
  std::vector<std::unique_ptr<InnerContentsCreationObserver>>
      inner_contents_creation_observers_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_H_
