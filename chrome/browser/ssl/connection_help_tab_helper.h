// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_
#define CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

// ConnectionHelpTabHelper checks URLs that trigger certificate error
// interstitials, and if a URL matches the connection help page of the help
// center, it redirects to chrome://connection-help. This allows users to view
// help content for certificate errors even when a certificate error is
// preventing them from accessing the live help center site.
class ConnectionHelpTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ConnectionHelpTabHelper> {
 public:
  ConnectionHelpTabHelper(const ConnectionHelpTabHelper&) = delete;
  ConnectionHelpTabHelper& operator=(const ConnectionHelpTabHelper&) = delete;

  ~ConnectionHelpTabHelper() override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Sets an alternate Help Center URL (the URL that will trigger the redirect)
  // for using in tests.
  void SetHelpCenterUrlForTesting(const GURL& url);

 private:
  friend class content::WebContentsUserData<ConnectionHelpTabHelper>;
  explicit ConnectionHelpTabHelper(content::WebContents* web_contents);

  GURL GetHelpCenterURL();

  GURL testing_url_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
#endif  // CHROME_BROWSER_SSL_CONNECTION_HELP_TAB_HELPER_H_
