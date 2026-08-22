// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// ConnectionHelpTabHelper checks URLs that trigger certificate error
// interstitials, and if a URL matches the connection help page of the help
// center, it redirects to chrome://connection-help. This allows users to view
// help content for certificate errors even when a certificate error is
// preventing them from accessing the live help center site.
class ConnectionHelpTabHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(ConnectionHelpTabHelper);

  // `web_contents` is passed explicitly because during a discard the helper
  // is recreated for the incoming WebContents before `tab` swaps its
  // contents.
  ConnectionHelpTabHelper(tabs::TabInterface& tab,
                          content::WebContents* web_contents);

  ConnectionHelpTabHelper(const ConnectionHelpTabHelper&) = delete;
  ConnectionHelpTabHelper& operator=(const ConnectionHelpTabHelper&) = delete;

  ~ConnectionHelpTabHelper() override;

  static ConnectionHelpTabHelper* From(tabs::TabInterface* tab);

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Sets an alternate Help Center URL (the URL that will trigger the redirect)
  // for using in tests.
  void SetHelpCenterUrlForTesting(const GURL& url);

 private:
  GURL GetHelpCenterURL();

  GURL testing_url_;
  ui::ScopedUnownedUserData<ConnectionHelpTabHelper> scoped_unowned_user_data_;
};
#endif  // CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_
