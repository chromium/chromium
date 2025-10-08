// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * The coordinator for the pinned tabs strip, which is a RecyclerView that shows a list of pinned
 * tabs.
 */
@NullMarked
public class PinnedTabStripCoordinator {
    private final PinnedTabStripMediator mMediator;
    private final TabListRecyclerView mPinnedTabsRecyclerView;

    /**
     * Constructor for PinnedTabStripCoordinator.
     *
     * @param activity The current activity.
     * @param parentView The parent view to attach the pinned tabs strip to.
     * @param tabListCoordinator The coordinator for the main tab grid.
     * @param tabGroupModelFilterSupplier The supplier of the current {@link TabGroupModelFilter}.
     */
    public PinnedTabStripCoordinator(
            Activity activity,
            ViewGroup parentView,
            TabListCoordinator tabListCoordinator,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier) {
        mPinnedTabsRecyclerView =
                (TabListRecyclerView)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.pinned_tab_strip_recycler_view_layout,
                                        parentView,
                                        /* attachToParent= */ false);
        TabListModel pinnedTabsModelList = new TabListModel();
        PropertyModel pinnedTabStripPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(PinnedTabStripProperties.IS_VISIBLE, false)
                        .with(PinnedTabStripProperties.SCROLL_TO_POSITION, -1)
                        .build();

        // Setup the adapter.
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(pinnedTabsModelList);
        adapter.registerType(
                UiType.TAB,
                parent -> createPinnedTabStripItemView(activity, parent),
                PinnedTabStripItemViewBinder::bind);

        LinearLayoutManager layoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false);
        mPinnedTabsRecyclerView.setLayoutManager(layoutManager);
        mPinnedTabsRecyclerView.setAdapter(adapter);

        PropertyModelChangeProcessor.create(
                pinnedTabStripPropertyModel,
                mPinnedTabsRecyclerView,
                PinnedTabStripViewBinder::bind);

        RecyclerView tabGridListRecyclerView = tabListCoordinator.getContainerView();
        TabListModel tabListModel = tabListCoordinator.getTabListModel();
        mMediator =
                createMediator(
                        activity,
                        tabGridListRecyclerView,
                        tabListCoordinator,
                        tabListModel,
                        pinnedTabsModelList,
                        pinnedTabStripPropertyModel,
                        tabGroupModelFilterSupplier);

        tabGridListRecyclerView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        mMediator.onScrolled();
                    }
                });
    }

    /** Returns the {@link TabListRecyclerView} for the pinned tabs strip. */
    public TabListRecyclerView getPinnedTabsRecyclerView() {
        return mPinnedTabsRecyclerView;
    }

    PinnedTabStripMediator createMediator(
            Activity activity,
            RecyclerView tabGridListRecyclerView,
            TabListCoordinator tabListCoordinator,
            TabListModel tabListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier) {
        GridLayoutManager tabGridListLayoutManager =
                (GridLayoutManager) tabGridListRecyclerView.getLayoutManager();
        assumeNonNull(tabGridListLayoutManager);
        return new PinnedTabStripMediator(
                activity,
                tabGridListLayoutManager,
                tabListCoordinator,
                tabListModel,
                pinnedTabsModelList,
                stripPropertyModel,
                tabGroupModelFilterSupplier);
    }

    private static PinnedTabStripItemView createPinnedTabStripItemView(
            Activity activity, ViewGroup parent) {
        return (PinnedTabStripItemView)
                LayoutInflater.from(activity)
                        .inflate(
                                R.layout.pinned_tab_strip_item,
                                parent,
                                /* attachToParent= */ false);
    }

    public void destroy() {
        mMediator.destroy();
    }
}
