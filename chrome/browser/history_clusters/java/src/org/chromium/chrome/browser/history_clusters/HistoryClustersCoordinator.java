// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Root component for the HistoryClusters UI component, which displays lists of related history
 * visits grouped into clusters.
 */
public class HistoryClustersCoordinator {
    private final HistoryClustersMediator mMediator;
    private final ModelList mModelList;
    private final HistoryClustersBottomSheetContent mBottomSheetContent;
    private SimpleRecyclerViewAdapter mAdapter;
    private final Context mContext;
    private boolean mBottomSheetInflated;
    private final PropertyModel mBottomSheetToolbarModel;

    /**
     * Construct a new HistoryClustersCoordinator.
     * @param profile The profile from which the coordinator should access history data.
     * @param context Android context from which UI configuration (strings, colors etc.) should be
     *         derived.
     * @param bottomSheetController Controller for interacting with the bottom sheet system, e.g. to
     *         request to show our content.
     */
    public HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Context context,
            @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mModelList = new ModelList();
        mBottomSheetContent = new HistoryClustersBottomSheetContent();
        mBottomSheetToolbarModel =
                new PropertyModel(HistoryClustersBottomSheetToolbarProperties.ALL_KEYS);
        mMediator = new HistoryClustersMediator(HistoryClustersBridge.getForProfile(profile),
                new LargeIconBridge(profile), context, context.getResources(), mModelList,
                mBottomSheetToolbarModel, bottomSheetController, mBottomSheetContent);
    }

    public void destroy() {
        mMediator.destroy();
    }

    /** Shows the bottom sheet, populating it with clusters matching the given query. */
    public void showBottomSheet(String query) {
        if (!mBottomSheetInflated) {
            inflateBottomSheet();
        }
        mMediator.showBottomSheet(query);
    }

    void inflateBottomSheet() {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.VISIT, this::buildVisitView, HistoryClustersViewBinder::bindVisitView);

        LayoutInflater layoutInflater = LayoutInflater.from(mContext);
        View contentView =
                layoutInflater.inflate(R.layout.history_clusters_bottom_sheet_content, null);
        RecyclerView recyclerView = contentView.findViewById(R.id.recycler_view);
        recyclerView.setLayoutManager(new LinearLayoutManager(
                recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.setItemAnimator(null);

        View bottomSheetToolbar =
                layoutInflater.inflate(R.layout.history_clusters_bottom_sheet_toolbar, null);

        PropertyModelChangeProcessor.create(mBottomSheetToolbarModel, bottomSheetToolbar,
                HistoryClustersViewBinder::bindBottomSheetToolbar);

        mBottomSheetContent.setContentView(contentView);
        mBottomSheetContent.setToolbarView(bottomSheetToolbar);
        mBottomSheetInflated = true;
    }

    private View buildVisitView(ViewGroup parent) {
        SelectableItemView<ClusterVisit> itemView =
                (SelectableItemView<ClusterVisit>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster_visit, parent, false);
        return itemView;
    }
}