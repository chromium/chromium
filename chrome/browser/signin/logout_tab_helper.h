// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_LOGOUT_TAB_HELPER_H_
#define CHROME_BROWSER_SIGNIN_LOGOUT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Tab helper used for logout tabs. Monitors if the logout tab loaded correctly
// and fallbacks to local signout in case of failure.
// Only the first navigation is monitored. Even though the logout page sometimes
// redirects to the SAML provider through javascript, that second navigation is
// not monitored. The logout is considered successful if the first navigation
// succeeds, because the signout headers which cause the tokens to be revoked
// are there.
class LogoutTabHelper : public content::WebContentsUserData<LogoutTabHelper>,
                        public content::WebContentsObserver {
 public:
  ~LogoutTabHelper() override;

  LogoutTabHelper(const LogoutTabHelper&) = delete;
  LogoutTabHelper& operator=(const LogoutTabHelper&) = delete;

 private:
  friend class content::WebContentsUserData<LogoutTabHelper>;
  explicit LogoutTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SIGNIN_LOGOUT_TAB_HELPER_H_
