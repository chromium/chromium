// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents.
 */
public class IncognitoTabSnapshotController extends EmptyTabModelSelectorObserver {
    private final Window mWindow;
    private final TabModelSelector mTabModelSelector;
    private final OverviewModeObserver mOverviewModeObserver;
    private boolean mInOverviewMode;

    /**
     * Creates and registers a new {@link IncognitoTabSnapshotController}.
     * @param window The {@link Window} containing the flags to which the secure flag will be added
     *               and cleared.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     *                         extracted.
     */
    public static void createIncognitoTabSnapshotController(
            Window window, LayoutManagerChrome layoutManager, TabModelSelector tabModelSelector) {
        new IncognitoTabSnapshotController(window, layoutManager, tabModelSelector);
    }

    @VisibleForTesting
    IncognitoTabSnapshotController(
            Window window, LayoutManagerChrome layoutManager, TabModelSelector tabModelSelector) {
        mWindow = window;
        mTabModelSelector = tabModelSelector;

        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                mInOverviewMode = true;
                updateIncognitoState();
            }

            @Override
            public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
                mInOverviewMode = false;
            }
        };

        layoutManager.addOverviewModeObserver(mOverviewModeObserver);
        tabModelSelector.addObserver(this);
    }

    @Override
    public void onChange() {
        updateIncognitoState();
    }

    /**
     * Sets the attributes flags to secure if there is an incognito tab visible.
     */
    @VisibleForTesting
    void updateIncognitoState() {
        WindowManager.LayoutParams attributes = mWindow.getAttributes();
        boolean currentSecureState = (attributes.flags & WindowManager.LayoutParams.FLAG_SECURE)
                == WindowManager.LayoutParams.FLAG_SECURE;
        boolean expectedSecureState = isShowingIncognito();
        if (currentSecureState == expectedSecureState) return;

        if (expectedSecureState) {
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        } else {
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    /**
     * @return Whether an incognito tab is visible.
     */
    @VisibleForTesting
    boolean isShowingIncognito() {
        boolean isInIncognitoModel = mTabModelSelector.getCurrentModel().isIncognito();

        // If we're using the overlapping tab switcher, we show the edge of the open incognito tabs
        // even if the tab switcher is showing the normal stack. But if the horizontal tab switcher
        // is enabled, incognito tabs are not visible while we're showing the normal tabs.
        return isInIncognitoModel
                || (!ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                           && mInOverviewMode && getIncognitoTabCount() > 0);
    }

    // Set in overview mode for testing.
    @VisibleForTesting
    public void setInOverViewMode(boolean overviewMode) {
        mInOverviewMode = overviewMode;
    }

    /**
     * @return The number of incognito tabs.
     */
    private int getIncognitoTabCount() {
        return mTabModelSelector.getModel(true).getCount();
    }
}
