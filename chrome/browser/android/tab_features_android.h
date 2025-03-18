// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_ANDROID_H_

#include <memory>

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

// This class holds state that is scoped to a tab in Android. It is constructed
// after the WebContents/tab_helpers, and destroyed before.
class TabFeaturesAndroid {
 public:
  TabFeaturesAndroid(content::WebContents* web_contents, Profile* profile);
  ~TabFeaturesAndroid();

 private:
  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_ANDROID_H_
