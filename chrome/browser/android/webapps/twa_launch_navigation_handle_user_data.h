// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
#define CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_

#include "components/webapps/browser/launch_queue/launch_params.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace webapps {

enum class DigitalAssetLinksVerificationStatus {
  // Verification is in progress.
  kPending,
  // Verification succeeded.
  kSuccess,
  // Verification failed.
  kFailed
};

// Data that is attached to a NavigationHandle which describes what TWA app
// was launched as part of the navigation. When the navigation commits, this is
// used to enqueue launch params into the LaunchQueue once verification is
// complete.
//
// Expected Flow for a "normal" navigation:
// 1. Java generates a unique `launch_token` and calls C++ `PrepareForLaunch`,
//    stashing the launch parameters in `TwaLaunchQueueTabHelper` as `kPending`.
// 2. The navigation starts. C++ `DidStartNavigation` matches the navigation
//    by the token associated with the navigation via `ChromeNavigationUIData`
//    and attaches this `TwaLaunchNavigationHandleUserData` to the
//    `NavigationHandle`, tracking the `launch_token` and `kPending` status.
// 3. Java initiates Digital Asset Links (DAL) verification.
// 4. When DAL verification completes:
//    - If the navigation is still active: Java calls C++ `OnLaunchVerified`,
//      which updates the status in this user data to `kSuccess` (or
//      `kFailed`).
//    - If the navigation already committed: C++ `DidFinishNavigation` would
//      have moved the launch parameters to a committed list.
//      `OnLaunchVerified` will find them there and enqueue them.
class TwaLaunchNavigationHandleUserData
    : public content::NavigationHandleUserData<
          TwaLaunchNavigationHandleUserData> {
 public:
  ~TwaLaunchNavigationHandleUserData() override;

  // Unique token generated in Java to identify this specific launch.
  int64_t launch_token() const { return launch_token_; }
  const LaunchParams& launch_params() const { return launch_params_; }
  LaunchParams& launch_params() { return launch_params_; }

  DigitalAssetLinksVerificationStatus status() const { return status_; }
  void set_status(DigitalAssetLinksVerificationStatus status) {
    status_ = status;
  }

 private:
  TwaLaunchNavigationHandleUserData(
      content::NavigationHandle& navigation_handle,
      int64_t launch_token,
      LaunchParams launch_params,
      DigitalAssetLinksVerificationStatus status);

  friend NavigationHandleUserData;

  int64_t launch_token_;
  LaunchParams launch_params_;
  DigitalAssetLinksVerificationStatus status_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_ANDROID_WEBAPPS_TWA_LAUNCH_NAVIGATION_HANDLE_USER_DATA_H_
