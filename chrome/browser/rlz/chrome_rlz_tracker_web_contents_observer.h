// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"

class ChromeRLZTrackerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  ChromeRLZTrackerWebContentsObserver(
      const ChromeRLZTrackerWebContentsObserver&) = delete;
  ChromeRLZTrackerWebContentsObserver& operator=(
      const ChromeRLZTrackerWebContentsObserver&) = delete;
  ~ChromeRLZTrackerWebContentsObserver() override;

  // Observes the web contents only if RLZ has not recorded that user has
  // performed a Google search from their Google homepage yet.
  static std::unique_ptr<ChromeRLZTrackerWebContentsObserver> MaybeCreate(
      content::WebContents* web_contents);

 private:
  explicit ChromeRLZTrackerWebContentsObserver(
      content::WebContents* web_contents);

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
};

#endif  // CHROME_BROWSER_RLZ_CHROME_RLZ_TRACKER_WEB_CONTENTS_OBSERVER_H_
