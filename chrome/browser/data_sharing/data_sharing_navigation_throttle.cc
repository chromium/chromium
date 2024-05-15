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

void DataSharingNavigationThrottle::SetServiceForTesting(
    DataSharingService* test_service) {
  test_service_ = test_service;
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::CheckIfShouldIntercept() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return PROCEED;
  }

  DataSharingService* data_sharing_service =
      DataSharingServiceFactory::GetForProfile(Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));

  if (test_service_) {
    data_sharing_service = test_service_;
  }

  const GURL& url = navigation_handle()->GetURL();
  if (data_sharing_service &&
      data_sharing_service->ShouldInterceptNavigationForShareURL(url)) {
    data_sharing_service->HandleShareURLNavigationIntercepted(url);

    // Close the tab if the url interception ends with an empty page.
    const GURL& last_committed_url =
        navigation_handle()->GetWebContents()->GetLastCommittedURL();
    if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
        last_committed_url.is_empty()) {
      navigation_handle()->GetWebContents()->ClosePage();
    }
    return CANCEL;
  }
  return PROCEED;
}

}  // namespace data_sharing
