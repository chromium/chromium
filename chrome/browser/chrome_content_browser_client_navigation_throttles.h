// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_NAVIGATION_THROTTLE_H_

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

void CreateAndAddChromeThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry);

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_NAVIGATION_THROTTLE_H_