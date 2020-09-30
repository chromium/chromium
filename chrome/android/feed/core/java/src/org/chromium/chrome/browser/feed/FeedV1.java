// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.chrome.browser.feed.v1.FeedProcessScopeFactory;
import org.chromium.chrome.browser.feed.v1.FeedRefreshTask;
import org.chromium.chrome.browser.feed.v1.FeedStreamWrapper;
import org.chromium.components.background_task_scheduler.BackgroundTask;

/**
 * Provides access to FeedV1. Stubbed out when V1 is removed from the build.
 */
public class FeedV1 {
    // Whether FeedV1 is compiled in.
    public static final boolean IS_AVAILABLE = true;

    public static void startup() {
        // We call getFeedAppLifecycle() here to ensure the app lifecycle is created so
        // that it can start listening for state changes.
        FeedProcessScopeFactory.getFeedAppLifecycle();
    }

    public static FeedSurfaceCoordinator.StreamWrapper createStreamWrapper() {
        return new FeedStreamWrapper();
    }

    public static BackgroundTask createRefreshTask() {
        return new FeedRefreshTask();
    }

    public static void destroy() {
        FeedProcessScopeFactory.destroy();
    }
}
