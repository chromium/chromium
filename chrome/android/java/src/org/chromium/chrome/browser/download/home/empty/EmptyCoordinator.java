// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.empty;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.StringRes;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.empty.EmptyProperties.State;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/** A class that determines whether an empty view should be shown and inserts into the model. */
public class EmptyCoordinator implements OfflineItemFilterObserver, FilterCoordinator.Observer {
    private final OfflineItemFilterSource mSource;

    private final PropertyModel mModel = new PropertyModel(EmptyProperties.ALL_KEYS);
    private final EmptyView mView;

    private boolean mShowingPrefetch;
    private boolean mInSearchMode;

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

    /**
     * Method to inform the coordinator about a change in search mode.
     * @param inSearchMode Whether we are currently in active search mode.
     */
    public void setInSearchMode(boolean inSearchMode) {
        mInSearchMode = inSearchMode;
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
            @DrawableRes
            int iconId;
            if (mShowingPrefetch) {
                iconId = R.drawable.ic_library_news_feed;

                if (PrefetchConfiguration.isPrefetchingEnabled()) {
                    textId = mInSearchMode ? R.string.download_manager_prefetch_tab_no_results
                                           : R.string.download_manager_prefetch_tab_empty;
                } else {
                    textId = R.string.download_manager_enable_prefetch_message;
                }
            } else {
                iconId = R.drawable.downloads_big;
                textId = mInSearchMode ? R.string.download_manager_no_results
                                       : R.string.download_manager_ui_empty;
            }

            mModel.set(EmptyProperties.EMPTY_TEXT_RES_ID, textId);
            mModel.set(EmptyProperties.EMPTY_ICON_RES_ID, iconId);
        } else {
            state = State.GONE;
        }

        mModel.set(EmptyProperties.STATE, state);
    }
}
