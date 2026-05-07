// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsContentDelegate;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator.BottomControlsVisibilityController;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.bottombar.BottomBar;
import org.chromium.chrome.browser.ui.bottombar.BottomBarCoordinator;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMediator;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Container for the bottom bar.
 *
 * <p>Note that the {@link BackPressHandler} implementation is left as default on purpose.
 */
@NullMarked
public class BottomBarContainerCoordinator
        implements BottomControlsContentDelegate, BottomBarMediator.VisibilityDelegate {
    private final FrameLayout mBottomBarContainer;
    private final Callback<Boolean> mRequestLayerUpdateCallback;
    private final BottomBarCoordinator mBottomBarCoordinator;

    private @Nullable BottomControlsVisibilityController mVisibilityController;
    private @Nullable Callback<Object> mOnModelTokenChange;
    private boolean mIsVisible = true;

    /**
     * @param bottomBarContainer The {@link FrameLayout} for the bottom bar.
     * @param requestLayerUpdateCallback A callback to request layer updates.
     * @param tabSupplier Supplier for the current tab.
     * @param themeColorProvider Theme color provider for the bottom bar.
     */
    public BottomBarContainerCoordinator(
            FrameLayout bottomBarContainer,
            Callback<Boolean> requestLayerUpdateCallback,
            ActionRegistry actionRegistry,
            NullableObservableSupplier<Tab> tabSupplier,
            ThemeColorProvider themeColorProvider,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mBottomBarContainer = bottomBarContainer;
        mRequestLayerUpdateCallback = requestLayerUpdateCallback;

        mBottomBarCoordinator =
                new BottomBarCoordinator(
                        bottomBarContainer,
                        actionRegistry,
                        themeColorProvider,
                        tabSupplier,
                        homepageEnabledSupplier,
                        this,
                        profileSupplier,
                        omniboxFocusStateSupplier);
    }

    @Override
    public void initializeWithNative(
            BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        mVisibilityController = visibilityController;
        mOnModelTokenChange = onModelTokenChange;
        updateVisibility();
        // TODO(crbug.com/493594829): The token change should be based on the property model of the
        // bottom bar.
        mOnModelTokenChange.onResult(new Object());
    }

    @Override
    public void destroy() {
        mBottomBarCoordinator.destroy();
    }

    @Override
    public void onVisibilityChanged(boolean isVisible) {
        mIsVisible = isVisible;
        updateVisibility();
    }

    @Override
    public @LayerScrollBehavior int getScrollBehavior() {
        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
    }

    @Override
    public @Nullable @ColorInt Integer getBackgroundColor() {
        return mBottomBarCoordinator.getBackgroundColor();
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

    public void updateVisibility() {
        mBottomBarContainer.setVisibility(mIsVisible ? View.VISIBLE : View.GONE);
        if (mVisibilityController != null) {
            mVisibilityController.setBottomControlsVisible(mIsVisible);
        }
    }
}
