// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.ScrollView;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.FaviconProvider;
import org.chromium.chrome.browser.download.home.StableIds;
import org.chromium.chrome.browser.download.home.empty.EmptyCoordinator;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.chrome.browser.download.home.rename.RenameDialogManager;
import org.chromium.chrome.browser.download.home.storage.StorageCoordinator;
import org.chromium.chrome.browser.download.home.toolbar.ToolbarCoordinator;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.ui.modaldialog.ModalDialogManager;

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

    private static final String TAG = "DownloadHome";
    private final Context mContext;
    private final StorageCoordinator mStorageCoordinator;
    private final FilterCoordinator mFilterCoordinator;
    private final EmptyCoordinator mEmptyCoordinator;
    private final DateOrderedListMediator mMediator;
    private final DateOrderedListView mListView;
    private final RenameDialogManager mRenameDialogManager;
    private ViewGroup mMainView;
    private View mEmptyView;
    private int mWindowHeight;
    private int mDownloadStorageSummaryHeightPx;
    private int mSelectableListToolbarHeightPx;

    /**
     * Creates an instance of a DateOrderedListCoordinator, which will visually represent
     * {@code provider} as a list of items.
     * @param context                   The {@link Context} to use to build the views.
     * @param config                    The {@link DownloadManagerUiConfig} to provide UI
     *                                  configuration params.
     * @param exploreOfflineTabVisiblitySupplier A supplier that indicates whether or not explore
     *         offline tab should be shown.
     * @param provider                  The {@link OfflineContentProvider} to visually represent.
     * @param deleteController          A class to manage whether or not items can be deleted.
     * @param filterObserver            A {@link FilterCoordinator.Observer} that should be notified
     *                                  of filter changes.  This is meant to be used for external
     *                                  components that need to take action based on the visual
     *                                  state of the list.
     * @param dateOrderedListObserver   A {@link DateOrderedListObserver}.
     * @param discardableReferencePool  A {@linK DiscardableReferencePool} reference to use for
     *                                  large objects (e.g. bitmaps) in the UI.
     */
    public DateOrderedListCoordinator(
            Context context,
            DownloadManagerUiConfig config,
            Supplier<Boolean> exploreOfflineTabVisibilitySupplier,
            OfflineContentProvider provider,
            DeleteController deleteController,
            SelectionDelegate<ListItem> selectionDelegate,
            FilterCoordinator.Observer filterObserver,
            DateOrderedListObserver dateOrderedListObserver,
            ModalDialogManager modalDialogManager,
            FaviconProvider faviconProvider,
            DiscardableReferencePool discardableReferencePool) {
        mContext = context;

        ListItemModel model = new ListItemModel();
        DecoratedListItemModel decoratedModel = new DecoratedListItemModel(model);
        mListView =
                new DateOrderedListView(
                        context,
                        config,
                        decoratedModel,
                        dateOrderedListObserver,
                        this::onConfigurationChangedCallback);
        mRenameDialogManager = new RenameDialogManager(context, modalDialogManager);
        mMediator =
                new DateOrderedListMediator(
                        provider,
                        faviconProvider,
                        this::startShareIntent,
                        deleteController,
                        this::startRename,
                        selectionDelegate,
                        config,
                        dateOrderedListObserver,
                        model,
                        discardableReferencePool);

        mEmptyCoordinator = new EmptyCoordinator(context, mMediator.getEmptySource());

        mStorageCoordinator = new StorageCoordinator(context, mMediator.getFilterSource());

        mFilterCoordinator =
                new FilterCoordinator(
                        context, mMediator.getFilterSource(), exploreOfflineTabVisibilitySupplier);
        mFilterCoordinator.addObserver(mMediator::onFilterTypeSelected);
        mFilterCoordinator.addObserver(filterObserver);
        mFilterCoordinator.addObserver(mEmptyCoordinator);

        decoratedModel.addHeader(
                new ViewListItem(StableIds.STORAGE_HEADER, mStorageCoordinator.getView()));
        decoratedModel.addHeader(
                new ViewListItem(StableIds.FILTERS_HEADER, mFilterCoordinator.getView()));
        mWindowHeight = getWindowHeight();

        mDownloadStorageSummaryHeightPx =
                (int)
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.download_storage_summary_height);
        mSelectableListToolbarHeightPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.selectable_list_toolbar_height);
        initializeView(context);
    }

    protected void onConfigurationChangedCallback() {
        mWindowHeight = getWindowHeight();

        // Update empty view margin when configuration changes.
        addMarginOnConfigurationChanged();
    }

    private int getWindowHeight() {
        return DimensionCompat.create((Activity) mContext, null).getWindowHeight();
    }

    /**
     * We need to apply mDownloadStorageSummaryHeightPx as top margin to ensure empty view don't
     * scroll above download storage summary, and add the same offset in the bottom margin to
     * center the empty view. But we shouldn't apply bottom margin to avoid empty view text get
     * cut off in small screen(when window height is smaller than maxEmptyHeight).
     */
    private void addMarginOnConfigurationChanged() {
        if (mEmptyView == null || mEmptyView.findViewById(R.id.empty_state_container) == null) {
            return;
        }
        ViewGroup emptyScrollView = mEmptyView.findViewById(R.id.empty_state_container);
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mEmptyView.getLayoutParams();
        mWindowHeight = getWindowHeight();

        // Adding margin to make sure empty view is centered from top of toolbar and not overlap
        // with download storage when screen size become small.
        int topMargin = mDownloadStorageSummaryHeightPx;
        int bottomMargin = mDownloadStorageSummaryHeightPx;

        // Height from the toolbar to the bottom of empty view.
        int maxEmptyHeight =
                ((ScrollView) emptyScrollView).getChildAt(0).getHeight()
                        + mSelectableListToolbarHeightPx
                        + bottomMargin
                        + topMargin;

        if (mWindowHeight <= maxEmptyHeight) {
            layoutParams.setMargins(0, topMargin, 0, 0);
            layoutParams.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        } else {
            layoutParams.setMargins(0, topMargin, 0, bottomMargin);
            layoutParams.gravity = Gravity.CENTER;
        }
        mEmptyView.setLayoutParams(layoutParams);
    }

    /**
     * Creates a top-level view containing the {@link DateOrderedListView} and {@link EmptyView}.
     * The list view is added on top of the empty view so that the empty view will show up when the
     * list has no items or is loading.
     * @param context The current context.
     */
    private void initializeView(Context context) {
        mMainView = new FrameLayout(context);
        mEmptyView = mEmptyCoordinator.getView();
        FrameLayout.LayoutParams emptyViewParams;
        emptyViewParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT);
        emptyViewParams.gravity = Gravity.CENTER;

        // Handle empty view position in the first run.
        mEmptyView
                .getViewTreeObserver()
                .addOnGlobalLayoutListener(
                        new ViewTreeObserver.OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                // Add margin depends on screen orientation.
                                addMarginOnConfigurationChanged();

                                // remove onGlobalLayout listener.
                                mEmptyView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                            }
                        });
        mMainView.addView(mEmptyView, emptyViewParams);

        FrameLayout.LayoutParams listParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT);
        mMainView.addView(mListView.getView(), listParams);

        // Bring to front to make empty view scrollable.
        mEmptyView.bringToFront();
    }

    /** Tears down this coordinator. */
    public void destroy() {
        mFilterCoordinator.destroy();
        mMediator.destroy();
        mRenameDialogManager.destroy();
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
        return mMediator.onBackPressed();
    }

    /**
     * @return A list of {@link BackPressHandler}, which supports predictive back press.
     */
    public BackPressHandler getBackPressHandler() {
        return mMediator;
    }

    @Override
    public void setSearchQuery(String query) {
        mMediator.onFilterStringChanged(query);
    }

    /** Sets the UI and list to filter based on the {@code filter} {@link FilterType}. */
    public void setSelectedFilter(@FilterType int filter) {
        mFilterCoordinator.setSelectedFilter(filter);
    }

    /** @return The currently selected filter. */
    public @FilterType int getSelectedFilter() {
        return mFilterCoordinator.getSelectedFilter();
    }

    private void startShareIntent(Intent intent) {
        try {
            mContext.startActivity(
                    Intent.createChooser(
                            intent, mContext.getString(R.string.share_link_chooser_title)));
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find activity for sharing");
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity for sharing, exception: " + e);
        }
    }

    private void startRename(String name, DateOrderedListMediator.RenameCallback callback) {
        mRenameDialogManager.startRename(name, callback::tryToRename);
    }
}
