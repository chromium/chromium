// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_CROSS_DEVICE_SIGNIN_FLOW_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_CROSS_DEVICE_SIGNIN_FLOW_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "components/signin/public/base/signin_deep_link_parser.h"
#include "content/public/browser/navigation_throttle.h"

class SigninBridge;
namespace content {
class NavigationThrottleRegistry;
}

namespace signin {
class IdentityManager;
}

// A navigation throttle that intercepts requests to the cross-device signin
// deep link URL.
//
// This throttle is used to cancel cross-device signin deep link requests and
// handle them with the native flow.
class CrossDeviceSigninFlowNavigationThrottle
    : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  CrossDeviceSigninFlowNavigationThrottle(
      const CrossDeviceSigninFlowNavigationThrottle&) = delete;
  CrossDeviceSigninFlowNavigationThrottle& operator=(
      const CrossDeviceSigninFlowNavigationThrottle&) = delete;

  ~CrossDeviceSigninFlowNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  friend class CrossDeviceSigninFlowNavigationThrottleUnitTest;
  friend class CrossDeviceSigninFlowNavigationThrottleTabClosingUnitTest;

  CrossDeviceSigninFlowNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      SigninBridge* signin_bridge,
      signin::IdentityManager* identity_manager,
      signin::SigninDeepLinkParser deep_link_parser);

  void ClosePageIfNeeded();

  raw_ptr<SigninBridge> signin_bridge_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  signin::SigninDeepLinkParser deep_link_parser_;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_CROSS_DEVICE_SIGNIN_FLOW_NAVIGATION_THROTTLE_H_
