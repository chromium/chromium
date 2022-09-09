// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_MESSAGE_DELEGATE_H_

#include <string>

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class Page;
}

class InstantAppsMessageDelegate : public content::WebContentsObserver {
 public:
  InstantAppsMessageDelegate(const InstantAppsMessageDelegate&) = delete;
  InstantAppsMessageDelegate& operator=(const InstantAppsMessageDelegate&) =
      delete;

  ~InstantAppsMessageDelegate() override;

  explicit InstantAppsMessageDelegate(content::WebContents* web_contents,
                                      jobject jdelegate,
                                      const std::string& url);

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  std::string url_;
  bool user_navigated_away_from_launch_url_;
};

#endif  // CHROME_BROWSER_ANDROID_INSTANTAPPS_INSTANT_APPS_MESSAGE_DELEGATE_H_
