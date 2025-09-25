// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.NavigationHandle;

/** Manages the lifespan of the {@link ReaderModeBottomSheetCoordinator}. */
@NullMarked
public class ReaderModeBottomSheetManager extends EmptyTabObserver implements Destroyable {
    private final EmptyTabObserver mEmptyTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDidFinishNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigationHandle) {
                    if (navigationHandle.hasCommitted()
                            && navigationHandle.isInPrimaryMainFrame()) {
                        handleNewTabOrUrl();
                    }
                }

                @Override
                public void onClosingStateChanged(Tab tab, boolean closing) {
                    if (closing && tab == mActiveTab) {
                        hide();
                    }
                }
            };

    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver =
            new BrowserControlsStateProvider.Observer() {
                @Override
                public void onControlsOffsetChanged(
                        int topOffset,
                        int topControlsMinHeightOffset,
                        boolean topControlsMinHeightChanged,
                        int bottomOffset,
                        int bottomControlsMinHeightOffset,
                        boolean bottomControlsMinHeightChanged,
                        boolean requestNewFrame,
                        boolean isVisibilityForced) {
                    handleBrowserControlsOffsetChange(
                            mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio());
                }
            };

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ActivityTabProvider mTabProvider;
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final ThemeColorProvider mThemeColorProvider;
    private final Callback<@Nullable Tab> mActivityTabTabObserver = this::onActivityTabChanged;

    private @Nullable ReaderModeBottomSheetCoordinator mCoordinator;
    private @Nullable Tab mActiveTab;

    /**
     * @param context The {@link Context} for the manager.
     * @param bottomSheetController The {@link BottomSheetController} for the manager.
     * @param tabProvider The {@link ActivityTabProvider} for the manager.
     * @param browserControlsVisibilityManager The {@link BrowserControlsVisibilityManager} for the
     *     manager.
     * @param themeColorProvider The {@link ThemeColorProvider} for the manager.
     */
    public ReaderModeBottomSheetManager(
            Context context,
            BottomSheetController bottomSheetController,
            ActivityTabProvider tabProvider,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            ThemeColorProvider themeColorProvider) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabProvider = tabProvider;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);
        mThemeColorProvider = themeColorProvider;
        mTabProvider.addObserver(mActivityTabTabObserver);
        mActivityTabTabObserver.onResult(mTabProvider.get());
    }

    // Destroyable implementation.

    @Override
    public void destroy() {
        mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
        mTabProvider.removeObserver(mActivityTabTabObserver);
        removeTabObservers();
        mActiveTab = null;
        hide();
    }

    // ActivityTabProvider.ActivityTabTabObserver implementation.

    public void onActivityTabChanged(@Nullable Tab tab) {
        // Remove the tab observers for the previous tab before continuing.
        removeTabObservers();
        mActiveTab = tab;

        if (mActiveTab != null) {
            addTabObservers();
            handleNewTabOrUrl();
        } else {
            hide();
        }
    }

    private void addTabObservers() {
        if (mActiveTab != null) {
            mActiveTab.addObserver(mEmptyTabObserver);
        }
    }

    private void removeTabObservers() {
        if (mActiveTab != null) {
            mActiveTab.removeObserver(mEmptyTabObserver);
        }
    }

    private void handleBrowserControlsOffsetChange(float browserControlHiddenRatio) {
        if (mActiveTab == null
                || mActiveTab.getWebContents() == null
                || !DomDistillerUrlUtils.isDistilledPage(mActiveTab.getUrl())) {
            return;
        }

        // If the browser controls are fully shown, then show the bottom sheet.
        // Set a static threshold for the browser controls to be considered hidden enough to hide
        // the bottom sheet. This is to prevent jumpy behavior when the user scrolls up and down
        // slightly.
        if (browserControlHiddenRatio == 0) {
            show(mActiveTab);
        } else if (browserControlHiddenRatio >= 0.5f) {
            hide();
        }
    }

    private void handleNewTabOrUrl() {
        if (mActiveTab != null
                && mActiveTab.getWebContents() != null
                && DomDistillerUrlUtils.isDistilledPage(mActiveTab.getUrl())) {
            show(mActiveTab);
        } else {
            hide();
        }
    }

    // Creates and shows the reader mode bottom sheet.
    private void show(Tab tab) {
        if (mCoordinator == null) {
            mCoordinator =
                    new ReaderModeBottomSheetCoordinator(
                            mContext,
                            tab.getProfile(),
                            mBottomSheetController,
                            mThemeColorProvider);
        }
        mCoordinator.show(tab);
    }

    // Destroys the reader mode bottom sheet.
    private void hide() {
        if (mCoordinator != null) {
            mCoordinator.hide();
        }
    }
}
