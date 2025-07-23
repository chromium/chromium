// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Empty implementation of HubBottomToolbarDelegate for testing purposes.
 *
 * <p>This implementation provides a basic empty bottom toolbar view and no bottom toolbar
 * functionality and can be used in tests to verify that the delegate integration works correctly
 * without requiring a full implementation.
 */
@NullMarked
public class EmptyHubBottomToolbarDelegate implements HubBottomToolbarDelegate {
    /** Visibility supplier that always emits false. */
    private final ObservableSupplierImpl<Boolean> mVisibilitySupplier =
            new ObservableSupplierImpl<>(false);

    @Override
    public @Nullable HubBottomToolbarView initializeBottomToolbarView(
            Context context,
            ViewGroup container,
            PaneManager paneManager,
            HubColorMixer hubColorMixer) {
        // Inflate the basic empty bottom toolbar layout
        HubBottomToolbarView hubBottomToolbarView =
                (HubBottomToolbarView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.hub_bottom_toolbar_layout, container, false);

        // Add the basic layout to container
        container.addView(hubBottomToolbarView);

        return hubBottomToolbarView;
    }

    @Override
    public boolean isBottomToolbarEnabled() {
        // Bottom toolbar is not enabled in the empty implementation
        return false;
    }

    @Override
    public ObservableSupplier<Boolean> getBottomToolbarVisibilitySupplier() {
        // Return a supplier that always indicates the toolbar is not visible
        return mVisibilitySupplier;
    }

    @Override
    public void destroy() {
        // No resources to clean up in the empty implementation
    }
}
