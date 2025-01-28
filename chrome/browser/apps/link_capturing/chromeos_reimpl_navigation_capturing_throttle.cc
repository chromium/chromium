// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "content/public/browser/browser_thread.h"

namespace apps {

namespace {

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
ChromeOsReimplNavigationCapturingThrottle::MaybeCreate(
    content::NavigationHandle* handle) {
  if (!features::IsNavigationCapturingReimplEnabled()) {
    return nullptr;
  }
  return base::WrapUnique(
      new ChromeOsReimplNavigationCapturingThrottle(handle));
}

ChromeOsReimplNavigationCapturingThrottle::
    ~ChromeOsReimplNavigationCapturingThrottle() = default;

const char* ChromeOsReimplNavigationCapturingThrottle::GetNameForLogging() {
  return "ChromeOsReimplNavigationCapturingThrottle";
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::NavigationThrottle::PROCEED;
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillRedirectRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::NavigationThrottle::PROCEED;
}

ThrottleCheckResult
ChromeOsReimplNavigationCapturingThrottle::WillProcessResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::NavigationThrottle::PROCEED;
}

ChromeOsReimplNavigationCapturingThrottle::
    ChromeOsReimplNavigationCapturingThrottle(
        content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

}  // namespace apps
