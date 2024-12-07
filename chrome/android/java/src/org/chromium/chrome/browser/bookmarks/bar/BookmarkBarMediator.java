// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the bookmark bar which provides users with bookmark access from top chrome. */
class BookmarkBarMediator implements BrowserControlsStateProvider.Observer {

    private final BrowserControlsManager mBrowserControlsManager;
    private final Callback<Integer> mHeightChangeCallback;
    private final ObservableSupplierImpl<Integer> mHeightSupplier;
    private final PropertyModel mModel;

    /**
     * Constructs the bookmark bar mediator.
     *
     * @param browserControlsManager the manager for browser control positioning/visibility.
     * @param heightChangeCallback a callback to notify of bookmark bar height change events.
     * @param model the model used to read/write bookmark bar properties.
     */
    public BookmarkBarMediator(
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull Callback<Integer> heightChangeCallback,
            @NonNull PropertyModel model) {
        mBrowserControlsManager = browserControlsManager;
        mBrowserControlsManager.addObserver(this);

        // NOTE: Height will be updated when binding the `HEIGHT_CHANGE_CALLBACK` property.
        mHeightSupplier = new ObservableSupplierImpl<Integer>(0);
        mHeightChangeCallback = heightChangeCallback;
        mHeightSupplier.addObserver(mHeightChangeCallback);

        mModel = model;
        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, mHeightSupplier::set);

        updateTopMargin();
        updateVisibility();
    }

    /** Destroys the bookmark bar mediator. */
    public void destroy() {
        mBrowserControlsManager.removeObserver(this);
        mHeightSupplier.removeObserver(mHeightChangeCallback);
        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, null);
    }

    /**
     * @return the supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mHeightSupplier;
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
        updateVisibility();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopMargin();
    }

    // TODO(crbug.com/339492600): Replace w/ positioning construct akin to `BottomControlsStacker`.
    private void updateTopMargin() {
        // NOTE: Top controls height is the sum of all top browser control heights which includes
        // that of the bookmark bar. Subtract the bookmark bar's height from the top controls height
        // when calculating top margin in order to bottom align the bookmark bar relative to other
        // top browser controls.
        mModel.set(
                BookmarkBarProperties.TOP_MARGIN,
                mBrowserControlsManager.getTopControlsHeight() - mHeightSupplier.get());
    }

    private void updateVisibility() {
        mModel.set(
                BookmarkBarProperties.VISIBILITY,
                mBrowserControlsManager.getTopControlOffset() == 0 ? View.VISIBLE : View.GONE);
    }
}
