// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_USER_AGENT_TAB_HELPER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_USER_AGENT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace contextual_tasks {

// A WebContentsObserver tab helper that manages overriding the User-Agent
// string and metadata for WebContents navigating to AIM pages.
class AimUserAgentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AimUserAgentTabHelper> {
 public:
  AimUserAgentTabHelper(const AimUserAgentTabHelper&) = delete;
  AimUserAgentTabHelper& operator=(const AimUserAgentTabHelper&) = delete;
  ~AimUserAgentTabHelper() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit AimUserAgentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AimUserAgentTabHelper>;

  void UpdateUserAgentForNavigation(
      content::NavigationHandle* navigation_handle);

  bool is_ua_overridden_by_aim_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_AIM_USER_AGENT_TAB_HELPER_H_
