// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace data_sharing {

// static
std::unique_ptr<content::NavigationThrottle>
DataSharingNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    return std::make_unique<DataSharingNavigationThrottle>(handle);
  }
  return nullptr;
}

DataSharingNavigationThrottle::DataSharingNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::WillStartRequest() {
  return CheckIfShouldIntercept();
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::WillRedirectRequest() {
  return CheckIfShouldIntercept();
}

const char* DataSharingNavigationThrottle::GetNameForLogging() {
  return "DataSharingNavigationThrottle";
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::CheckIfShouldIntercept() {
  // TODO(haileywang): Call data sharing service to know if the url should be
  // intercepted.
  return PROCEED;
}

}  // namespace data_sharing
