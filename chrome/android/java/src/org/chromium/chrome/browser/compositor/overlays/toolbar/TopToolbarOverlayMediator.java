// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.toolbar;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
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
    private final LayoutManager mLayoutManager;

    /** The observer of changes to the active layout. */
    private final SceneChangeObserver mSceneChangeObserver;

    /** A Layout for browser controls. */
    private final @Nullable ControlContainer mToolbarContainer;

    /** Provides current tab. */
    private final ActivityTabProvider mTabSupplier;

    /** An observer that watches for changes in the active tab. */
    private final ActivityTabProvider.ActivityTabObserver mTabSupplierObserver;

    /** Access to the current state of the browser controls. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** An observer of the browser controls offsets. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    /** A means of checking whether the toolbar android view is being force-hidden or shown. */
    private final ObservableSupplier<Boolean> mAndroidViewShownSupplier;

    /** An observer of the android view's hidden state. */
    private final Callback<Boolean> mAndroidViewShownObserver;

    /** The view state for this overlay. */
    private final PropertyModel mModel;

    /** The last non-null tab. */
    private Tab mLastActiveTab;

    /** Whether the active layout has its own toolbar to display instead of this one. */
    private boolean mLayoutHasOwnToolbar;

    TopToolbarOverlayMediator(PropertyModel model, Context context, LayoutManager layoutManager,
            @Nullable ControlContainer controlContainer, ActivityTabProvider tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<Boolean> androidViewShownSupplier) {
        mContext = context;
        mLayoutManager = layoutManager;
        mToolbarContainer = controlContainer;
        mTabSupplier = tabSupplier;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mAndroidViewShownSupplier = androidViewShownSupplier;
        mModel = model;

        mSceneChangeObserver = new SceneChangeObserver() {
            @Override
            public void onTabSelectionHinted(int tabId) {}

            @Override
            public void onSceneChange(Layout layout) {
                // TODO(1100332): Use layout IDs instead of type checking when they are available.
                // TODO(1100332): Once ToolbarSwipeLayout uses a SceneLayer that does not include
                //                its own toolbar, only check for the vertical tab switcher.
                mLayoutHasOwnToolbar =
                        layout instanceof StackLayout || layout instanceof ToolbarSwipeLayout;
                updateVisibility();
            }
        };
        mLayoutManager.addSceneChangeObserver(mSceneChangeObserver);

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

        mAndroidViewShownObserver = (shown) -> updateShadowState();
        mAndroidViewShownSupplier.addObserver(mAndroidViewShownObserver);

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
     * Compute whether the texture's shadow should be visible. The shadow is visible whenever the
     * android view is not shown.
     */
    private void updateShadowState() {
        boolean drawControlsAsTexture =
                BrowserControlsUtils.drawControlsAsTexture(mBrowserControlsStateProvider);
        boolean showShadow = drawControlsAsTexture || !mAndroidViewShownSupplier.get();
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
        if (isTablet() || mToolbarContainer == null) {
            return;
        }

        if (mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO) == null) {
            mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO,
                    new ClipDrawableProgressBar.DrawingInfo());
        }

        // Update and set the progress info to trigger an update; the PROGRESS_BAR_INFO
        // property skips the object equality check.
        mToolbarContainer.getProgressBarDrawingInfo(
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
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

        mLayoutManager.removeSceneChangeObserver(mSceneChangeObserver);
        mAndroidViewShownSupplier.removeObserver(mAndroidViewShownObserver);
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
