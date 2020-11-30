// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.toolbar;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;

/** The business logic for controlling the top toolbar's cc texture. */
public class TopToolbarOverlayMediator {
    // Forced testing params.
    private static Boolean sIsTabletForTesting;
    private static Integer sToolbarBackgroundColorForTesting;
    private static Integer sUrlBarColorForTesting;

    /** An Android Context. */
    private final Context mContext;

    /** A handle to the layout manager for observing scene changes. */
    private final LayoutStateProvider mLayoutStateProvider;

    /** The observer of changes to the active layout. */
    private final LayoutStateObserver mSceneChangeObserver;

    /** A means of populating draw info for the progress bar. */
    private final Callback<ClipDrawableProgressBar.DrawingInfo> mProgressInfoCallback;

    /** Provides current tab. */
    private final ActivityTabProvider mTabSupplier;

    /** An observer that watches for changes in the active tab. */
    private final ActivityTabProvider.ActivityTabObserver mTabSupplierObserver;

    /** Access to the current state of the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** An observer of the browser controls offsets. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    /** The view state for this overlay. */
    private final PropertyModel mModel;

    /** The last non-null tab. */
    private Tab mLastActiveTab;

    /** Whether the active layout has its own toolbar to display instead of this one. */
    private boolean mLayoutHasOwnToolbar;

    /** Whether the android view for this overlay is visible. */
    private boolean mIsAndroidViewVisible;

    TopToolbarOverlayMediator(PropertyModel model, Context context,
            LayoutStateProvider layoutStateProvider,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            ActivityTabProvider tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mContext = context;
        mLayoutStateProvider = layoutStateProvider;
        mProgressInfoCallback = progressInfoCallback;
        mTabSupplier = tabSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModel = model;

        mSceneChangeObserver = new LayoutStateObserver() {
            @Override
            public void onStartedShowing(@LayoutType int layout, boolean showToolbar) {
                // TODO(1100332): Once ToolbarSwipeLayout uses a SceneLayer that does not include
                //                its own toolbar, only check for the vertical tab switcher.
                mLayoutHasOwnToolbar = (layout == LayoutType.TAB_SWITCHER
                                               && !TabUiFeatureUtilities.isGridTabSwitcherEnabled())
                        || layout == LayoutType.TOOLBAR_SWIPE;
                updateVisibility();
            }
        };
        mLayoutStateProvider.addObserver(mSceneChangeObserver);

        final TabObserver currentTabObserver = new EmptyTabObserver() {
            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updateThemeColor(tab);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                updateProgress();
            }

            @Override
            public void onContentChanged(Tab tab) {
                updateVisibility();
                updateThemeColor(tab);
            }
        };

        // Keep an observer attached to the visible tab (and only the visible tab) to update
        // properties including theme color.
        mTabSupplierObserver = (tab, hint) -> {
            if (mLastActiveTab != null) mLastActiveTab.removeObserver(currentTabObserver);
            if (tab == null) return;

            mLastActiveTab = tab;
            mLastActiveTab.addObserver(currentTabObserver);
            updateVisibility();
            updateThemeColor(mLastActiveTab);
            updateProgress();
        };
        mTabSupplier.addObserverAndTrigger(mTabSupplierObserver);

        mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                // The content offset is passed to the toolbar layer so that it can position itself
                // at the bottom of the space available for top controls. The main reason for using
                // content offset instead of top controls offset is that top controls can have a
                // greater height than that of the toolbar, e.g. when status indicator is visible,
                // and the toolbar needs to be positioned at the bottom of the top controls
                // regardless of the total height.
                mModel.set(TopToolbarOverlayProperties.CONTENT_OFFSET,
                        mBrowserControlsStateProvider.getContentOffset());

                updateVisibility();
                updateShadowState();
            }
        };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
     * @param isVisible Whether the android view is visible.
     */
    void setIsAndroidViewVisible(boolean isVisible) {
        mIsAndroidViewVisible = isVisible;
        updateShadowState();
    }

    /**
     * Compute whether the texture's shadow should be visible. The shadow is visible whenever the
     * android view is not shown.
     */
    private void updateShadowState() {
        boolean drawControlsAsTexture =
                BrowserControlsUtils.drawControlsAsTexture(mBrowserControlsStateProvider);
        boolean showShadow = drawControlsAsTexture || !mIsAndroidViewVisible;
        mModel.set(TopToolbarOverlayProperties.SHOW_SHADOW, showShadow);
    }

    /**
     * Update the colors of the layer based on the specified tab.
     * @param tab The tab to base the colors on.
     */
    private void updateThemeColor(Tab tab) {
        @ColorInt
        int color = getToolbarBackgroundColor(tab);
        mModel.set(TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR, color);
        mModel.set(TopToolbarOverlayProperties.URL_BAR_COLOR, getUrlBarBackgroundColor(tab, color));
    }

    /**
     * @param tab The tab to get the background color for.
     * @return The background color.
     */
    @ColorInt
    private int getToolbarBackgroundColor(Tab tab) {
        if (sToolbarBackgroundColorForTesting != null) return sToolbarBackgroundColorForTesting;
        return ToolbarColors.getToolbarSceneLayerBackground(tab);
    }

    /**
     * @param tab The tab to get the background color for.
     * @param backgroundColor The tab's background color.
     * @return The url bar color.
     */
    @ColorInt
    private int getUrlBarBackgroundColor(Tab tab, @ColorInt int backgroundColor) {
        if (sUrlBarColorForTesting != null) return sUrlBarColorForTesting;
        return ToolbarColors.getTextBoxColorForToolbarBackground(
                mContext.getResources(), tab, backgroundColor);
    }

    /** Update the state of the composited progress bar. */
    private void updateProgress() {
        // Tablets have their own version of a progress "spinner".
        if (isTablet()) return;

        if (mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO) == null) {
            mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                    new ClipDrawableProgressBar.DrawingInfo());
        }

        // Update and set the progress info to trigger an update; the PROGRESS_BAR_INFO
        // property skips the object equality check.
        mProgressInfoCallback.onResult(mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }

    /** @return Whether this component is in tablet mode. */
    private boolean isTablet() {
        if (sIsTabletForTesting != null) return sIsTabletForTesting;
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    /** Clean up any state and observers. */
    void destroy() {
        mTabSupplier.removeObserver(mTabSupplierObserver);
        mTabSupplierObserver.onActivityTabChanged(null, false);
        mLastActiveTab = null;

        mLayoutStateProvider.removeObserver(mSceneChangeObserver);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
    }

    /** Update the visibility of the overlay. */
    private void updateVisibility() {
        mModel.set(TopToolbarOverlayProperties.VISIBLE,
                !BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsStateProvider)
                        && !mLayoutHasOwnToolbar);
    }

    /** @return Whether this overlay should be attached to the tree. */
    boolean shouldBeAttachedToTree() {
        return true;
    }

    @VisibleForTesting
    static void setIsTabletForTesting(Boolean isTablet) {
        sIsTabletForTesting = isTablet;
    }

    @VisibleForTesting
    static void setToolbarBackgroundColorForTesting(@ColorInt int color) {
        sToolbarBackgroundColorForTesting = color;
    }

    @VisibleForTesting
    static void setUrlBarColorForTesting(@ColorInt int color) {
        sUrlBarColorForTesting = color;
    }
}
