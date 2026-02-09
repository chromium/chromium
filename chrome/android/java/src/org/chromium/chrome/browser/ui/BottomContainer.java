// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.ui.base.ApplicationViewportInsetTracker;
import org.chromium.ui.base.ViewportInsets;

/**
 * The container that holds both infobars and snackbars. It will be translated up and down when the
 * bottom controls' offset changes.
 */
@NullMarked
public class BottomContainer extends FrameLayout
        implements Destroyable, BrowserControlsStateProvider.Observer {
    /** An observer of the viewport insets to change this container's position. */
    private final Callback<ViewportInsets> mInsetObserver;

    /** The {@link BrowserControlsStateProvider} to listen for controls offset changes. */
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** {@link ApplicationViewportInsetTracker} to listen for viewport-shrinking features. */
    private NonNullObservableSupplier<ViewportInsets> mViewportInsetSupplier;

    /** The desired Y offset if unaffected by other UI. */
    private float mBaseYOffset;

    private @Nullable MonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier;

    /** Constructor for XML inflation. */
    public BottomContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
        mInsetObserver = (unused) -> setTranslationY(mBaseYOffset);
    }

    /** Initializes this container. */
    @Initializer
    public void initialize(
            BrowserControlsStateProvider browserControlsStateProvider,
            NonNullObservableSupplier<ViewportInsets> viewportInsetSupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        mViewportInsetSupplier = viewportInsetSupplier;
        mViewportInsetSupplier.addSyncObserverAndPostIfNonNull(mInsetObserver);
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
        setTranslationY(mBaseYOffset);
    }

    // BrowserControlsStateProvidder.Observer methods
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
        setTranslationY(mBaseYOffset);
    }

    @Override
    public void setTranslationY(float y) {

        mBaseYOffset = y;

        float offsetFromControls =
                mBrowserControlsStateProvider.getBottomControlOffset()
                        - mBrowserControlsStateProvider.getBottomControlsHeight();
        offsetFromControls -= assumeNonNull(mViewportInsetSupplier.get()).viewVisibleHeightInset;

        int bottomInset =
                mEdgeToEdgeControllerSupplier != null && mEdgeToEdgeControllerSupplier.get() != null
                        ? mEdgeToEdgeControllerSupplier.get().getBottomInsetPx()
                        : 0;

        // The floating snackbar shouldn't scroll into the bottom inset.
        super.setTranslationY(Math.min(mBaseYOffset + offsetFromControls + bottomInset, 0));
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        setTranslationY(mBaseYOffset);
    }

    @CallSuper
    @Override
    public void destroy() {
        // Class may never have been initialized in the case of an early finish() call.
        if (mBrowserControlsStateProvider == null) return;
        mBrowserControlsStateProvider.removeObserver(this);
        mViewportInsetSupplier.removeObserver(mInsetObserver);
    }
}
