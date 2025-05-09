// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_navigation_throttles.h"

#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"

void CreateAndAddChromeThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (handle.IsInMainFrame()) {
    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    // TODO(https://crbug.com/412524375): This assumption is fragile. This
    // should be cared by adding an attribute flag to
    // NavigationThrottleRegistry::AddThrottle().
    page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);
  }

  // Add new throttles here.
}