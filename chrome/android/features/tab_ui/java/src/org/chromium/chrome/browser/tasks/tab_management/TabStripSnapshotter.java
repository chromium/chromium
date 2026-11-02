// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListPropertyChangeFilter;
import org.chromium.ui.modelutil.PropertyKey;

import java.util.Objects;
import java.util.Set;

/**
 * Observes a {@link ModelList} and returns tokens for conceptual snapshots of the model state. The
 * opaque token can be compared against other tokens with {@link Object#equals(Object)}. The purpose
 * of these tokens is to decide when the bottom toolbar should take a bitmap capture in preparation
 * for browser controls being scrolled off the screen. This class only watches properties that
 * directly effect the steady state of the view, and thus it implicitly tightly coupled with the
 * TabListMode.BOTTOM_STRIP mode of the {@link TabListCoordinator} component.
 */
@NullMarked
public class TabStripSnapshotter {
    private static final Set<PropertyKey> SNAPSHOT_PROPERTY_KEY_SET =
            Set.of(
                    TabProperties.FAVICON_FETCHER,
                    TabProperties.FAVICON_FETCHED,
                    TabProperties.IS_SELECTED);

    /**
     * A token that contains the model version and the scroll offset. Should be compared against
     * other snapshot tokens with {@link Object#equals(Object)}.
     */
    private static class TabStripSnapshotToken {
        private final int mModelVersion;
        private final int mScrollX;

        /* package */ TabStripSnapshotToken(int modelVersion, int scrollX) {
            mModelVersion = modelVersion;
            mScrollX = scrollX;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mModelVersion, mScrollX);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return (obj instanceof TabStripSnapshotToken other)
                    && mModelVersion == other.mModelVersion
                    && mScrollX == other.mScrollX;
        }
    }

    private final Callback<Object> mOnModelTokenChange;
    private final RecyclerView mRecyclerView;
    private final OnScrollListener mOnScrollListener;
    private final ModelListPropertyChangeFilter mPropertyObserverFilter;
    private int mModelVersion;

    /**
     * @param onModelTokenChange Where to pass the token when the snapshot is taken.
     * @param modelList The model to observe.
     * @param recyclerView The recycler view that can be scrolled.
     */
    public TabStripSnapshotter(
            Callback<Object> onModelTokenChange, ModelList modelList, RecyclerView recyclerView) {
        mOnModelTokenChange = onModelTokenChange;
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
                        this::onModelChanged, modelList, SNAPSHOT_PROPERTY_KEY_SET);
    }

    private void onModelChanged() {
        // Bump the version instead of copying every item's state, which was O(n) per change.
        mModelVersion++;
        doSnapshot();
    }

    private void doSnapshot() {
        int scrollX = mRecyclerView.computeHorizontalScrollOffset();
        mOnModelTokenChange.onResult(new TabStripSnapshotToken(mModelVersion, scrollX));
    }

    /** Cleans up when this class is no longer needed. */
    public void destroy() {
        mRecyclerView.removeOnScrollListener(mOnScrollListener);
        mPropertyObserverFilter.destroy();
    }
}
