// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;

/**
 * Observes navigations on every tab in the given tab model. Filters to navigations for tabs in tab
 * groups and notifies sync of them.
 */
public class NavigationObserver extends TabModelSelectorTabObserver {
    private final TabGroupSyncService mTabGroupSyncService;
    private final NavigationTracker mNavigationTracker;
    private boolean mEnableObservers;

    /**
     * Constructor.
     *
     * @param tabModelSelector The {@link TabModelSelector} whose tabs are to be observed.
     * @param tabGroupSyncService The sync backend to be notified of navigations.
     * @param navigationTracker Tracker for identifying sync initiated navigations.
     */
    public NavigationObserver(
            TabModelSelector tabModelSelector,
            TabGroupSyncService tabGroupSyncService,
            NavigationTracker navigationTracker) {
        super(tabModelSelector);
        mTabGroupSyncService = tabGroupSyncService;
        mNavigationTracker = navigationTracker;
    }

    /**
     * Called to enable or disable this observer. When disabled, the navigations will not be
     * propagated to sync. Typically invoked when chrome is in the middle of applying remote updates
     * to the local tab model.
     *
     * @param enableObservers Whether to enable the observer.
     */
    public void enableObservers(boolean enableObservers) {
        mEnableObservers = enableObservers;
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {
        if (!mEnableObservers) return;

        // Ignore redirects, incognito, and non-tabgroup tabs.
        boolean isRedirect =
                (navigationHandle.pageTransition() & PageTransition.IS_REDIRECT_MASK) != 0;

        if (tab.isIncognito() || isRedirect || tab.getTabGroupId() == null) {
            return;
        }

        // Avoid loops if the navigation was initiated from sync.
        if (mNavigationTracker.wasNavigationFromSync(navigationHandle.getUserDataHost())) {
            return;
        }

        // Propagate the update to sync. We set the position argument as -1 so that it can be
        // ignored in native.
        int rootId = tab.getRootId();
        mTabGroupSyncService.updateTab(
                rootId, tab.getId(), tab.getTitle(), tab.getUrl(), /* position= */ -1);
    }
}
