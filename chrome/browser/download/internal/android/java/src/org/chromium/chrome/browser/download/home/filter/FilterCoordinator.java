// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    private final ObservableSupplier<Boolean> mIsPrefetchEnabledSupplier;

    private final Callback<Boolean> mIsPrefetchEnabledObserver = this::onPrefetchEnabledChanged;
    /**
     * Builds a new FilterCoordinator.
     * @param context The context to build the views and pull parameters from.
     * @param chipFilterSource The list of OfflineItems to use to generate the set of available
     *         filters.
     * @param isPrefetchEnabledSupplier A supplier that indicates whether or not prefetch is
     *         enabled.
     */
    public FilterCoordinator(Context context, OfflineItemFilterSource chipFilterSource,
            ObservableSupplier<Boolean> isPrefetchEnabledSupplier) {
        mChipsProvider =
                new FilterChipsProvider(context, type -> handleChipSelected(), chipFilterSource);
        mChipsCoordinator = new ChipsCoordinator(context, mChipsProvider);
        mIsPrefetchEnabledSupplier = isPrefetchEnabledSupplier;

        mView = new FilterView(context);
        PropertyModelChangeProcessor.create(mModel, mView, new FilterViewBinder());

        mModel.set(FilterProperties.CHANGE_LISTENER, this::handleTabSelected);
        selectTab(TabType.FILES);

        mModel.set(FilterProperties.SHOW_TABS, mIsPrefetchEnabledSupplier.get());

        mIsPrefetchEnabledSupplier.addObserver(mIsPrefetchEnabledObserver);
    }

    /** Tears down this coordinator. */
    public void destroy() {
        mIsPrefetchEnabledSupplier.removeObserver(mIsPrefetchEnabledObserver);
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
        @TabType
        int tabSelected;
        if (filter == Filters.FilterType.PREFETCHED && mIsPrefetchEnabledSupplier.get()) {
            tabSelected = TabType.PREFETCH;
        } else {
            mChipsProvider.setFilterSelected(filter);
            tabSelected = TabType.FILES;
        }

        handleTabSelected(tabSelected);
    }

    /** @return The currently selected filter. */
    public @FilterType int getSelectedFilter() {
        if (mModel.get(FilterProperties.SELECTED_TAB) == TabType.PREFETCH) {
            return FilterType.PREFETCHED;
        } else {
            return mChipsProvider.getSelectedFilter();
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

    private void onPrefetchEnabledChanged(Boolean enabled) {
        mModel.set(FilterProperties.SHOW_TABS, enabled);
        int selectedTab = mModel.get(FilterProperties.SELECTED_TAB);
        if (!enabled) selectedTab = TabType.FILES;
        handleTabSelected(selectedTab);
    }
}
