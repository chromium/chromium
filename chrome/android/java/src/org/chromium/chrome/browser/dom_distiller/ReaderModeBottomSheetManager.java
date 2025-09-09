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
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
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
                        handleNewTabOrUrl(tab);
                    }
                }

                @Override
                public void onClosingStateChanged(Tab tab, boolean closing) {
                    if (closing && tab == mActiveTab) {
                        hide();
                    }
                }
            };

    private final GestureStateListener mGestureStateListener =
            new GestureStateListener() {
                @Override
                public void onScrollStarted(
                        int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                    if (isDirectionUp && mActiveTab != null) {
                        handleScroll(mActiveTab);
                    }
                }
            };

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ActivityTabProvider mTabProvider;
    private final ThemeColorProvider mThemeColorProvider;
    private final Callback<@Nullable Tab> mActivityTabTabObserver = this::onActivityTabChanged;

    private @Nullable ReaderModeBottomSheetCoordinator mCoordinator;
    private @Nullable Tab mActiveTab;
    private @Nullable GestureListenerManager mGestureListenerManager;

    /**
     * @param context The {@link Context} for the manager.
     * @param bottomSheetController The {@link BottomSheetController} for the manager.
     * @param tabProvider The {@link ActivityTabProvider} for the manager.
     * @param themeColorProvider The {@link ThemeColorProvider} for the manager.
     */
    public ReaderModeBottomSheetManager(
            Context context,
            BottomSheetController bottomSheetController,
            ActivityTabProvider tabProvider,
            ThemeColorProvider themeColorProvider) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabProvider = tabProvider;
        mThemeColorProvider = themeColorProvider;
        mTabProvider.addObserver(mActivityTabTabObserver);
        mActivityTabTabObserver.onResult(mTabProvider.get());
    }

    // Destroyable implementation.

    @Override
    public void destroy() {
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
            handleNewTabOrUrl(mActiveTab);
        } else {
            hide();
        }
    }

    private void addTabObservers() {
        if (mActiveTab != null) {
            mActiveTab.addObserver(mEmptyTabObserver);
            if (mActiveTab.getWebContents() != null) {
                mGestureListenerManager =
                        GestureListenerManager.fromWebContents(mActiveTab.getWebContents());
                if (mGestureListenerManager != null) {
                    mGestureListenerManager.addListener(mGestureStateListener);
                }
            }
        }
    }

    private void removeTabObservers() {
        if (mActiveTab != null) {
            mActiveTab.removeObserver(mEmptyTabObserver);
            if (mGestureListenerManager != null) {
                mGestureListenerManager.removeListener(mGestureStateListener);
                mGestureListenerManager = null;
            }
        }
    }

    private void handleScroll(Tab tab) {
        if (tab.getWebContents() != null && DomDistillerUrlUtils.isDistilledPage(tab.getWebContents().getVisibleUrl())) {
            show(tab);
        }
    }

    private void handleNewTabOrUrl(Tab tab) {
        if (tab.getWebContents() != null
                && DomDistillerUrlUtils.isDistilledPage(tab.getWebContents().getVisibleUrl())) {
            show(tab);
        } else {
            hide();
        }
    }

    // Creates and shows the reader mode bottom sheet.
    private void show(Tab tab) {
        if (mCoordinator == null) {
            mCoordinator =
                    new ReaderModeBottomSheetCoordinator(
                            tab,
                            mContext,
                            tab.getProfile(),
                            mBottomSheetController,
                            mThemeColorProvider);
        }
        mCoordinator.show(/* showFullSheet= */ false);
    }

    // Destroys the reader mode bottom sheet.
    private void hide() {
        if (mCoordinator != null) {
            mCoordinator.hide();
        }
    }
}
