// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.content.Context;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/**
 * This is the controller that prevents incognito tabs from being visible in Android Recents.
 */
public class IncognitoTabSnapshotController implements TabModelSelectorObserver {
    private final Window mWindow;
    private final TabModelSelector mTabModelSelector;
    private boolean mInOverviewMode;
    private Context mContext;

    /**
     * Creates and registers a new {@link IncognitoTabSnapshotController}.
     * @param context The activity context.
     * @param window The {@link Window} containing the flags to which the secure flag will be added
     *               and cleared.
     * @param layoutManager The {@link LayoutManagerChrome} where this controller will be added.
     * @param tabModelSelector The {@link TabModelSelector} from where tab information will be
     */
    public static void createIncognitoTabSnapshotController(Context context, Window window,
            LayoutManagerChrome layoutManager, TabModelSelector tabModelSelector) {
        new IncognitoTabSnapshotController(context, window, layoutManager, tabModelSelector);
    }

    @VisibleForTesting
    IncognitoTabSnapshotController(Context context, Window window,
            LayoutManagerChrome layoutManager, TabModelSelector tabModelSelector) {
        mWindow = window;
        mTabModelSelector = tabModelSelector;
        mContext = context;

        LayoutStateObserver mLayoutStateObserver =
                new FilterLayoutStateObserver(LayoutType.TAB_SWITCHER, new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType, boolean showToolbar) {
                        assert layoutType == LayoutType.TAB_SWITCHER;

                        mInOverviewMode = true;
                        updateIncognitoState();
                    }

                    @Override
                    public void onStartedHiding(
                            int layoutType, boolean showToolbar, boolean delayAnimation) {
                        assert layoutType == LayoutType.TAB_SWITCHER;

                        mInOverviewMode = false;
                    }
                });

        layoutManager.addObserver(mLayoutStateObserver);
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
        if (FeatureList.isInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_SCREENSHOT)) {
            expectedSecureState = false;
        }
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
        // even if the tab switcher is showing the normal stack. But if the grid tab switcher
        // is enabled, incognito tabs are not visible while we're showing the normal tabs.
        return isInIncognitoModel
                || (!isGridTabSwitcherEnabled() && mInOverviewMode && getIncognitoTabCount() > 0);
    }

    // Set in overview mode for testing.
    @VisibleForTesting
    void setInOverViewMode(boolean overviewMode) {
        mInOverviewMode = overviewMode;
    }

    @VisibleForTesting
    public boolean isGridTabSwitcherEnabled() {
        return TabUiFeatureUtilities.isGridTabSwitcherEnabled(mContext);
    }

    /**
     * @return The number of incognito tabs.
     */
    private int getIncognitoTabCount() {
        return mTabModelSelector.getModel(true).getCount();
    }
}
