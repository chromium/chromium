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
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Root component for the HistoryClusters UI component, which displays lists of related history
 * visits grouped into clusters.
 */
public class HistoryClustersCoordinator {
    private final HistoryClustersMediator mMediator;
    private final ModelList mModelList;
    private SimpleRecyclerViewAdapter mAdapter;
    private final Context mContext;

    /**
     * Construct a new HistoryClustersCoordinator.
     * @param profile The profile from which the coordinator should access history data.
     * @param context Android context from which UI configuration (strings, colors etc.) should be
     *         derived.
     */
    public HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Context context) {
        mContext = context;
        mModelList = new ModelList();
        mMediator = new HistoryClustersMediator(HistoryClustersBridge.getForProfile(profile),
                new LargeIconBridge(profile), context, context.getResources(), mModelList);
    }

    public void destroy() {
        mMediator.destroy();
    }

    void inflate() {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.VISIT, this::buildVisitView, HistoryClustersViewBinder::bindVisitView);

        View contentView =
                LayoutInflater.from(mContext).inflate(R.layout.history_clusters_content_view, null);
        SelectableListLayout listLayout = contentView.findViewById(R.id.selectable_list);
        RecyclerView recyclerView = listLayout.initializeRecyclerView(mAdapter);

        recyclerView.setLayoutManager(new LinearLayoutManager(
                recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.setItemAnimator(null);

        mMediator.query("");
    }

    private View buildVisitView(ViewGroup parent) {
        SelectableItemView<ClusterVisit> itemView =
                (SelectableItemView<ClusterVisit>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster_visit, parent, false);
        return itemView;
    }
}