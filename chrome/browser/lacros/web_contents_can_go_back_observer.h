// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_WEB_CONTENTS_CAN_GO_BACK_OBSERVER_H_
#define CHROME_BROWSER_LACROS_WEB_CONTENTS_CAN_GO_BACK_OBSERVER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// An instance of this class is created for every WebContents instance.
// It observes changes to its respective WebContents' visible state and
// back/forward list. This information is forwarded to ash-chrome, through
// Wayland.
//
// With such information ash-chrome decides whether it should or
// not "minimize the window on back gestore", among other scenarios.
class WebContentsCanGoBackObserver
    : public content::WebContentsUserData<WebContentsCanGoBackObserver>,
      public content::WebContentsObserver {
 public:
  ~WebContentsCanGoBackObserver() override;

  // Ensures an instance of this class for the given |web_contents| is created.
  // Note that the returned metrics is owned by the web contents.
  static WebContentsCanGoBackObserver* CreateForWebContents(
      content::WebContents* web_contents);

 private:
  explicit WebContentsCanGoBackObserver(content::WebContents* web_contents);
  WebContentsCanGoBackObserver(const WebContentsCanGoBackObserver&) = delete;
  WebContentsCanGoBackObserver& operator=(const WebContentsCanGoBackObserver&) =
      delete;

  friend class content::WebContentsUserData<WebContentsCanGoBackObserver>;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  void UpdateLatestFocusedWebContentsStatus();

  bool visible_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_LACROS_WEB_CONTENTS_CAN_GO_BACK_OBSERVER_H_
