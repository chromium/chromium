// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
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
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Container for the bottom bar.
 *
 * <p>Note that the {@link BackPressHandler} implementation is left as default on purpose.
 */
@NullMarked
public class BottomBarContainerCoordinator
        implements BottomControlsContentDelegate, BottomBarMediator.VisibilityDelegate {
    private final FrameLayout mBottomBarContainer;
    private final Context mContext;
    private final Callback<Boolean> mRequestLayerUpdateCallback;
    private final BottomBarCoordinator mBottomBarCoordinator;
    private final Handler mHandler;
    private final Runnable mModelTokenChangeRunnable = this::onModelTokenChange;
    private final ComponentCallbacks mComponentCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration newConfig) {
                    if (newConfig.orientation == mCurrentOrientation) {
                        return;
                    }

                    if (mPendingVisibilityUpdate) {
                        mHandler.removeCallbacks(mModelTokenChangeRunnable);
                    }
                    mCurrentOrientation = newConfig.orientation;
                    mPendingVisibilityUpdate = true;
                    mHandler.post(mModelTokenChangeRunnable);
                }

                @Override
                public void onLowMemory() {}
            };

    private @Nullable BottomControlsVisibilityController mVisibilityController;
    private @Nullable Callback<Object> mOnModelTokenChange;
    private boolean mIsVisible = true;
    // Tracking if there is a pending visibility update to avoid scanning the whole queue.
    private boolean mPendingVisibilityUpdate;
    private int mCurrentOrientation;

    /**
     * @param bottomBarContainer The {@link FrameLayout} for the bottom bar.
     * @param requestLayerUpdateCallback A callback to request layer updates.
     * @param actionRegistry The {@link ActionRegistry}.
     * @param tabSupplier Supplier for the current tab.
     * @param themeColorProvider Theme color provider for the bottom bar.
     * @param homepageEnabledSupplier Supplier of whether the homepage is enabled.
     * @param profileSupplier Supplier of the current profile.
     * @param omniboxFocusStateSupplier Supplier of the omnibox focus state.
     * @param modalDialogManagerSupplier Supplier of the {@link ModalDialogManager}.
     */
    public BottomBarContainerCoordinator(
            FrameLayout bottomBarContainer,
            Callback<Boolean> requestLayerUpdateCallback,
            ActionRegistry actionRegistry,
            NullableObservableSupplier<Tab> tabSupplier,
            ThemeColorProvider themeColorProvider,
            NonNullObservableSupplier<Boolean> homepageEnabledSupplier,
            NullableObservableSupplier<Profile> profileSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            NonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mBottomBarContainer = bottomBarContainer;
        Context context = bottomBarContainer.getContext();
        mContext = context;
        mCurrentOrientation = context.getResources().getConfiguration().orientation;
        mContext.registerComponentCallbacks(mComponentCallbacks);
        mHandler = new Handler(Looper.getMainLooper());
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
                        omniboxFocusStateSupplier,
                        modalDialogManagerSupplier);
    }

    @Override
    public void initializeWithNative(
            BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        mVisibilityController = visibilityController;
        mOnModelTokenChange = onModelTokenChange;
        updateVisibility();
        onModelTokenChange();
    }

    @Override
    public void destroy() {
        if (mPendingVisibilityUpdate) {
            mHandler.removeCallbacks(mModelTokenChangeRunnable);
            mPendingVisibilityUpdate = false;
        }
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
        mBottomBarCoordinator.destroy();
    }

    @Override
    public void onVisibilityChanged(boolean isVisible) {
        mIsVisible = isVisible;
        updateVisibility();
    }

    @Override
    public void onModelTokenChange() {
        if (mOnModelTokenChange != null) {
            mOnModelTokenChange.onResult(new Object());
        }
        mPendingVisibilityUpdate = false;
    }

    @Override
    public void onBackgroundColorChanged() {
        mRequestLayerUpdateCallback.onResult(false);
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

        onModelTokenChange();
        mRequestLayerUpdateCallback.onResult(true);
    }

    private void updateVisibility() {
        mBottomBarContainer.setVisibility(mIsVisible ? View.VISIBLE : View.GONE);
        if (mVisibilityController != null) {
            mVisibilityController.setBottomControlsVisible(mIsVisible);
        }
    }

    /*package*/ ComponentCallbacks getComponentCallbacksForTesting() {
        return mComponentCallbacks;
    }
}
