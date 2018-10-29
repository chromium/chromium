// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * Vends dummy Feed host implementations.
 */
public class FeedProcessScopeFactory {
    private static FeedAppLifecycle sFeedAppLifecycle;

    /**
     * @return A dummy {@link FeedAppLifecycle} instance.
     */
    public static FeedAppLifecycle getFeedAppLifecycle() {
        return new FeedAppLifecycle();
    }

    static public void setTestNetworkClient(TestNetworkClient client) {}
}
