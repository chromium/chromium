// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.components.background_task_scheduler.BackgroundTask;

/**
 * A stub of FeedV1.
 */
public class FeedV1 {
    // Whether FeedV1 is compiled in.
    public static final boolean IS_AVAILABLE = false;

    public static void startup() {}

    public static FeedSurfaceCoordinator.StreamWrapper createStreamWrapper() {
        return null;
    }

    public static BackgroundTask createRefreshTask() {
        return null;
    }
    public static void destroy() {}
}
