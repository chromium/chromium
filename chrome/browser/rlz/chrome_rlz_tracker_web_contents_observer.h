// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ChromeRLZTrackerWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ChromeRLZTrackerWebContentsObserver> {
 public:
  ~ChromeRLZTrackerWebContentsObserver() override;

  // Observes the web contents only if RLZ has not recorded that user has
  // performed a Google search from their Google homepage yet.
  static void CreateForWebContentsIfNeeded(content::WebContents* web_contents);

 private:
  explicit ChromeRLZTrackerWebContentsObserver(
      content::WebContents* web_contents);
  ChromeRLZTrackerWebContentsObserver(
      const ChromeRLZTrackerWebContentsObserver&) = delete;
  ChromeRLZTrackerWebContentsObserver& operator=(
      const ChromeRLZTrackerWebContentsObserver&) = delete;
  friend class content::WebContentsUserData<
      ChromeRLZTrackerWebContentsObserver>;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_
