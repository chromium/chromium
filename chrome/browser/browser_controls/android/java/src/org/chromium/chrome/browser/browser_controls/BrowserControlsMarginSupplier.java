// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.graphics.Rect;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * An implementation of {@link DestroyableObservableSupplier} that monitors changes to browser
 * controls and updates a Rect indicating top/bottom margins for Views that should be inset by the
 * browser control(s) height(s).
 */
public class BrowserControlsMarginSupplier extends ObservableSupplierImpl<Rect>
        implements BrowserControlsStateProvider.Observer, DestroyableObservableSupplier<Rect> {
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    public BrowserControlsMarginSupplier(
            BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        updateMargins();
    }

    @Override
    public void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    private void updateMargins() {
        int topMargin =
                mBrowserControlsStateProvider.getTopControlsHeight()
                        + mBrowserControlsStateProvider.getTopControlOffset();
        int bottomMargin =
                mBrowserControlsStateProvider.getBottomControlsHeight()
                        - mBrowserControlsStateProvider.getBottomControlOffset();
        super.set(new Rect(0, topMargin, 0, bottomMargin));
    }
}
