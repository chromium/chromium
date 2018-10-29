// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.StableIds;
import org.chromium.chrome.browser.download.home.empty.EmptyCoordinator;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.download.home.metrics.FilterChangeLogger;
import org.chromium.chrome.browser.download.home.storage.StorageCoordinator;
import org.chromium.chrome.browser.download.home.toolbar.ToolbarCoordinator;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.List;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
public class DateOrderedListCoordinator implements ToolbarCoordinator.ToolbarListActionDelegate {
    /**
     * A helper interface for exposing the decision for whether or not to delete
     * {@link OfflineItem}s to an external layer.
     */
    @FunctionalInterface
    public interface DeleteController {
        /**
         * Will be called whenever {@link OfflineItem}s are in the process of being removed from the
         * UI.  This method will be called to determine if that removal should actually happen.
         * Based on the result passed to {@code callback}, the removal might be reverted instead of
         * being committed.  It is expected that {@code callback} will always be triggered no matter
         * what happens to the controller itself.
         *
         * @param items    The list of {@link OfflineItem}s that were explicitly slated for removal.
         * @param callback The {@link Callback} to notify when the deletion decision is finalized.
         *                 The callback value represents whether or not the deletion should occur.
         */
        void canDelete(List<OfflineItem> items, Callback<Boolean> callback);
    }

    /**
     * An observer to be notified about certain changes about the recycler view and the underlying
     * list.
     */
    public interface DateOrderedListObserver {
        /**
         * Called after a scroll operation on the view.
         * @param canScrollUp Whether the scroll position can scroll vertically further up.
         */
        void onListScroll(boolean canScrollUp);

        /**
         * Called when the empty state of the list has changed.
         * @param isEmpty Whether the list is now empty.
         */
        void onEmptyStateChanged(boolean isEmpty);
    }

    private final Context mContext;
    private final StorageCoordinator mStorageCoordinator;
    private final FilterCoordinator mFilterCoordinator;
    private final EmptyCoordinator mEmptyCoordinator;
    private final DateOrderedListMediator mMediator;
    private final DateOrderedListView mListView;
    private ViewGroup mMainView;

    /**
     * Creates an instance of a DateOrderedListCoordinator, which will visually represent
     * {@code provider} as a list of items.
     * @param context The {@link Context} to use to build the views.
     * @param config The {@link DownloadManagerUiConfig} to provide UI configuration params.
     * @param provider The {@link OfflineContentProvider} to visually represent.
     * @param deleteController A class to manage whether or not items can be deleted.
     * @param filterObserver A {@link FilterCoordinator.Observer} that should be notified of
     *                       filter changes.  This is meant to be used for external components that
     *                       need to take action based on the visual state of the list.
     * @param dateOrderedListObserver A {@link DateOrderedListObserver}.
     */
    public DateOrderedListCoordinator(Context context, DownloadManagerUiConfig config,
            OfflineContentProvider provider, DeleteController deleteController,
            SelectionDelegate<ListItem> selectionDelegate,
            FilterCoordinator.Observer filterObserver,
            DateOrderedListObserver dateOrderedListObserver) {
        mContext = context;

        ListItemModel model = new ListItemModel();
        DecoratedListItemModel decoratedModel = new DecoratedListItemModel(model);
        mListView =
                new DateOrderedListView(context, config, decoratedModel, dateOrderedListObserver);
        mMediator = new DateOrderedListMediator(provider, this ::startShareIntent, deleteController,
                selectionDelegate, config, dateOrderedListObserver, model);

        mEmptyCoordinator = new EmptyCoordinator(context, mMediator.getEmptySource());

        mStorageCoordinator = new StorageCoordinator(context, mMediator.getFilterSource());

        mFilterCoordinator = new FilterCoordinator(context, mMediator.getFilterSource());
        mFilterCoordinator.addObserver(mMediator::onFilterTypeSelected);
        mFilterCoordinator.addObserver(filterObserver);
        mFilterCoordinator.addObserver(mEmptyCoordinator);
        mFilterCoordinator.addObserver(new FilterChangeLogger());

        decoratedModel.addHeader(
                new ViewListItem(StableIds.STORAGE_HEADER, mStorageCoordinator.getView()));
        decoratedModel.addHeader(
                new ViewListItem(StableIds.FILTERS_HEADER, mFilterCoordinator.getView()));
        initializeView(context);
    }

    /**
     * Creates a top-level view containing the {@link DateOrderedListView} and {@link EmptyView}.
     * The list view is added on top of the empty view so that the empty view will show up when the
     * list has no items or is loading.
     * @param context The current context.
     */
    private void initializeView(Context context) {
        mMainView = new FrameLayout(context);
        FrameLayout.LayoutParams emptyViewParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        emptyViewParams.gravity = Gravity.CENTER;
        mMainView.addView(mEmptyCoordinator.getView(), emptyViewParams);

        FrameLayout.LayoutParams listParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        mMainView.addView(mListView.getView(), listParams);
    }

    /** Tears down this coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    /** @return The {@link View} representing downloads home. */
    public View getView() {
        return mMainView;
    }

    // ToolbarListActionDelegate implementation.
    @Override
    public int deleteSelectedItems() {
        return mMediator.deleteSelectedItems();
    }

    @Override
    public int shareSelectedItems() {
        return mMediator.shareSelectedItems();
    }

    /** Called to handle a back press event. */
    public boolean handleBackPressed() {
        return mMediator.handleBackPressed();
    }

    @Override
    public void setSearchQuery(String query) {
        mEmptyCoordinator.setInSearchMode(!TextUtils.isEmpty(query));
        mMediator.onFilterStringChanged(query);
    }

    /** Sets the UI and list to filter based on the {@code filter} {@link FilterType}. */
    public void setSelectedFilter(@FilterType int filter) {
        mFilterCoordinator.setSelectedFilter(filter);
    }

    private void startShareIntent(Intent intent) {
        mContext.startActivity(Intent.createChooser(
                intent, mContext.getString(R.string.share_link_chooser_title)));
    }
}
