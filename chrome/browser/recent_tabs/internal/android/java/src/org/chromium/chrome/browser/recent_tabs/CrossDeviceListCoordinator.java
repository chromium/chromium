// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.recent_tabs.ui.CrossDevicePaneView;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Orchestrates the displaying of a list of cross device tabs and related promos. */
public class CrossDeviceListCoordinator {
    private final CrossDevicePaneView mView;
    private final CrossDeviceListMediator mCrossDeviceListMediator;
    private @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /**
     * @param context Used to load resources and views.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     */
    public CrossDeviceListCoordinator(
            Context context, ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        ModelList listItems = new ModelList();
        ModelListAdapter adapter = new ModelListAdapter(listItems);

        mView =
                (CrossDevicePaneView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.cross_device_pane, /* root= */ null);
        ListView listView = (ListView) mView.findViewById(R.id.cross_device_list_view);
        listView.setAdapter(adapter);

        PropertyModel model = CrossDeviceListProperties.create();
        PropertyModelChangeProcessor.create(model, mView, CrossDeviceListViewBinder::bind);

        mCrossDeviceListMediator = new CrossDeviceListMediator(listItems, model);
        if (EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            listView, edgeToEdgeSupplier);
        }
    }

    /** Returns the root view of this component. */
    public View getView() {
        return mView;
    }

    /** Clears all model data associated with the cross device pane. */
    public void clearCrossDeviceData() {
        mCrossDeviceListMediator.clearModelList();
    }

    /** Builds all model data associated with the cross device pane. */
    public void buildCrossDeviceData() {
        mCrossDeviceListMediator.buildModelList();
    }

    /** Permanently cleans up this component. */
    public void destroy() {
        mCrossDeviceListMediator.destroy();
        if (mEdgeToEdgePadAdjuster != null) {
            mEdgeToEdgePadAdjuster.destroy();
        }
    }
}
