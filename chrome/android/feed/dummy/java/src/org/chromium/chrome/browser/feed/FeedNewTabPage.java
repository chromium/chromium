// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Provides a new tab page that displays an interest feed rendered list of content suggestions.
 */
public class FeedNewTabPage extends NewTabPage {
    /**
     * Constructs a new FeedNewTabPage.
     * @param activity The containing {@link ChromeActivity}.
     * @param nativePageHost The host for this native page.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param activityTabProvider Allows us to check if we are the current tab.
     * @param activityLifecycleDispatcher Allows us to subscribe to backgrounding events.
     */
    public FeedNewTabPage(ChromeActivity activity, NativePageHost nativePageHost,
            TabModelSelector tabModelSelector, ActivityTabProvider activityTabProvider,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        super(activity, nativePageHost, tabModelSelector, activityTabProvider,
                activityLifecycleDispatcher);
    }

    @VisibleForTesting
    public static boolean isDummy() {
        return true;
    }
}
