// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.util.Pair;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;

/**
 * An implementation of {@link MonotonicObservableSupplier} that monitors changes to browser
 * controls and updates a {@link Pair} indicating top/bottom margins in px for Views that should be
 * inset by the browser control(s) height(s).
 */
@NullMarked
public class BrowserControlsMarginAdapter
        implements BrowserControlsStateProvider.Observer, Destroyable {
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final SettableMonotonicObservableSupplier<Pair<Integer, Integer>> mTargetSupplier;

    private BrowserControlsMarginAdapter(
            BrowserControlsStateProvider browserControlsStateProvider,
            SettableMonotonicObservableSupplier<Pair<Integer, Integer>> targetSupplier) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTargetSupplier = targetSupplier;
    }

    public static Destroyable create(
            BrowserControlsStateProvider browserControlsStateProvider,
            SettableMonotonicObservableSupplier<Pair<Integer, Integer>> targetSupplier) {
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

    @Override
    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
        updateMargins();
    }

    private void updateMargins() {
        // TODO(crbug.com/542066164): Consider refactoring this flow to also account for Side UI
        //  values, rather than assuming it will only ever be called to set top/bottom margins. e.g.
        //  create some new component that observes both Browser Controls and Side UI state.
        int topMargin =
                mBrowserControlsStateProvider.getTopControlsHeight()
                        + mBrowserControlsStateProvider.getTopControlOffset();
        int bottomMargin =
                mBrowserControlsStateProvider.getBottomControlsHeight()
                        - mBrowserControlsStateProvider.getBottomControlOffset();
        mTargetSupplier.set(new Pair<>(topMargin, bottomMargin));
    }
}
