// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_NAVIGATION_ABLATION_THROTTLE_H_
#define CHROME_BROWSER_PRELOADING_NAVIGATION_ABLATION_THROTTLE_H_

#include <memory>

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

// Creates a throttle that will delay the start of the navigation by a fixed
// amount of time. This ablation occurs for most navigations, but does not occur
// for subframes, prerenders, fenced frames, bf/restore style navigations,
// client redirects, web bundles, or the default search engine based on
// configuration.
std::unique_ptr<content::NavigationThrottle>
MaybeCreateNavigationAblationThrottle(content::NavigationHandle* navigation);

#endif  // CHROME_BROWSER_PRELOADING_NAVIGATION_ABLATION_THROTTLE_H_
