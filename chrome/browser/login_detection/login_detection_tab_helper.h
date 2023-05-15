// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_

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

  // Called when a new signed-in site has just been discovered from one of the
  // supported OAuth login flows.
  void ProcessNewSignedInSite(const GURL& signedin_site);

  // Detects successful OAuth login flows.
  std::unique_ptr<OAuthLoginDetector> oauth_login_detector_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_TAB_HELPER_H_
