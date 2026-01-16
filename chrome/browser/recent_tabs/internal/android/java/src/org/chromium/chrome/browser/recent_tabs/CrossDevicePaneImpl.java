// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneBase;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;

import java.util.function.DoubleConsumer;

/**
 * A {@link Pane} representing tabs from other devices. This feature is being migrated here from the
 * Recent Tabs page and used to exist under the foreign session tabs section.
 */
@NullMarked
public class CrossDevicePaneImpl extends PaneBase {
    private final MonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeController;
    private @Nullable CrossDeviceListCoordinator mCrossDeviceListCoordinator;

    /**
     * @param context Used to inflate UI.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    CrossDevicePaneImpl(
            Context context,
            DoubleConsumer onToolbarAlphaChange,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        super(PaneId.CROSS_DEVICE, context, onToolbarAlphaChange);
        mEdgeToEdgeController = edgeToEdgeSupplier;
        mReferenceButtonDataSupplier.set(
                new ResourceButtonData(
                        R.string.accessibility_cross_device_tabs,
                        R.string.accessibility_cross_device_tabs,
                        R.drawable.devices_black_24dp));
    }

    @Override
    public void destroy() {
        if (mCrossDeviceListCoordinator != null) {
            mCrossDeviceListCoordinator.destroy();
            mCrossDeviceListCoordinator = null;
        }
        mRootView.removeAllViews();
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT) {
            if (mCrossDeviceListCoordinator == null) {
                mCrossDeviceListCoordinator =
                        new CrossDeviceListCoordinator(mContext, mEdgeToEdgeController);
                mRootView.addView(mCrossDeviceListCoordinator.getView());
            } else {
                mCrossDeviceListCoordinator.buildCrossDeviceData();
            }
        } else if (loadHint == LoadHint.WARM && mCrossDeviceListCoordinator != null) {
            mCrossDeviceListCoordinator.clearCrossDeviceData();
        } else if (loadHint == LoadHint.COLD && mCrossDeviceListCoordinator != null) {
            destroy();
        }
    }
}
