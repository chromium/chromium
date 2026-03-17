// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;

/**
 * Implementation of {@link HubBottomToolbarDelegate} that provides bottom bar functionality for the
 * Hub.
 */
@NullMarked
public class HubBottomBarBottomToolbarDelegateImpl implements HubBottomToolbarDelegate {
    private final NonNullObservableSupplier<Boolean> mVisibilitySupplier =
            ObservableSuppliers.alwaysTrue();

    private final Context mContext;

    // Non-null after {@link #initializeBottomToolbarView} is called.
    private HubBottomToolbarView mHubBottomToolbarView;

    /**
     * @param context The context.
     */
    public HubBottomBarBottomToolbarDelegateImpl(Context context) {
        mContext = context;
        // TODO(crbug.com/491509787): Dynamically attach/detach a child view to the container.
    }

    @Initializer
    @Override
    public HubBottomToolbarView initializeBottomToolbarView(
            Context context,
            ViewGroup container,
            PaneManager paneManager,
            HubColorMixer hubColorMixer) {

        // Inflate the basic bottom toolbar layout. We assume it attaches an externally
        // provided view to the container as the prompt dictates.
        mHubBottomToolbarView =
                (HubBottomToolbarView)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.hub_bottom_toolbar_layout,
                                        container,
                                        /* attachToRoot= */ false);

        ViewGroup.LayoutParams params =
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        container.addView(mHubBottomToolbarView, params);

        return mHubBottomToolbarView;
    }

    /** Attaches the provided bottom bar view to the container. */
    @Override
    public void attachBottomBarView(View view) {
        mHubBottomToolbarView.addView(view);
    }

    @Override
    public boolean isBottomToolbarEnabled() {
        return true;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getBottomToolbarVisibilitySupplier() {
        return mVisibilitySupplier;
    }

    @Override
    public void destroy() {}
}
