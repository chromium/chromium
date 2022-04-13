// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_

#include "base/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class DIPSBounceDetector
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DIPSBounceDetector> {
 public:
  ~DIPSBounceDetector() override;
  DIPSBounceDetector(const DIPSBounceDetector&) = delete;
  DIPSBounceDetector& operator=(const DIPSBounceDetector&) = delete;

  using RedirectHandler =
      base::RepeatingCallback<void(content::NavigationHandle*, int)>;

  void SetStatefulRedirectHandlerForTesting(RedirectHandler handler) {
    stateful_redirect_handler_ = handler;
  }

 private:
  explicit DIPSBounceDetector(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<DIPSBounceDetector>;

  void HandleStatefulRedirect(content::NavigationHandle*, int);

  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // By default, this just calls this->HandleStatefulRedirect(), but it can be
  // overridden for tests.
  RedirectHandler stateful_redirect_handler_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BOUNCE_DETECTOR_H_
