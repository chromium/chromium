// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_WEB_VIEW_SIDE_PANEL_THROTTLE_H_
#define CHROME_BROWSER_PAGE_INFO_WEB_VIEW_SIDE_PANEL_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"

class GURL;

namespace content {
struct OpenURLParams;
class NavigationThrottle;
class NavigationThrottleRegistry;
}  // namespace content

extern const char kWebViewSidePanelWebContentsUserDataKey[];

// Holds a handler to open a URL in a new tab in the browser that the sidepanel
// of this webcontents is associated with. The NavigationThrottle from
// `MaybeCreateAndAddWebViewSidePanelThrottle` will check if this UserData is
// present and if it is present, intercepts navigations if `IsNavigationAllowed`
// and opens them using `OpenUrlInBrowser` instead.
class WebViewSidePanelWebContentsUserData
    : public base::SupportsUserData::Data {
 public:
  class Delegate {
   public:
    virtual void OpenUrlInBrowser(const content::OpenURLParams& params) = 0;
  };

  explicit WebViewSidePanelWebContentsUserData(
      base::WeakPtr<Delegate> delegate);
  ~WebViewSidePanelWebContentsUserData() override;

  Delegate* delegate() { return delegate_.get(); }

 private:
  base::WeakPtr<Delegate> delegate_;
};

// Installs a NavigationThrottle if an WebViewSidePanelWebContentsUserData is
// associated with the WebContents of this navigation.
void MaybeCreateAndAddWebViewSidePanelThrottle(
    content::NavigationThrottleRegistry& registry);

#endif  // CHROME_BROWSER_PAGE_INFO_WEB_VIEW_SIDE_PANEL_THROTTLE_H_
