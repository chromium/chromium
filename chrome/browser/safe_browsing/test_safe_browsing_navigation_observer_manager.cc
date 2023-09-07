// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_navigation_observer_manager.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

InnerContentsCreationObserver::InnerContentsCreationObserver(
    content::WebContents* web_contents,
    base::RepeatingCallback<void(content::WebContents*)>
        on_inner_contents_created)
    : content::WebContentsObserver(web_contents),
      on_inner_contents_created_(on_inner_contents_created) {}

InnerContentsCreationObserver::~InnerContentsCreationObserver() = default;

// WebContentsObserver:
void InnerContentsCreationObserver::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  on_inner_contents_created_.Run(inner_web_contents);
}

TestSafeBrowsingNavigationObserverManager::
    TestSafeBrowsingNavigationObserverManager(Browser* browser)
    : SafeBrowsingNavigationObserverManager(browser->profile()->GetPrefs(),
                                            browser->profile()
                                                ->GetDefaultStoragePartition()
                                                ->GetServiceWorkerContext()) {
  browser->tab_strip_model()->AddObserver(this);
}
TestSafeBrowsingNavigationObserverManager::
    ~TestSafeBrowsingNavigationObserverManager() = default;

void TestSafeBrowsingNavigationObserverManager::ObserveContents(
    content::WebContents* contents) {
  ASSERT_TRUE(contents);
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto observer = std::make_unique<SafeBrowsingNavigationObserver>(
      contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile));
  observer->SetObserverManagerForTesting(this);
  observer_list_.push_back(std::move(observer));
  inner_contents_creation_observers_.push_back(
      std::make_unique<InnerContentsCreationObserver>(
          contents,
          base::BindRepeating(
              &TestSafeBrowsingNavigationObserverManager::ObserveContents,
              // Unretained is safe because this object owns
              // inner_contents_creation_observers_
              base::Unretained(this))));
}

// TabStripModelObserver:
void TestSafeBrowsingNavigationObserverManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;

  for (const TabStripModelChange::ContentsWithIndex& tab :
       change.GetInsert()->contents) {
    ObserveContents(tab.contents);
  }
}

}  // namespace safe_browsing
