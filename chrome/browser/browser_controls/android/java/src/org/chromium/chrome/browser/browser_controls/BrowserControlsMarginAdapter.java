// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.graphics.Rect;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;

/**
 * An implementation of {@link ObservableSupplier} that monitors changes to browser controls and
 * updates a Rect indicating top/bottom margins for Views that should be inset by the browser
 * control(s) height(s).
 */
@NullMarked
public class BrowserControlsMarginAdapter
        implements BrowserControlsStateProvider.Observer, Destroyable {
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ObservableSupplierImpl<Rect> mTargetSupplier;

    private BrowserControlsMarginAdapter(
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplierImpl<Rect> targetSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTargetSupplier = targetSupplier;
    }

    public static Destroyable create(
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplierImpl<Rect> targetSupplier) {
        BrowserControlsMarginAdapter ret =
                new BrowserControlsMarginAdapter(browserControlsStateProvider, targetSupplier);
        browserControlsStateProvider.addObserver(ret);
        ret.updateMargins();
        return ret;
    }

    @Override
    public void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

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
        mTargetSupplier.set(new Rect(0, topMargin, 0, bottomMargin));
    }
}
