// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;

/**
 * Business logic for the container the hosts the Continuous Search Navigation UI. The container
 * is part of the top browser controls and aligns below the top toolbar.
 */
class ContinuousSearchContainerMediator implements BrowserControlsStateProvider.Observer {
    private PropertyModel mModel;
    private final HashSet<Callback<Integer>> mObservers = new HashSet<>();
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Runnable mInitializeLayout;
    private final Supplier<Boolean> mCanAnimateNativeBrowserControls;
    private Runnable mRequestLayout;
    private final Supplier<Integer> mDefaultTopContainerHeightSupplier;

    private boolean mInitialized;
    private boolean mIsVisible;
    private boolean mIsTabObscured;
    private int mJavaLayoutHeight;

    ContinuousSearchContainerMediator(BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Boolean> canAnimateNativeBrowserControls,
            Supplier<Integer> defaultTopContainerHeightSupplier, Runnable initializeLayout) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mCanAnimateNativeBrowserControls = canAnimateNativeBrowserControls;
        mDefaultTopContainerHeightSupplier = defaultTopContainerHeightSupplier;
        mInitializeLayout = initializeLayout;
    }

    void onLayoutInitialized(PropertyModel model, Runnable requestLayout) {
        mModel = model;
        mRequestLayout = requestLayout;
    }

    /**
     * Called when the obscurity state of the current Tab changes.
     * @param isObscured Whether the tab is obscured.
     */
    void updateTabObscured(boolean isObscured) {
        mIsTabObscured = isObscured;
        if (mModel == null) return;

        mModel.set(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY,
                !mIsTabObscured && mIsVisible ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Displays the container. This will increase the top controls height with an animation that
     * is controlled by cc and displays the container.
     */
    void show() {
        if (mIsVisible) return;

        mInitializeLayout.run();
        mInitialized = true;
        if (mJavaLayoutHeight == 0) {
            mRequestLayout.run();
        } else {
            updateVisibility(true);
        }
    }

    /**
     * Hides the container. This will decrease the top controls height with an animation that
     * is controlled by cc and hides the container.
     */
    void hide() {
        if (!mInitialized || !mIsVisible) return;

        updateVisibility(false);
    }

    void setJavaHeight(int javaHeight) {
        if (mJavaLayoutHeight > 0 || javaHeight <= 0) return;

        mJavaLayoutHeight = javaHeight;
        updateVisibility(true);
    }

    private void updateVisibility(boolean isVisible) {
        mIsVisible = isVisible;
        mBrowserControlsStateProvider.addObserver(this);

        for (Callback<Integer> observer : mObservers) {
            observer.onResult(isVisible ? mJavaLayoutHeight : 0);
        }
    }

    void addHeightObserver(Callback<Integer> observer) {
        mObservers.add(observer);
    }

    void removeHeightObserver(Callback<Integer> observer) {
        mObservers.remove(observer);
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        // Whether container height is part of top controls height.
        boolean isIncludedInHeight = mBrowserControlsStateProvider.getTopControlsHeight()
                > mDefaultTopContainerHeightSupplier.get()
                        + mBrowserControlsStateProvider.getTopControlsMinHeight();
        // Whether the part of top controls that is not included in min height visible.
        boolean isNonMinHeightTopControlsVisible = topOffset
                        + mBrowserControlsStateProvider.getTopControlsHeight()
                        - mBrowserControlsStateProvider.getTopControlsMinHeight()
                > 0;
        // Whether container is at least partly visible.
        boolean isUiVisible = isIncludedInHeight && isNonMinHeightTopControlsVisible;
        final boolean uiFullyVisible = isUiVisible && topOffset == 0;
        int yOffset = topOffset + mBrowserControlsStateProvider.getTopControlsMinHeight()
                + mDefaultTopContainerHeightSupplier.get();
        mModel.set(ContinuousSearchContainerProperties.VERTICAL_OFFSET, yOffset);

        // Only show the composited view when the UI is partly visible (mid transition) and native
        // can run animations.
        mModel.set(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE,
                !mIsTabObscured
                        && (!uiFullyVisible && isUiVisible
                                && mCanAnimateNativeBrowserControls.get()));

        // If we're running the animations in native, the Android view should only be visible when
        // the container is fully shown. Otherwise, the Android view will be visible if it's within
        // screen boundaries.
        mModel.set(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY,
                mIsTabObscured
                        ? View.INVISIBLE
                        : !uiFullyVisible && isUiVisible && mCanAnimateNativeBrowserControls.get()
                                ? View.GONE
                                : ((isUiVisible && !mCanAnimateNativeBrowserControls.get())
                                                        || uiFullyVisible
                                                ? View.VISIBLE
                                                : View.GONE));

        final boolean doneHiding = !isUiVisible && !mIsVisible;
        if (doneHiding) {
            mBrowserControlsStateProvider.removeObserver(this);
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
