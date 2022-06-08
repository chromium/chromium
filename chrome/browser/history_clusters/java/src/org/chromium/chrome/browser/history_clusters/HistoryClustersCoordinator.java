// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Root component for the HistoryClusters UI component, which displays lists of related history
 * visits grouped into clusters.
 */
public class HistoryClustersCoordinator implements OnMenuItemClickListener {
    private final HistoryClustersMediator mMediator;
    private final ModelList mModelList;
    private final HistoryClustersDelegate mDelegate;
    private SimpleRecyclerViewAdapter mAdapter;
    private final Activity mActivity;
    private boolean mActivityViewInflated;
    private final PropertyModel mToolbarModel;
    private ViewGroup mActivityContentView;
    private HistoryClustersToolbar mToolbar;
    private SelectionDelegate mSelectionDelegate;
    private SelectableListLayout mSelectableListLayout;

    /**
     * Construct a new HistoryClustersCoordinator.
     * @param profile The profile from which the coordinator should access history data.
     * @param activity Activity in which this UI resides.
     * @param historyClustersDelegate Delegate that provides functionality that must be implemented
     *         externally, e.g. populating intents targeting activities we can't reference directly.
     */
    public HistoryClustersCoordinator(@NonNull Profile profile, @NonNull Activity activity,
            TemplateUrlService templateUrlService,
            HistoryClustersDelegate historyClustersDelegate) {
        mActivity = activity;
        mDelegate = historyClustersDelegate;
        mModelList = new ModelList();
        mToolbarModel = new PropertyModel.Builder(HistoryClustersToolbarProperties.ALL_KEYS)
                                .with(HistoryClustersToolbarProperties.QUERY_STATE,
                                        QueryState.forQueryless())
                                .build();
        mMediator = new HistoryClustersMediator(HistoryClustersBridge.getForProfile(profile),
                new LargeIconBridge(profile), mActivity, mActivity.getResources(), mModelList,
                mToolbarModel, mDelegate, System::currentTimeMillis, templateUrlService);
    }

    public void destroy() {
        mMediator.destroy();
        if (mActivityViewInflated) {
            mSelectableListLayout.onDestroyed();
        }
    }

    public void setQueryState(QueryState queryState) {
        mMediator.setQueryState(queryState);
    }

    /**
     * Opens the History Clusters UI. On phones this opens the History Activity; on tablets, it
     * navigates to a NativePage in the active tab.
     * @param query The preset query to populate when opening the UI.
     */
    public void openHistoryClustersUi(String query) {
        mMediator.openHistoryClustersUi(query);
    }

    /** Gets the root view for a "full activity" presentation of the user's history clusters. */
    public ViewGroup getActivityContentView() {
        if (!mActivityViewInflated) {
            inflateActivityView();
        }

        return mActivityContentView;
    }

    void inflateActivityView() {
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);
        mAdapter.registerType(
                ItemType.VISIT, this::buildVisitView, HistoryClustersViewBinder::bindVisitView);
        mAdapter.registerType(ItemType.CLUSTER, this::buildClusterView,
                HistoryClustersViewBinder::bindClusterView);
        mAdapter.registerType(ItemType.RELATED_SEARCHES, this::buildRelatedSearchesView,
                HistoryClustersViewBinder::bindRelatedSearchesView);
        mAdapter.registerType(ItemType.TOGGLE, mDelegate::getToggleView,
                HistoryClustersViewBinder::bindToggleView);

        LayoutInflater layoutInflater = LayoutInflater.from(mActivity);
        mActivityContentView = (ViewGroup) layoutInflater.inflate(
                R.layout.history_clusters_activity_content, null);

        mSelectableListLayout = mActivityContentView.findViewById(R.id.selectable_list);
        mSelectableListLayout.setEmptyViewText(R.string.history_manager_empty);
        RecyclerView recyclerView = mSelectableListLayout.initializeRecyclerView(mAdapter);

        recyclerView.setLayoutManager(new LinearLayoutManager(
                recyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
        recyclerView.setItemAnimator(null);
        recyclerView.addOnScrollListener(mMediator);

        mSelectionDelegate = new SelectionDelegate<>();
        mToolbar = (HistoryClustersToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.history_clusters_toolbar, mSelectionDelegate,
                R.string.history_clusters_journeys_tab_label, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, this, true);
        mToolbar.initializeSearchView(
                mMediator, R.string.history_clusters_search_your_journeys, R.id.search_menu_id);
        mSelectableListLayout.configureWideDisplayStyle();
        mToolbar.setSearchEnabled(true);
        if (!mDelegate.isSeparateActivity()) {
            mToolbar.getMenu().removeItem(R.id.close_menu_id);
        }

        PropertyModelChangeProcessor.create(
                mToolbarModel, mToolbar, HistoryClustersViewBinder::bindToolbar);
        PropertyModelChangeProcessor.create(
                mToolbarModel, mSelectableListLayout, HistoryClustersViewBinder::bindListLayout);

        mActivityViewInflated = true;
    }

    private View buildClusterView(ViewGroup parent) {
        SelectableItemView<HistoryCluster> clusterView =
                (SelectableItemView<HistoryCluster>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster, parent, false);
        return clusterView;
    }

    private View buildVisitView(ViewGroup parent) {
        SelectableItemView<ClusterVisit> itemView =
                (SelectableItemView<ClusterVisit>) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.history_cluster_visit, parent, false);
        return itemView;
    }

    private View buildRelatedSearchesView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.history_clusters_related_searches_view, parent, false);
    }

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.search_menu_id) {
            mMediator.setQueryState(QueryState.forQuery(""));
            return true;
        }
        if (menuItem.getItemId() == R.id.close_menu_id && mDelegate.isSeparateActivity()) {
            mActivity.finish();
            return true;
        }
        return false;
    }
}