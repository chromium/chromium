// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.support.annotation.IntDef;
import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.chips.ChipsCoordinator;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A Coordinator responsible for showing the tab filter selection UI for downloads home. */
public class FilterCoordinator {
    @IntDef({TabType.FILES, TabType.PREFETCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabType {
        int FILES = 0;
        int PREFETCH = 1;
    }

    /** An Observer to notify when the selected tab has changed. */
    public interface Observer {
        /** Called when the selected tab has changed. */
        void onFilterChanged(@FilterType int selectedTab);
    }

    private final ObserverList<Observer> mObserverList = new ObserverList<>();
    private final PropertyModel mModel = new PropertyModel(FilterProperties.ALL_KEYS);
    private final FilterView mView;

    private final ChipsCoordinator mChipsCoordinator;
    private final FilterChipsProvider mChipsProvider;

    /**
     * Builds a new FilterCoordinator.
     * @param context The context to build the views and pull parameters from.
     */
    public FilterCoordinator(Context context, OfflineItemFilterSource chipFilterSource) {
        mChipsProvider = new FilterChipsProvider(type -> handleChipSelected(), chipFilterSource);
        mChipsCoordinator = new ChipsCoordinator(context, mChipsProvider);

        mView = new FilterView(context);
        PropertyModelChangeProcessor.create(mModel, mView, new FilterViewBinder());

        mModel.set(FilterProperties.CHANGE_LISTENER, this::handleTabSelected);
        selectTab(TabType.FILES);

        mModel.set(FilterProperties.SHOW_TABS, PrefetchConfiguration.isPrefetchingFlagEnabled());
    }

    /** @return The {@link View} representing this widget. */
    public View getView() {
        return mView.getView();
    }

    /** Registers {@code observer} to be notified of tab selection changes. */
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    /** Unregisters {@code observer} from tab selection changes. */
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    /**
     * Pushes a selected filter onto this {@link FilterCoordinator}.  This is used when external
     * components might need to update the UI state.
     */
    public void setSelectedFilter(@FilterType int filter) {
        if (filter == Filters.FilterType.PREFETCHED
                && PrefetchConfiguration.isPrefetchingFlagEnabled()) {
            selectTab(TabType.PREFETCH);
        } else {
            mChipsProvider.setFilterSelected(filter);
            selectTab(TabType.FILES);
        }
    }

    private void selectTab(@TabType int selectedTab) {
        mModel.set(FilterProperties.SELECTED_TAB, selectedTab);

        if (selectedTab == TabType.FILES) {
            mModel.set(FilterProperties.CONTENT_VIEW, mChipsCoordinator.getView());
        } else if (selectedTab == TabType.PREFETCH) {
            mModel.set(FilterProperties.CONTENT_VIEW, null);
        }
    }

    private void handleTabSelected(@TabType int selectedTab) {
        selectTab(selectedTab);

        @FilterType
        int filterType;
        if (selectedTab == TabType.FILES) {
            filterType = mChipsProvider.getSelectedFilter();
        } else {
            filterType = Filters.FilterType.PREFETCHED;
        }

        notifyFilterChanged(filterType);
    }

    private void notifyFilterChanged(@FilterType int filter) {
        for (Observer observer : mObserverList) observer.onFilterChanged(filter);
    }

    private void handleChipSelected() {
        handleTabSelected(mModel.get(FilterProperties.SELECTED_TAB));
    }
}