// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

/** Orchestrates the displaying of a list of cross device tabs and related promos. */
public class CrossDeviceListCoordinator {
    private final View mView;

    private CrossDeviceListMediator mCrossDeviceListMediator;

    /**
     * @param context Used to load resources and views.
     */
    public CrossDeviceListCoordinator(Context context) {
        ModelList listItems = new ModelList();
        ModelListAdapter adapter = new ModelListAdapter(listItems);

        mView = LayoutInflater.from(context).inflate(R.layout.cross_device_pane, /* root= */ null);
        ListView listView = (ListView) mView.findViewById(R.id.cross_device_list_view);
        listView.setAdapter(adapter);

        mCrossDeviceListMediator = new CrossDeviceListMediator(listItems);
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
    }
}
