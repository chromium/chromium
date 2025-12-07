// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_navigation_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/data_sharing_utils.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace data_sharing {

namespace {
bool ShouldHandleShareURLNavigation(
    content::NavigationHandle* navigation_handle) {
  // Make sure to keep it in sync between platforms.
  // LINT.IfChange(ShouldHandleShareURLNavigation)
  if (!navigation_handle->IsInMainFrame()) {
    return false;
  }

  // If this is a session or tab restore, don't intercept the
  // navigation to avoid showing the dialog on each browser
  // start.
  if (navigation_handle->GetRestoreType() == content::RestoreType::kRestored) {
    return false;
  }

  if (navigation_handle->IsRendererInitiated()) {
    if (navigation_handle->HasUserGesture()) {
      return true;
    }

    if (DataSharingNavigationUtils::GetInstance()->IsLastUserInteractionExpired(
            navigation_handle->GetWebContents())) {
      return false;
    }

    // Only allow redirect if the user interaction has not expired.
    if (navigation_handle->GetRedirectChain().size() <= 1) {
      return false;
    }
  }

  return true;
  // LINT.ThenChange(/ios/chrome/browser/collaboration/model/data_sharing_tab_helper.mm:ShouldHandleShareURLNavigation)
}
}  // namespace

// static
void DataSharingNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (features::IsDataSharingFunctionalityEnabled() &&
      features::ShouldInterceptUrlForVersioning()) {
    registry.AddThrottle(
        std::make_unique<DataSharingNavigationThrottle>(registry));
  }
}

DataSharingNavigationThrottle::DataSharingNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

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
    collaboration::CollaborationService* test_service) {
  test_service_ = test_service;
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::CheckIfShouldIntercept() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return PROCEED;
  }

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              navigation_handle()->GetWebContents()->GetBrowserContext()));

  if (test_service_) {
    collaboration_service = test_service_;
  }

  const GURL& url = navigation_handle()->GetURL();
  if (collaboration_service &&
      DataSharingUtils::ShouldInterceptNavigationForShareURL(url)) {
    if (ShouldHandleShareURLNavigation(navigation_handle())) {
      collaboration_service->HandleShareURLNavigationIntercepted(
          url, /* context = */ nullptr,
          collaboration::GetEntryPointFromPageTransition(
              navigation_handle()->GetPageTransition()));
    }

    // crbug.com/411646000: Only enable this for Android because on Desktop if
    // user clicks an invite link to launch the browser, the browser will quit
    // when the current tab is closed due to no tab remains.
#if BUILDFLAG(IS_ANDROID)
    // Close the tab if the url interception ends with an empty page.
    const GURL& last_committed_url =
        navigation_handle()->GetWebContents()->GetLastCommittedURL();
    if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
        last_committed_url.is_empty()) {
      navigation_handle()->GetWebContents()->ClosePage();
    }
#endif  // BUILDFLAG(IS_ANDROID)

    return CANCEL;
  }

  // Update interaction time to handle the case of client redirect.
  if (navigation_handle()->IsInMainFrame() &&
      (!navigation_handle()->IsRendererInitiated() ||
       navigation_handle()->HasUserGesture())) {
    DataSharingNavigationUtils::GetInstance()->UpdateLastUserInteractionTime(
        web_contents);
  }
  return PROCEED;
}

}  // namespace data_sharing
