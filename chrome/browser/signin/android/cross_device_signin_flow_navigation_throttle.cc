// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/cross_device_signin_flow_navigation_throttle.h"

#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/signin/public/base/signin_deep_link_parser.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "url/gurl.h"

CrossDeviceSigninFlowNavigationThrottle::
    CrossDeviceSigninFlowNavigationThrottle(
        content::NavigationThrottleRegistry& registry,
        signin::SigninDeepLinkParser deep_link_parser)
    : content::NavigationThrottle(registry),
      deep_link_parser_(std::move(deep_link_parser)) {}

content::NavigationThrottle::ThrottleCheckResult
CrossDeviceSigninFlowNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();
  const auto payload = deep_link_parser_.Parse(url);
  if (payload.has_value() && payload->HasAllRequiredFields()) {
    // TODO(crbug.com/505626758): Handle the deep link payload.
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
CrossDeviceSigninFlowNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* CrossDeviceSigninFlowNavigationThrottle::GetNameForLogging() {
  return "CrossDeviceSigninFlowNavigationThrottle";
}

CrossDeviceSigninFlowNavigationThrottle::
    ~CrossDeviceSigninFlowNavigationThrottle() = default;

// static
void CrossDeviceSigninFlowNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  auto parser =
      signin::SigninDeepLinkParser::CreateForCrossDeviceSigninIfEnabled();
  if (!parser.has_value()) {
    return;
  }
  registry.AddThrottle(
      base::WrapUnique(new CrossDeviceSigninFlowNavigationThrottle(
          registry, std::move(parser.value()))));
}
