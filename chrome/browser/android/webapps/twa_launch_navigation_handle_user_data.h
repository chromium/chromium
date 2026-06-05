// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_

#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace webapps {

// Data that is attached to a NavigationHandle which describes what TWA app
// was launched as part of the navigation. When the navigation commits, this is
// used to enqueue launch params into the LaunchQueue.
class TwaLaunchNavigationHandleUserData
    : public content::NavigationHandleUserData<
          TwaLaunchNavigationHandleUserData> {
 public:
  ~TwaLaunchNavigationHandleUserData() override;

  // Static helper for non-navigating launches to enqueue launch params
  // directly without waiting for navigation to commit.
  static void EnqueueNonNavigating(content::WebContents* web_contents,
                                   LaunchParams launch_params);

  const LaunchParams& launch_params() const { return launch_params_; }
  LaunchParams& launch_params() { return launch_params_; }

 private:
  TwaLaunchNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      LaunchParams launch_params);

  friend NavigationHandleUserData;

  LaunchParams launch_params_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
