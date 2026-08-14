// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURITY_STATE_EVENT_OBSERVER_H_
#define CHROME_BROWSER_SSL_SECURITY_STATE_EVENT_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

// Stateless navigation-driven side effects that used to live on
// ChromeSecurityStateTabHelper: showing the known-interception disclosure
// UI when a page loads with a known-interception certificate, and recording
// the HTTPS form submission UKM for the omnibox security indicator. Owned by
// the tab's TabFeatures and recreated when a discarded tab's WebContents is
// swapped; security state itself is computed on demand by
// chrome_security_state (see chrome_security_state_util.h).
class SecurityStateEventObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStateEventObserver(content::WebContents* web_contents);
  SecurityStateEventObserver(const SecurityStateEventObserver&) = delete;
  SecurityStateEventObserver& operator=(const SecurityStateEventObserver&) =
      delete;
  ~SecurityStateEventObserver() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
};

#endif  // CHROME_BROWSER_SSL_SECURITY_STATE_EVENT_OBSERVER_H_
