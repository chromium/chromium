// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
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
    private final TabListCoordinator mTabListEditorCoordinator;

    private final TabListCoordinator.TabListItemSizeChangedObserver
            mTabListItemSizeChangedObserver =
                    new TabListCoordinator.TabListItemSizeChangedObserver() {
                        @Override
                        public void onSizeChanged(int spanCount, Size cardSize) {
                            mMediator.onSizeChanged(cardSize);
                        }
                    };

    /**
     * Constructor for PinnedTabStripCoordinator.
     *
     * @param activity The current activity.
     * @param parentView The parent view to attach the pinned tabs strip to.
     */
    public PinnedTabStripCoordinator(
            Activity activity, ViewGroup parentView, TabListCoordinator tabListCoordinator) {
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

        mTabListEditorCoordinator = tabListCoordinator;

        RecyclerView tabGridListRecyclerView = tabListCoordinator.getContainerView();
        TabListModel tabListModel = tabListCoordinator.getTabListModel();
        mMediator =
                createMediator(
                        activity,
                        tabGridListRecyclerView,
                        tabListModel,
                        pinnedTabsModelList,
                        pinnedTabStripPropertyModel);

        tabGridListRecyclerView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        mMediator.onScrolled();
                    }
                });

        tabListCoordinator.addTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
    }

    /** Returns the {@link TabListRecyclerView} for the pinned tabs strip. */
    public TabListRecyclerView getPinnedTabsRecyclerView() {
        return mPinnedTabsRecyclerView;
    }

    PinnedTabStripMediator createMediator(
            Activity activity,
            RecyclerView tabGridListRecyclerView,
            TabListModel tabListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel) {
        GridLayoutManager tabGridListLayoutManager =
                (GridLayoutManager) tabGridListRecyclerView.getLayoutManager();
        assumeNonNull(tabGridListLayoutManager);
        return new PinnedTabStripMediator(
                activity,
                tabGridListLayoutManager,
                tabListModel,
                pinnedTabsModelList,
                stripPropertyModel);
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
        mTabListEditorCoordinator.removeTabListItemSizeChangedObserver(
                mTabListItemSizeChangedObserver);
    }
}
