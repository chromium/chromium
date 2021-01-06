// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/login_detection/oauth_login_detector.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace login_detection {

// Observes the navigations and records login detection metrics.
class LoginDetectionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LoginDetectionTabHelper> {
 public:
  // Enumerates the different types of user log-in that can be detected on a
  // page based on the site (effective TLD+1). This is recorded in metrics and
  // should not be reordered or removed. Should be in sync with the same name in
  // enums.xml
  enum LoginDetectionType {
    // No login was detected.
    kNoLogin,

    // OAuth login was detected for this site, and was remembered in persistent
    // memory.
    kOauthLogin,

    // Successful OAuth login flow was detected.
    kOauthFirstTimeLoginFlow,

    
    // The user had typed password to log-in. This includes sites where user
    // typed password manually or used Chrome password manager to fill-in.
    kPasswordEnteredLogin,

    // The site is in one of preloaded top sites where users commonly log-in.
    kPreloadedPasswordSiteLogin,

    // Treated as logged-in since as the site was retrieved from field trial as
    // commonly logged-in.
    kFieldTrialLoggedInSite,

    // The site has credentials saved in the password manager.
    kPasswordManagerSavedSite,

    // Successful popup based OAuth login flow was detected.
    kOauthPopUpFirstTimeLoginFlow,

    kMaxValue = kOauthPopUpFirstTimeLoginFlow
  };

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~LoginDetectionTabHelper() override;
  LoginDetectionTabHelper(const LoginDetectionTabHelper&) = delete;
  LoginDetectionTabHelper& operator=(const LoginDetectionTabHelper&) = delete;

 private:
  friend class content::WebContentsUserData<LoginDetectionTabHelper>;

  explicit LoginDetectionTabHelper(content::WebContents* web_contents);

  // Indicates this WebContents was opened as a popup, and the opener
  // WebContents had the |opener_navigation_url|.
  void DidOpenAsPopUp(const GURL& opener_navigation_url);

  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void WebContentsDestroyed() override;

  // Detects successful OAuth login flows.
  std::unique_ptr<OAuthLoginDetector> oauth_login_detector_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_
