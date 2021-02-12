// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import org.chromium.url.GURL;

/**
 * Collection of methods used across Web Feed follow entry points.
 */
public class WebFeedBridge {
    /**
     * Returns whether the given {@link GURL} is followed.
     */
    public boolean isFollowed(GURL url) {
        return false;
    }
}