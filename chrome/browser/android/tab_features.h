// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_H_

#include <memory>

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace metrics {
class DwaWebContentsObserver;
}  // namespace metrics

namespace privacy_sandbox {
class PrivacySandboxIncognitoTabObserver;
}  // namespace privacy_sandbox

namespace tabs {

// This class holds state that is scoped to a tab in Android. It is constructed
// after the WebContents/tab_helpers, and destroyed before.
class TabFeatures {
 public:
  TabFeatures(content::WebContents* web_contents, Profile* profile);
  ~TabFeatures();

 private:
  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;
  std::unique_ptr<metrics::DwaWebContentsObserver> dwa_web_contents_observer_;
  std::unique_ptr<privacy_sandbox::PrivacySandboxIncognitoTabObserver>
      privacy_sandbox_incognito_tab_observer_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
