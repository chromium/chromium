// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_

#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"

namespace safe_browsing {

// Provides embedder-specific logic for SafeBrowsingTabObserver.
class ChromeSafeBrowsingTabObserverDelegate
    : public SafeBrowsingTabObserver::Delegate {
 public:
  ChromeSafeBrowsingTabObserverDelegate();
  ~ChromeSafeBrowsingTabObserverDelegate() override;

  ChromeSafeBrowsingTabObserverDelegate(
      const ChromeSafeBrowsingTabObserverDelegate&) = delete;
  ChromeSafeBrowsingTabObserverDelegate& operator=(
      const ChromeSafeBrowsingTabObserverDelegate&) = delete;

  // SafeBrowsingTabObserver::Delegate:
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  ClientSideDetectionService* GetClientSideDetectionServiceIfExists(
      content::BrowserContext* browser_context) override;
  bool DoesSafeBrowsingServiceExist() override;
  std::unique_ptr<ClientSideDetectionHost> CreateClientSideDetectionHost(
      content::WebContents* web_contents) override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_TAB_OBSERVER_DELEGATE_H_
