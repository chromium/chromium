// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * The container that holds both infobars and snackbars. It will be translated up and down when the
 * bottom controls' offset changes.
 */
public class BottomContainer
        extends FrameLayout implements Destroyable, BrowserControlsStateProvider.Observer {
    /** An observer of the viewport insets to change this container's position. */
    private final Callback<Integer> mInsetObserver;

    /** The {@link BrowserControlsStateProvider} to listen for controls offset changes. */
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    /** {@link ApplicationViewportInsetSupplier} to listen for viewport-shrinking features. */
    private ApplicationViewportInsetSupplier mViewportInsetSupplier;
    /** {@link ObservableSupplier} to listen for page-shrinking features. */
    private ObservableSupplier<Integer> mAutofillUiBottomInsetSupplier;

    /** The desired Y offset if unaffected by other UI. */
    private float mBaseYOffset;

    /** Constructor for XML inflation. */
    public BottomContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
        mInsetObserver = (inset) -> setTranslationY(mBaseYOffset);
    }

    /** Initializes this container. */
    public void initialize(BrowserControlsStateProvider browserControlsStateProvider,
            ApplicationViewportInsetSupplier viewportInsetSupplier,
            ObservableSupplier<Integer> autofillUiBottomInsetSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        mViewportInsetSupplier = viewportInsetSupplier;
        mAutofillUiBottomInsetSupplier = autofillUiBottomInsetSupplier;
        mViewportInsetSupplier.addObserver(mInsetObserver);
        mAutofillUiBottomInsetSupplier.addObserver(mInsetObserver);
        setTranslationY(mBaseYOffset);
    }

    // BrowserControlsStateProvidder.Observer methods
    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        setTranslationY(mBaseYOffset);
    }

    @Override
    public void setTranslationY(float y) {
        mBaseYOffset = y;

        float offsetFromControls = mBrowserControlsStateProvider.getBottomControlOffset()
                - mBrowserControlsStateProvider.getBottomControlsHeight();
        offsetFromControls -= mViewportInsetSupplier.get();
        offsetFromControls -= mAutofillUiBottomInsetSupplier.get();

        // Sit on top of either the bottom sheet or the bottom toolbar depending on which is larger
        // (offsets are negative).
        super.setTranslationY(mBaseYOffset + offsetFromControls);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        setTranslationY(mBaseYOffset);
    }

    @CallSuper
    @Override
    public void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
        mViewportInsetSupplier.removeObserver(mInsetObserver);
        mAutofillUiBottomInsetSupplier.removeObserver(mInsetObserver);
    }
}
