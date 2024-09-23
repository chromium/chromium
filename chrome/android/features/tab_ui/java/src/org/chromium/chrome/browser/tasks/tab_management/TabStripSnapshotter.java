// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListPropertyChangeFilter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * Observes a {@link ModelList} and returns tokens for conceptual snapshots of the model state.
 * The opaque token can be compared against other tokens with {@link Object#equals(Object)}. The
 * purpose of these tokens is to decide when the bottom toolbar should take a bitmap capture in
 * preparation for browser controls being scrolled off the screen. This class only watches
 * properties that directly effect the steady state of the view, and thus it implicitly tightly
 * coupled with the TabListMode.STRIP mode of the {@link TabListCoordinator} component.
 */
public class TabStripSnapshotter {
    private static final Set<PropertyKey> SNAPSHOT_PROPERTY_KEY_SET =
            CollectionUtil.newHashSet(
                    TabProperties.FAVICON_FETCHER,
                    TabProperties.FAVICON_FETCHED,
                    TabProperties.IS_SELECTED);

    /**
     * A token that contains an ordered list of tuples for each tab in the tab strip. Should be
     * compared against other snapshot tokens with {@link Object#equals(Object)}.
     */
    private static class TabStripSnapshotToken {
        private final int mScrollX;
        private final List<TabStripItemSnapshot> mList;

        public TabStripSnapshotToken(ModelList modelList, int scrollX) {
            mScrollX = scrollX;
            mList = new ArrayList<>(modelList.size());
            for (int i = 0; i < modelList.size(); i++) {
                ListItem listItem = modelList.get(i);
                TabStripItemSnapshot itemSnapshot = new TabStripItemSnapshot(listItem.model);
                mList.add(itemSnapshot);
            }
        }

        @Override
        public int hashCode() {
            return Objects.hash(mList, mScrollX);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof TabStripSnapshotToken)) {
                return false;
            }
            TabStripSnapshotToken other = (TabStripSnapshotToken) obj;
            if (mScrollX != other.mScrollX) {
                return false;
            }
            return mList.equals(other.mList);
        }
    }

    /** Simple tuple to hold all relevant fields for a single tab item. */
    private static class TabStripItemSnapshot {
        @Nullable public final TabListFaviconProvider.TabFaviconFetcher mTabFaviconFetcher;
        public final boolean mFaviconFetched;
        public final boolean mIsSelected;

        public TabStripItemSnapshot(PropertyModel propertyModel) {
            mTabFaviconFetcher = propertyModel.get(TabProperties.FAVICON_FETCHER);
            mFaviconFetched = propertyModel.get(TabProperties.FAVICON_FETCHED);
            mIsSelected = propertyModel.get(TabProperties.IS_SELECTED);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mTabFaviconFetcher, mFaviconFetched, mIsSelected);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof TabStripItemSnapshot)) {
                return false;
            }
            TabStripItemSnapshot other = (TabStripItemSnapshot) obj;
            return Objects.equals(mTabFaviconFetcher, other.mTabFaviconFetcher)
                    && this.mFaviconFetched == other.mFaviconFetched
                    && this.mIsSelected == other.mIsSelected;
        }
    }

    private final Callback<Object> mOnModelTokenChange;
    private final ModelList mModelList;
    private final RecyclerView mRecyclerView;
    private final OnScrollListener mOnScrollListener;
    private final ModelListPropertyChangeFilter mPropertyObserverFilter;

    /**
     * @param onModelTokenChange Where to pass the token when the snapshot is taken.
     * @param modelList The model to observe.
     * @param recyclerView The recycler view that can be scrolled.
     */
    public TabStripSnapshotter(
            @NonNull Callback<Object> onModelTokenChange,
            @NonNull ModelList modelList,
            @NonNull RecyclerView recyclerView) {
        mOnModelTokenChange = onModelTokenChange;
        mModelList = modelList;
        mRecyclerView = recyclerView;
        mOnScrollListener =
                new OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        if (newState == RecyclerView.SCROLL_STATE_IDLE) {
                            doSnapshot();
                        }
                    }
                };
        mRecyclerView.addOnScrollListener(mOnScrollListener);
        mPropertyObserverFilter =
                new ModelListPropertyChangeFilter(
                        this::doSnapshot, modelList, SNAPSHOT_PROPERTY_KEY_SET);
    }

    private void doSnapshot() {
        int scrollX = mRecyclerView.computeHorizontalScrollOffset();
        mOnModelTokenChange.onResult(new TabStripSnapshotToken(mModelList, scrollX));
    }

    public void destroy() {
        mRecyclerView.removeOnScrollListener(mOnScrollListener);
        mPropertyObserverFilter.destroy();
    }
}
