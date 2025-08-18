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

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
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
     * @param mTabGridListRecyclerView The RecyclerView for the main tab list.
     * @param tabListModel The model for the main tab list.
     */
    public PinnedTabStripCoordinator(
            Activity activity,
            ViewGroup parentView,
            RecyclerView mTabGridListRecyclerView,
            TabListModel tabListModel) {
        mPinnedTabsRecyclerView =
                (TabListRecyclerView)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.tab_list_recycler_view_layout,
                                        parentView,
                                        /* attachToParent= */ false);
        TabListModel pinnedTabsModelList = new TabListModel();
        PropertyModel pinnedTabstripPropertyModel =
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
                pinnedTabstripPropertyModel,
                mPinnedTabsRecyclerView,
                PinnedTabStripViewBinder::bind);

        mMediator =
                createMediator(
                        mTabGridListRecyclerView,
                        tabListModel,
                        pinnedTabsModelList,
                        pinnedTabstripPropertyModel);

        mTabGridListRecyclerView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        mMediator.onScrolled();
                    }
                });
    }

    PinnedTabStripMediator createMediator(
            RecyclerView tabGridListRecyclerView,
            TabListModel tabListModel,
            TabListModel pinnedTabsModelList,
            PropertyModel stripPropertyModel) {
        GridLayoutManager tabGridListLayoutManager =
                (GridLayoutManager) tabGridListRecyclerView.getLayoutManager();
        assumeNonNull(tabGridListLayoutManager);
        return new PinnedTabStripMediator(
                tabGridListLayoutManager, tabListModel, pinnedTabsModelList, stripPropertyModel);
    }

    /** Returns the {@link TabListRecyclerView} for the pinned tabs strip for testing purposes. */
    TabListRecyclerView getPinnedTabsRecyclerViewForTesting() {
        return mPinnedTabsRecyclerView;
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
}
