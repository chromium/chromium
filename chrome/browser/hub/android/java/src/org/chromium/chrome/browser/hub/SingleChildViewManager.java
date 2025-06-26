// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Manages attaching and detaching a dynamically supplied child view to a parent view. */
@NullMarked
public class SingleChildViewManager {
    private final Callback<@Nullable View> mOnViewChanged = this::onViewChanged;
    private final ViewGroup mContainerView;
    private final ObservableSupplier<@Nullable View> mChildViewSupplier;

    /**
     * @param containerView The container to attach views to.
     * @param viewSupplier The supplier of the current view.
     */
    public SingleChildViewManager(
            ViewGroup containerView, ObservableSupplier<@Nullable View> overlayViewSupplier) {
        mContainerView = containerView;
        mChildViewSupplier = overlayViewSupplier;
        mChildViewSupplier.addObserver(mOnViewChanged);
    }

    /** Destroys and removes observers. */
    public void destroy() {
        mChildViewSupplier.removeObserver(mOnViewChanged);
        onViewChanged(/* view= */ null);
    }

    private void onViewChanged(@Nullable View view) {
        if (view == null) {
            mContainerView.removeAllViews();
            mContainerView.setVisibility(View.GONE);
            return;
        }

        int childCount = mContainerView.getChildCount();
        if (childCount != 0) {
            assert childCount == 1;
            View child = mContainerView.getChildAt(0);
            if (child == view) return;
            mContainerView.removeAllViews();
            mContainerView.addView(view);
        } else {
            mContainerView.addView(view);
        }
        mContainerView.setVisibility(View.VISIBLE);
    }
}
