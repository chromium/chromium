// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;
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

    // Temporary view to act as a placeholder for the bottom bar.
    private final FrameLayout mTemporaryView;
    private @Nullable Tab mCurrentTab;

    // Temporary bottom bar implementation to be replaced with the real bottom bar (likely
    // constructed externally).
    private final BottomBar mTemporaryBottomBar =
            new BottomBar() {
                @Override
                public View getView() {
                    return mTemporaryView;
                }

                @Override
                public void setParent(@Host int host) {
                    // Do nothing for now.
                }
            };

    private @Nullable BottomControlsVisibilityController mVisibilityController;
    private @Nullable Callback<Object> mOnModelTokenChange;

    /**
     * @param bottomBarContainer The {@link FrameLayout} for the bottom bar.
     * @param requestLayerUpdateCallback A callback to request layer updates.
     * @param tabSupplier Supplier for the current tab.
     */
    public BottomBarContainerCoordinator(
            FrameLayout bottomBarContainer,
            Callback<Boolean> requestLayerUpdateCallback,
            NullableObservableSupplier<Tab> tabSupplier) {
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

        // Create a temporary view to act as a placeholder for the bottom bar.
        Context context = bottomBarContainer.getContext();
        mTemporaryView = new FrameLayout(context);
        int bottomBarHeight =
                context.getResources().getDimensionPixelOffset(R.dimen.bottom_controls_height);
        mTemporaryView.setLayoutParams(
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, bottomBarHeight));
        mTemporaryView.setBackgroundColor(0xFF00FF00);

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
    }

    private void updateBottomBarVisibility() {
        if (mVisibilityController == null) return;
        boolean currentTabIsRegularNtp =
                mCurrentTab != null
                        && UrlUtilities.isNtpUrl(mCurrentTab.getUrl())
                        && !mCurrentTab.isIncognito();
        boolean visible = !(BottomBarConfigUtils.shouldDisableOnNtp() && currentTabIsRegularNtp);
        mVisibilityController.setBottomControlsVisible(visible);
        mTemporaryView.setVisibility(visible ? View.VISIBLE : View.GONE);
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
        return mTemporaryBottomBar;
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
