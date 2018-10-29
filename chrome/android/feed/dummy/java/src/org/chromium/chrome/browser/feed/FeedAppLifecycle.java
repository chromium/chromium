// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * Aggregation point for application lifecycle events that the Feed cares about.
 */
public class FeedAppLifecycle {
    /**
     * Create a FeedAppLifecycle instance. In normal use, this should only be called by {@link
     * FeedAppLifecycleFactory}.
     */
    public FeedAppLifecycle() {}

    /**
     * This is called when a tabbed activity is launched.
     */
    public void onActivityLaunched() {}
}
