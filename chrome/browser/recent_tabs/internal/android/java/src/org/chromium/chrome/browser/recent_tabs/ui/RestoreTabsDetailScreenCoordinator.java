// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the detail screens (device select, review tabs) of the Restore Tabs on FRE promo.
 */
public class RestoreTabsDetailScreenCoordinator {
    private final RecyclerView mRecyclerView;

    /** The delegate of the class. */
    public interface Delegate {
        /** The user clicked on the select/deselect all tabs item. */
        void onChangeSelectionStateForAllTabs();
        /** The user clicked on restoring all selected tabs. */
        void onSelectedTabsChosen();
    }

    public RestoreTabsDetailScreenCoordinator(Context context, View view, PropertyModel model) {
        mRecyclerView = view.findViewById(R.id.restore_tabs_detail_screen_recycler_view);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new RestoreTabsDetailItemDecoration(context.getResources().getDimensionPixelSize(
                        R.dimen.restore_tabs_detail_sheet_spacing_vertical)));

        RestoreTabsDetailScreenViewBinder.ViewHolder viewHolder =
                new RestoreTabsDetailScreenViewBinder.ViewHolder(view);

        PropertyModelChangeProcessor.create(
                model, viewHolder, RestoreTabsDetailScreenViewBinder::bind);
    }
}
