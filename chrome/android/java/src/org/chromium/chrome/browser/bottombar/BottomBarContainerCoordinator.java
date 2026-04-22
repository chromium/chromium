// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarCoordinator;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * Container for the bottom bar.
 *
 * <p>Note that the {@link BackPressHandler} implementation is left as default on purpose.
 */
@NullMarked
public class BottomBarContainerCoordinator implements BottomControlsContentDelegate {
    private final FrameLayout mBottomBarContainer;
    private final Callback<Boolean> mRequestLayerUpdateCallback;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final Callback<@Nullable Tab> mTabSupplierObserver;
    private final TabObserver mTabObserver;
    private final BottomBarCoordinator mBottomBarCoordinator;

    private @Nullable Tab mCurrentTab;
    private @Nullable BottomControlsVisibilityController mVisibilityController;
    private @Nullable Callback<Object> mOnModelTokenChange;

    /**
     * @param bottomBarContainer The {@link FrameLayout} for the bottom bar.
     * @param requestLayerUpdateCallback A callback to request layer updates.
     * @param tabSupplier Supplier for the current tab.
     * @param themeColorProvider Theme color provider for the bottom bar.
     */
    public BottomBarContainerCoordinator(
            FrameLayout bottomBarContainer,
            Callback<Boolean> requestLayerUpdateCallback,
            NullableObservableSupplier<Tab> tabSupplier,
            ThemeColorProvider themeColorProvider) {
        mBottomBarContainer = bottomBarContainer;
        mRequestLayerUpdateCallback = requestLayerUpdateCallback;
        mTabSupplier = tabSupplier;

        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateBottomBarVisibility();
                    }
                };

        mTabSupplierObserver =
                (tab) -> {
                    if (mCurrentTab != null) {
                        mCurrentTab.removeObserver(mTabObserver);
                    }
                    mCurrentTab = tab;
                    if (mCurrentTab != null) {
                        mCurrentTab.addObserver(mTabObserver);
                    }
                    updateBottomBarVisibility();
                };
        mTabSupplier.addSyncObserverAndCallIfNonNull(mTabSupplierObserver);

        mBottomBarCoordinator = new BottomBarCoordinator(bottomBarContainer, themeColorProvider);

        updateBottomBarVisibility();
    }

    @Override
    public void initializeWithNative(
            BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        mVisibilityController = visibilityController;
        mOnModelTokenChange = onModelTokenChange;

        mVisibilityController.setBottomControlsVisible(true);
        // TODO(crbug.com/493594829): The token change should be based on the property model of the
        // bottom bar.
        mOnModelTokenChange.onResult(new Object());
    }

    @Override
    public void destroy() {
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
            mCurrentTab = null;
        }
        if (mTabSupplier != null) {
            mTabSupplier.removeObserver(mTabSupplierObserver);
        }
        mBottomBarCoordinator.destroy();
    }

    private void updateBottomBarVisibility() {
        if (mVisibilityController == null) return;
        boolean currentTabIsRegularNtp =
                mCurrentTab != null
                        && UrlUtilities.isNtpUrl(mCurrentTab.getUrl())
                        && !mCurrentTab.isIncognito();
        boolean visible = !(BottomBarConfigUtils.shouldDisableOnNtp() && currentTabIsRegularNtp);
        mVisibilityController.setBottomControlsVisible(visible);
        mBottomBarContainer.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    @Override
    public @LayerScrollBehavior int getScrollBehavior() {
        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
    }

    @Override
    public @Nullable @ColorInt Integer getBackgroundColor() {
        return null;
    }

    /** Returns the bottom bar. */
    public BottomBar getBottomBar() {
        return mBottomBarCoordinator;
    }

    /** Attaches the provided bottom bar view to the container. */
    public void attachBottomBarView(View view) {
        mBottomBarContainer.addView(view);

        if (mOnModelTokenChange != null) {
            // TODO(crbug.com/493594829): The token change should be based on the property model of
            // the bottom bar.
            mOnModelTokenChange.onResult(new Object());
        }

        mRequestLayerUpdateCallback.onResult(true);
    }
}
