// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.util.RunnableTimer;

/** Helper class to take screenshots of tabs when navigation completes, for syncing purposes. */
@NullMarked
public class TabScreenshotSyncHelper extends TabModelSelectorTabObserver {
    private static final int DELAY_MS = 5000;

    private final TabContentManager mTabContentManager;
    private final RunnableTimer mTimer;

    public TabScreenshotSyncHelper(TabModelSelector selector, TabContentManager tabContentManager) {
        this(selector, tabContentManager, new RunnableTimer());
    }

    @VisibleForTesting
    TabScreenshotSyncHelper(
            TabModelSelector selector, TabContentManager tabContentManager, RunnableTimer timer) {
        super(selector);
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SYNC_TAB_SCREENSHOTS);
        mTabContentManager = tabContentManager;
        mTimer = timer;
    }

    @Override
    public void onDidStartNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        // Cancel any ongoing timer:
        // * If it was running for a previous navigation in the same tab, it's now obsolete.
        // * If it was for a different tab, it's also obsolete since screenshots can't be taken for
        //   background tabs. (Instead, a screenshot was taken just before the tab switch.)
        mTimer.cancelTimer();
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(Tab tab, NavigationHandle navigation) {
        if (!navigation.hasCommitted() || navigation.isErrorPage()) {
            return;
        }

        // Cancel any ongoing timer:
        // * If it was running for a previous navigation in the same tab, it's now obsolete.
        // * If it was for a different tab, it's also obsolete since screenshots can't be taken for
        //   background tabs. (Instead, a screenshot was taken just before the tab switch.)
        mTimer.cancelTimer();

        mTimer.startTimer(
                DELAY_MS,
                () -> {
                    if (tab.isInitialized() && !tab.isDestroyed() && !tab.isHidden()) {
                        mTabContentManager.cacheTabThumbnail(tab);
                    }
                });
    }

    @Override
    public void destroy() {
        mTimer.cancelTimer();
        super.destroy();
    }
}
