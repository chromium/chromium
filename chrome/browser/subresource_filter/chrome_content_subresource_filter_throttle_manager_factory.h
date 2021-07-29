// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_FACTORY_H_

namespace content {
class WebContents;
}  // namespace content

// Creates a ContentSubresourceFilterThrottleManager and attaches it to
// |web_contents|, passing it embedder-level state.
void CreateSubresourceFilterThrottleManagerForWebContents(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_FACTORY_H_
