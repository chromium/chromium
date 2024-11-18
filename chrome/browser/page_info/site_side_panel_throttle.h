// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_SITE_SIDE_PANEL_THROTTLE_H_
#define CHROME_BROWSER_PAGE_INFO_SITE_SIDE_PANEL_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

class GURL;

namespace content {
struct OpenURLParams;
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

extern const char kSiteSidePanelWebContentsUserDataKey[];

// Holds a handler to open a URL in a new tab in the browser that the sidepanel
// of this webcontents is associated with. The NavigationThrottle from
// |MaybeCreateSiteSidePanelThrottleFor| will check if this UserData is present
// and if it is present, intercepts navigations if |IsNavigationAllowed|
// and opens them using |OpenUrlInBrowser| instead.
class SiteSidePanelWebContentsUserData : public base::SupportsUserData::Data {
 public:
  class Delegate {
   public:
    virtual void OpenUrlInBrowser(const content::OpenURLParams& params) = 0;
    virtual bool IsNavigationAllowed(const GURL& new_url,
                                     const GURL& old_url) = 0;
  };

  explicit SiteSidePanelWebContentsUserData(base::WeakPtr<Delegate> delegate);
  ~SiteSidePanelWebContentsUserData() override;

  Delegate* delegate() { return delegate_.get(); }

 private:
  base::WeakPtr<Delegate> delegate_;
};

// Installs a NavigationThrottle if an SiteSidePanelWebContentsUserData is
// associated with the WebContents of this navigation.
std::unique_ptr<content::NavigationThrottle>
MaybeCreateSiteSidePanelThrottleFor(content::NavigationHandle* handle);

#endif  // CHROME_BROWSER_PAGE_INFO_SITE_SIDE_PANEL_THROTTLE_H_
