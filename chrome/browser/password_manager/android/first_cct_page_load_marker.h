// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FIRST_CCT_PAGE_LOAD_MARKER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FIRST_CCT_PAGE_LOAD_MARKER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

// Attached to the WebContents when the CCT is initialized to be
// consumed by the password manager once the first page was loaded.
// It identifies the first page loaded by the CCT.
class FirstCctPageLoadMarker
    : public content::WebContentsUserData<FirstCctPageLoadMarker> {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  // Returns whether the `FirstCctPageLoadmarker` exists and removes
  // if from `web_contents` if it does.
  static bool ConsumeMarker(content::WebContents* web_contents);

  FirstCctPageLoadMarker(const FirstCctPageLoadMarker&) = delete;
  FirstCctPageLoadMarker& operator=(const FirstCctPageLoadMarker&) = delete;
  ~FirstCctPageLoadMarker() override;

 private:
  explicit FirstCctPageLoadMarker(content::WebContents* contents);

  friend class content::WebContentsUserData<FirstCctPageLoadMarker>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_FIRST_CCT_PAGE_LOAD_MARKER_H_
