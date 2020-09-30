// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.feed.v2.FeedStreamSurface;
import org.chromium.chrome.browser.feed.v2.FeedStreamWrapper;

/**
 * Provides access to FeedV2 with a similar interface as FeedV1.
 */
public class FeedV2 {
    // Whether FeedV2 is compiled in.
    public static final boolean IS_AVAILABLE = FeedV2BuildFlag.IS_AVAILABLE;

    public static void startup() {
        FeedStreamSurface.startup();
    }

    public static FeedSurfaceCoordinator.StreamWrapper createStreamWrapper() {
        return new FeedStreamWrapper();
    }
}
