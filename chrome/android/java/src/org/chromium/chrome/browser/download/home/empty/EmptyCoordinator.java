// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.empty;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.empty.EmptyProperties.State;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Collection;

/** A class that determines whether an empty view should be shown and inserts into the model. */
public class EmptyCoordinator implements OfflineItemFilterObserver, FilterCoordinator.Observer {
    private final OfflineItemFilterSource mSource;

    private final PropertyModel mModel = new PropertyModel(EmptyProperties.ALL_KEYS);
    private final EmptyView mView;

    private boolean mShowingPrefetch;

    /** Creates a {@link EmptyCoordinator} instance that monitors {@code source}. */
    public EmptyCoordinator(Context context, OfflineItemFilterSource source) {
        mSource = source;

        mSource.addObserver(this);

        mView = new EmptyView(context);
        PropertyModelChangeProcessor.create(mModel, mView, new EmptyViewBinder());

        calculateState();
    }

    /** @return The {@link View} that represents the empty screen. */
    public View getView() {
        return mView.getView();
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        calculateState();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        calculateState();
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {}

    @Override
    public void onItemsAvailable() {
        calculateState();
    }

    // FilterCoordinator.Observer implementation.
    @Override
    public void onFilterChanged(@FilterType int selectedTab) {
        mShowingPrefetch = selectedTab == FilterType.PREFETCHED;
        calculateState();
    }

    private void calculateState() {
        @State
        int state;
        if (!mSource.areItemsAvailable()) {
            state = State.LOADING;
        } else if (mSource.getItems().isEmpty()) {
            state = State.EMPTY;

            @StringRes
            int textId;
            if (mShowingPrefetch) {
                textId = R.string.download_manager_prefetch_tab_empty;
            } else {
                textId = R.string.download_manager_no_downloads;
            }

            mModel.set(EmptyProperties.EMPTY_TEXT_RES_ID, textId);
        } else {
            state = State.GONE;
        }

        mModel.set(EmptyProperties.STATE, state);
    }
}
