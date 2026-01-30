// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_navigation_observer.h"

#include <string>

#include "chrome/browser/enterprise/reporting/saas_usage/navigation_handle_data_delegate.h"
#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_reporting_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_reporting {

namespace {

bool ShouldRecordNavigation(content::NavigationHandle* navigation_handle) {
  // Only report navigations that have committed, are not error pages, and are
  // not same document navigations.
  // Downloads don't result in a committed navigation in the main frame, so they
  // will not be reported.
  // Don't report subframe navigations.
  return navigation_handle->HasCommitted() &&
         !navigation_handle->IsErrorPage() &&
         !navigation_handle->IsSameDocument() &&
         navigation_handle->IsInMainFrame();
}

}  // namespace

SaasUsageNavigationObserver::SaasUsageNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  controller_ = SaasUsageReportingControllerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

SaasUsageNavigationObserver::~SaasUsageNavigationObserver() = default;

void SaasUsageNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ShouldRecordNavigation(navigation_handle)) {
    return;
  }

  // Controller can be null for non-regular profiles.
  if (controller_) {
    controller_->RecordNavigation(
        NavigationHandleDataDelegate(*navigation_handle));
  }
}

}  // namespace enterprise_reporting
