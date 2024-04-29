// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/**
 * Utility class for creating and navigation a tab. Designed to handle creating tabs in background
 * and deferring navigation until the user switches back to the tab.
 */
public class TabCreationDelegateImpl implements TabCreationDelegate {
    private static final String TAG = "TG.TabCreationDelegate";

    private final TabCreator mTabCreator;
    private final NavigationTracker mNavigationTracker;

    /**
     * Constructor.
     *
     * @param tabCreator The tab creator to help with creating a tab.
     * @param navigationTracker Tracks navigations in order to prevent back propagation to sync.
     */
    public TabCreationDelegateImpl(TabCreator tabCreator, NavigationTracker navigationTracker) {
        mTabCreator = tabCreator;
        mNavigationTracker = navigationTracker;
    }

    @Override
    public Tab createBackgroundTab(GURL url, String title, Tab parent, int position) {
        LogUtils.log(TAG, "createBackgroundTab " + url);
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        mNavigationTracker.setNavigationWasFromSync(loadUrlParams.getNavigationHandleUserData());
        // TODO(shaktisahu): TabLaunchType will be a different type for revisit surface?
        return mTabCreator.createNewTab(
                loadUrlParams, title, TabLaunchType.FROM_SYNC_BACKGROUND, parent, position);
    }

    @Override
    public void navigateToUrl(Tab tab, GURL url, String title, boolean isForegroundTab) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        // TODO(shaktisahu): Since we are doing freezing tab now, the navigation handle will be
        // lost from the LoadUrlParams. However, tracking this navigation isn't as important
        // any more, since there will be no navigation and the only navigation will happen when
        // user switches to the tab.
        mNavigationTracker.setNavigationWasFromSync(loadUrlParams.getNavigationHandleUserData());
        if (!isForegroundTab) {
            // Set the URL and title on the tab. But defer the navigation until the tab becomes
            // active.
            LogUtils.log(TAG, "freezeAndAppendPendingNavigation, url = " + url);
            tab.freezeAndAppendPendingNavigation(loadUrlParams, title);
        } else {
            LogUtils.log(TAG, "tab.loadUrl, url = " + url);
            tab.loadUrl(loadUrlParams);
        }
    }
}
