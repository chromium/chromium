// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.HeightObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.HashSet;

/**
 * Business logic for the container the hosts the Continuous Search Navigation UI. The container
 * is part of the top browser controls and aligns below the top toolbar.
 */
class ContinuousSearchContainerMediator implements BrowserControlsStateProvider.Observer {
    private PropertyModel mModel;
    private final HashSet<HeightObserver> mObservers = new HashSet<>();
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final LayoutStateProvider mLayoutStateProvider;
    private final Runnable mInitializeLayout;
    private final Supplier<Boolean> mCanAnimateNativeBrowserControls;
    private Runnable mRequestLayout;
    private final Supplier<Integer> mDefaultTopContainerHeightSupplier;
    private final Callback<Boolean> mHideToolbarShadow;
    private final TokenHolder mVisibilityTokenHolder =
            new TokenHolder(this::onVisibilityTokenUpdate);

    private Runnable mOnFinishedHide;
    private Runnable mOnFinishedShow;
    private boolean mInitialized;
    private boolean mIsVisible;
    private boolean mWantVisible;
    private boolean mListenForContentOffset;
    private boolean mAndroidViewSuppressed;
    private int mJavaLayoutHeight;
    private int mObscuredToken = TokenHolder.INVALID_TOKEN;

    ContinuousSearchContainerMediator(BrowserControlsStateProvider browserControlsStateProvider,
            LayoutStateProvider layoutStateProvider,
            Supplier<Boolean> canAnimateNativeBrowserControls,
            Supplier<Integer> defaultTopContainerHeightSupplier, Runnable initializeLayout,
            Callback<Boolean> hideToolbarShadow) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mLayoutStateProvider = layoutStateProvider;
        mCanAnimateNativeBrowserControls = canAnimateNativeBrowserControls;
        mDefaultTopContainerHeightSupplier = defaultTopContainerHeightSupplier;
        mInitializeLayout = initializeLayout;
        mHideToolbarShadow = hideToolbarShadow;
        mBrowserControlsStateProvider.addObserver(this);
    }

    void onLayoutInitialized(PropertyModel model, Runnable requestLayout) {
        mModel = model;
        mRequestLayout = requestLayout;
    }

    int hideContainer() {
        return mVisibilityTokenHolder.acquireToken();
    }

    void showContainer(int token) {
        mVisibilityTokenHolder.releaseToken(token);
    }

    /**
     * Called when the obscurity state of the current Tab changes.
     * @param isObscured Whether the tab is obscured.
     */
    void updateTabObscured(boolean isObscured) {
        if (isObscured) {
            assert mObscuredToken == TokenHolder.INVALID_TOKEN;
            mObscuredToken = hideContainer();
            return;
        }
        assert mObscuredToken != TokenHolder.INVALID_TOKEN;
        showContainer(mObscuredToken);
        mObscuredToken = TokenHolder.INVALID_TOKEN;
    }

    private void onVisibilityTokenUpdate() {
        if (mModel == null) return;

        // Avoid showing on unobscure if the UI should be hidden.
        if (!mWantVisible && !mVisibilityTokenHolder.hasTokens()) return;

        // Avoid obscuring if already in the correct state.
        if (mIsVisible != mVisibilityTokenHolder.hasTokens()) return;

        updateVisibility(!mVisibilityTokenHolder.hasTokens(), true);
    }

    /**
     * Displays the container. This will increase the top controls height with an animation that
     * is controlled by cc and displays the container.
     */
    void show(Runnable onFinishedShow) {
        TraceEvent.begin("ContinuousSearchContainerMediator#show");
        mOnFinishedHide = null;
        mOnFinishedShow = onFinishedShow;
        mWantVisible = true;

        if (mIsVisible) {
            runOnFinishedShow();
            TraceEvent.end("ContinuousSearchContainerMediator#show");
            return;
        }

        mInitializeLayout.run();
        mInitialized = true;
        if (mJavaLayoutHeight == 0) {
            mRequestLayout.run();
        } else {
            updateVisibility(true, true);
        }
        TraceEvent.end("ContinuousSearchContainerMediator#show");
    }

    /**
     * Hides the container. This will decrease the top controls height with an animation that
     * is controlled by cc and hides the container.
     */
    void hide(Runnable onFinishedHide) {
        TraceEvent.begin("ContinuousSearchContainerMediator#hide");
        mOnFinishedHide = onFinishedHide;
        mOnFinishedShow = null;
        mWantVisible = false;

        if (!mInitialized || !mIsVisible) {
            runOnFinishedHide();
            TraceEvent.end("ContinuousSearchContainerMediator#hide");
            return;
        }

        updateVisibility(false, true);
        TraceEvent.end("ContinuousSearchContainerMediator#hide");
    }

    void setJavaHeight(int javaHeight) {
        if (mJavaLayoutHeight > 0 || javaHeight <= 0) return;

        mJavaLayoutHeight = javaHeight;
        updateVisibility(true, true);
    }

    private void updateVisibility(boolean isVisible, boolean forceNoAnimation) {
        TraceEvent.begin("ContinuousSearchContainerMediator#updateVisibility");
        mIsVisible = isVisible;
        mListenForContentOffset = true;
        if (isVisible) {
            mHideToolbarShadow.onResult(true);
        }

        for (HeightObserver observer : mObservers) {
            observer.onHeightChange(isVisible ? mJavaLayoutHeight : 0,
                    !forceNoAnimation && mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING));
        }
        TraceEvent.end("ContinuousSearchContainerMediator#updateVisibility");
    }

    void addHeightObserver(HeightObserver observer) {
        mObservers.add(observer);
    }

    void removeHeightObserver(HeightObserver observer) {
        mObservers.remove(observer);
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        updateState();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        // When animations are disabled {@link #onControlsOffsetChanged} isn't always called when
        // navigating between pages as the topbar doesn't always move.
        // TODO(crbug/1217105): updateState() should be calculated relative to the content offset
        // rather than top offset to ensure this works properly regardless of whether this is
        // animated.
        if (mModel == null
                || mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY)
                        != View.VISIBLE) {
            // Avoid triggering on initial height change when making visible.
            return;
        }
        updateState();
    }

    private void updateState() {
        if (!mListenForContentOffset) return;
        TraceEvent.begin("ContinuousSearchContainerMediator#updateState");

        final int topControlsHeight = mBrowserControlsStateProvider.getTopControlsHeight();
        final int topControlsMinHeight = mBrowserControlsStateProvider.getTopControlsMinHeight();

        // Whether container height is part of top controls height.
        boolean isIncludedInHeight =
                topControlsHeight > mDefaultTopContainerHeightSupplier.get() + topControlsMinHeight;

        final int topOffset = mBrowserControlsStateProvider.getTopControlOffset();
        // Whether the part of top controls that is not included in min height visible.
        boolean isNonMinHeightTopControlsVisible =
                (topOffset + topControlsHeight - topControlsMinHeight) > 0;
        // Whether container is at least partly visible.
        boolean isUiVisible = isIncludedInHeight && isNonMinHeightTopControlsVisible;
        final boolean uiFullyVisible = isUiVisible && topOffset == 0;
        int yOffset = topOffset + topControlsMinHeight + mDefaultTopContainerHeightSupplier.get();
        mModel.set(ContinuousSearchContainerProperties.VERTICAL_OFFSET, yOffset);

        // Show the composited view when the UI is at least partly visible and native
        // can run animations. This change will happen on the next composited frame.
        final boolean showCompositedView = !mVisibilityTokenHolder.hasTokens() && isUiVisible
                && mCanAnimateNativeBrowserControls.get();
        mModel.set(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE, showCompositedView);

        // If we're running the animations in native, the Android view should only be visible when
        // the container is fully shown. Otherwise, the Android view will be visible if it's within
        // screen boundaries. This change will happen immediately.
        final int androidViewState = (mAndroidViewSuppressed || mVisibilityTokenHolder.hasTokens())
                ? View.INVISIBLE
                : !uiFullyVisible && isUiVisible && mCanAnimateNativeBrowserControls.get()
                        ? View.GONE
                        : ((isUiVisible && !mCanAnimateNativeBrowserControls.get())
                                                || uiFullyVisible
                                        ? View.VISIBLE
                                        : View.GONE);
        mModel.set(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY, androidViewState);

        if (androidViewState == View.VISIBLE) runOnFinishedShow();

        final boolean doneHiding = !isUiVisible && !mIsVisible;
        if (doneHiding) {
            mHideToolbarShadow.onResult(false);
            runOnFinishedHide();
            mListenForContentOffset = false;
        }
        TraceEvent.end("ContinuousSearchContainerMediator#updateState");
    }

    /**
     * Updates the Android visibility in response to {@link SceneOverlay} Android view suppression
     * requests.
     * @param visibility The Android View is visiblility.
     */
    @Override
    public void onAndroidVisibilityChanged(int visibility) {
        if (mModel == null) return;

        mAndroidViewSuppressed = visibility != View.VISIBLE;
        final int androidViewState =
                (!mAndroidViewSuppressed && mIsVisible && !mVisibilityTokenHolder.hasTokens())
                ? View.VISIBLE
                : View.INVISIBLE;

        mModel.set(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY, androidViewState);
    }

    // TODO(crbug.com/1232595): merge onFinishedShow and onFinishedHide logic.
    @VisibleForTesting
    void runOnFinishedHide() {
        if (mOnFinishedHide != null) {
            mOnFinishedHide.run();
            mOnFinishedHide = null;
        }
    }

    void runOnFinishedShow() {
        if (mOnFinishedShow != null) {
            mOnFinishedShow.run();
            mOnFinishedShow = null;
        }
    }

    void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

    @VisibleForTesting
    boolean isVisibleForTesting() {
        return mIsVisible;
    }
}
