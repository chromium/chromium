// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;

/**
 * Implementation of {@link HubBottomToolbarDelegate} that provides bottom bar functionality for the
 * Hub.
 */
@NullMarked
public class HubBottomBarBottomToolbarDelegateImpl implements HubBottomToolbarDelegate {
    private final NonNullObservableSupplier<Boolean> mVisibilitySupplier =
            ObservableSuppliers.alwaysTrue();

    private final NullableObservableSupplier<Tab> mCurrentTabSupplier;
    private final NonNullObservableSupplier<Boolean> mIsHidingSupplier;

    // Non-null after {@link #initializeBottomToolbarView} is called.
    private HubBottomToolbarView mHubBottomToolbarView;
    private @Nullable HubColorMixer mHubColorMixer;
    private @Nullable BottomBarHubColorMixerAdapter mColorMixerAdapter;

    /**
     * @param currentTabSupplier The supplier of the current tab.
     * @param isHidingSupplier Supplies whether the Hub is currently hiding / exiting.
     */
    public HubBottomBarBottomToolbarDelegateImpl(
            NullableObservableSupplier<Tab> currentTabSupplier,
            NonNullObservableSupplier<Boolean> isHidingSupplier) {
        mCurrentTabSupplier = currentTabSupplier;
        mIsHidingSupplier = isHidingSupplier;
        // TODO(crbug.com/491509787): Dynamically attach/detach a child view to the container.
    }

    public HubBottomBarBottomToolbarDelegateImpl() {
        this(ObservableSuppliers.alwaysNull(), ObservableSuppliers.alwaysFalse());
    }

    @Initializer
    @Override
    public HubBottomToolbarView initializeBottomToolbarView(
            Context context,
            ViewGroup container,
            PaneManager paneManager,
            HubColorMixer hubColorMixer) {
        mHubColorMixer = hubColorMixer;
        // Inflate the basic bottom toolbar layout. We assume it attaches an externally
        // provided view to the container as the prompt dictates.
        mHubBottomToolbarView =
                (HubBottomToolbarView)
                        LayoutInflater.from(context)
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
        assert mHubBottomToolbarView != null
                : "attachBottomBarView called before initializeBottomToolbarView";
        mHubBottomToolbarView.addView(view);
        if (view instanceof BottomBarView bottomBarView) {
            if (mColorMixerAdapter != null) {
                mColorMixerAdapter.destroy();
            }
            mColorMixerAdapter =
                    new BottomBarHubColorMixerAdapter(
                            bottomBarView, mHubColorMixer, mCurrentTabSupplier, mIsHidingSupplier);
        }
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
    public void destroy() {
        if (mColorMixerAdapter != null) {
            mColorMixerAdapter.destroy();
            mColorMixerAdapter = null;
        }
    }

    @Nullable BottomBarHubColorMixerAdapter getBottomBarColorMixerAdapterForTesting() {
        return mColorMixerAdapter;
    }
}
