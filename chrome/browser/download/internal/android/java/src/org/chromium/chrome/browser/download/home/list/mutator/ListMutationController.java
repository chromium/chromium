// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list.mutator;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.JustNowProvider;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.list.ListItemModel;

import java.util.Date;
import java.util.List;

/**
 * Controls the list mutation pipeline which includes the sorter, label adder, paginator, and
 * property setter.
 */
public class ListMutationController {
    private final DownloadManagerUiConfig mUiConfig;
    private final ListItemModel mModel;
    private final DateOrderedListMutator mMutator;

    private final ListConsumer mDefaultDateSorter;
    private final ListConsumer mDefaultDateLabelAdder;
    private final DateOrderedListMutator.ListPaginator mDefaultListPaginator;

    private final ListConsumer mPrefetchSorter;
    private final ListConsumer mPrefetchLabelAdder;
    private final DateOrderedListMutator.ListPaginator mPrefetchListPaginator;
    private final ListConsumer mListItemPropertySetter;
    private final NoopListConsumer mNoopListConsumer;
    private final CardPaginator mCardPaginator;

    private int mFilterType = Filters.FilterType.NONE;

    private final ListConsumer mModelConsumer =
            new ListConsumer() {
                @Override
                public ListConsumer setListConsumer(ListConsumer nextConsumer) {
                    return null;
                }

                @Override
                public void onListUpdated(List<ListItem> inputList) {
                    mModel.set(inputList);
                    mModel.dispatchLastEvent();
                }
            };

    /** Constructor. */
    public ListMutationController(
            DownloadManagerUiConfig config,
            JustNowProvider justNowProvider,
            DateOrderedListMutator mutator,
            ListItemModel model) {
        mUiConfig = config;
        mMutator = mutator;
        mModel = model;

        mNoopListConsumer = new NoopListConsumer();
        mDefaultListPaginator =
                mUiConfig.showPaginationHeaders ? new DateListPaginator() : mNoopListConsumer;
        mPrefetchListPaginator =
                mUiConfig.showPaginationHeaders ? new PrefetchListPaginator() : mNoopListConsumer;
        mCardPaginator = new CardPaginator();
        mDefaultDateSorter = new DateSorter(justNowProvider);
        mDefaultDateLabelAdder = new DateLabelAdder(config, justNowProvider);
        mPrefetchSorter = new DateSorterForCards();
        mPrefetchLabelAdder =
                mUiConfig.supportsGrouping
                        ? new GroupCardLabelAdder(mCardPaginator)
                        : mNoopListConsumer;
        mListItemPropertySetter = new ListItemPropertySetter(mUiConfig);

        resetPipeline();
        mMutator.reload();
    }

    /** To be called when this mediator should filter its content based on {@code filter}. */
    public void onFilterTypeSelected(@Filters.FilterType int filter) {
        if (mDefaultListPaginator != null) mDefaultListPaginator.reset();
        if (mPrefetchListPaginator != null) mPrefetchListPaginator.reset();
        mCardPaginator.reset();

        boolean filterTypeSame =
                mFilterType == filter
                        || (mFilterType != Filters.FilterType.PREFETCHED
                                && filter != Filters.FilterType.PREFETCHED);
        if (mFilterType != -1 && filterTypeSame) return;

        mFilterType = filter;
        resetPipeline();
    }

    /** Called to add more pages to show in the list. */
    public void loadMorePages() {
        DateOrderedListMutator.ListPaginator paginator =
                mFilterType == Filters.FilterType.PREFETCHED
                        ? mPrefetchListPaginator
                        : mDefaultListPaginator;
        paginator.loadMorePages();
        mMutator.reload();
    }

    /**
     * Called to add more items in a group card.
     * @param dateAndDomain The date and domain corresponding to the card.
     */
    public void loadMoreItemsOnCard(android.util.Pair<Date, String> dateAndDomain) {
        mCardPaginator.loadMore(dateAndDomain);
        mMutator.reload();
    }

    private void resetPipeline() {
        boolean isPrefetch = mFilterType == Filters.FilterType.PREFETCHED;
        ListConsumer sorter = isPrefetch ? mPrefetchSorter : mDefaultDateSorter;
        ListConsumer labelAdder = isPrefetch ? mPrefetchLabelAdder : mDefaultDateLabelAdder;
        ListConsumer paginator = (isPrefetch ? mPrefetchListPaginator : mDefaultListPaginator);
        mMutator.setListConsumer(sorter)
                .setListConsumer(labelAdder)
                .setListConsumer(paginator)
                .setListConsumer(mListItemPropertySetter)
                .setListConsumer(mModelConsumer);
    }

    /** An empty implementation for {@link ListConsumer} and {@link ListPaginator}. */
    private static class NoopListConsumer
            implements ListConsumer, DateOrderedListMutator.ListPaginator {
        private ListConsumer mListConsumer;

        @Override
        public void onListUpdated(List<ListItem> inputList) {
            if (mListConsumer == null) return;
            mListConsumer.onListUpdated(inputList);
        }

        @Override
        public ListConsumer setListConsumer(ListConsumer nextConsumer) {
            mListConsumer = nextConsumer;
            return mListConsumer;
        }

        @Override
        public void loadMorePages() {}

        @Override
        public void reset() {}
    }
}
