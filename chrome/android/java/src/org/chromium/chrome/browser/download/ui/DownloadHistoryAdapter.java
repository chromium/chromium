// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.ComponentName;
import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.chrome.browser.download.DownloadSharedPreferenceHelper;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.metrics.FileExtensions;
import org.chromium.chrome.browser.download.home.storage.StorageSummaryProvider;
import org.chromium.chrome.browser.download.ui.BackendProvider.DownloadDelegate;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.DownloadItemWrapper;
import org.chromium.chrome.browser.download.ui.DownloadHistoryItemWrapper.OfflineItemWrapper;
import org.chromium.chrome.browser.widget.DateDividedAdapter;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.download.R;
import org.chromium.components.download.DownloadState;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.variations.VariationsAssociatedData;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collection;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Bridges the user's download history and the UI used to display it. */
public class DownloadHistoryAdapter
        extends DateDividedAdapter implements DownloadSharedPreferenceHelper.Observer,
                                              OfflineContentProvider.Observer, DownloadObserver {
    private static final String TAG = "DownloadAdapter";

    /** Alerted about changes to internal state. */
    static interface TestObserver {
        abstract void onDownloadItemCreated(DownloadItem item);
        abstract void onDownloadItemUpdated(DownloadItem item);
        abstract void onOfflineItemCreated(OfflineItem item);
        abstract void onOfflineItemUpdated(OfflineItem item);
    }

    private class BackendItemsImpl extends BackendItems {
        @Override
        public DownloadHistoryItemWrapper removeItem(String guid) {
            DownloadHistoryItemWrapper wrapper = super.removeItem(guid);

            if (wrapper != null) {
                mFilePathsToItemsMap.removeItem(wrapper);
                if (getSelectionDelegate().isItemSelected(wrapper)) {
                    getSelectionDelegate().toggleSelectionForItem(wrapper);
                }
            }

            return wrapper;
        }
    }

    /** Represents the subsection header of the suggested pages for a given date. */
    protected static class SubsectionHeader extends TimedItem {
        private List<DownloadHistoryItemWrapper> mSubsectionItems;
        private long mTotalFileSize;
        private long mLatestUpdateTime;
        private final Long mStableId;
        private boolean mIsExpanded;
        private boolean mShouldShowRecentBadge;

        public SubsectionHeader() {
            // Generate a stable ID based on timestamp.
            mStableId = 0xFFFFFFFF00000000L + (new Date().getTime() & 0x0FFFFFFFF);
        }

        @Override
        public long getTimestamp() {
            return mLatestUpdateTime;
        }

        /**
         * Returns all the items associated with the subsection irrespective of whether it is
         * expanded or collapsed.
         */
        public List<DownloadHistoryItemWrapper> getItems() {
            return mSubsectionItems;
        }

        public int getItemCount() {
            return mSubsectionItems.size();
        }

        public long getTotalFileSize() {
            return mTotalFileSize;
        }

        @Override
        public long getStableId() {
            return mStableId;
        }

        /** @return Whether the subsection is currently expanded. */
        public boolean isExpanded() {
            return mIsExpanded;
        }

        /** @param isExpanded Whether the subsection is currently expanded. */
        public void setIsExpanded(boolean isExpanded) {
            mIsExpanded = isExpanded;
        }

        /** @return Whether the NEW badge should be shown. */
        public boolean shouldShowRecentBadge() {
            return mShouldShowRecentBadge;
        }

        /** @param show Whether the NEW badge should be shown. */
        public void setShouldShowRecentBadge(boolean show) {
            mShouldShowRecentBadge = show;
        }

        /**
         * Helper method to set the items for this subsection.
         * @param subsectionItems The items associated with this subsection.
         */
        public void update(List<DownloadHistoryItemWrapper> subsectionItems) {
            mSubsectionItems = subsectionItems;
            mTotalFileSize = 0;
            for (DownloadHistoryItemWrapper item : subsectionItems) {
                mTotalFileSize += item.getFileSize();
                mLatestUpdateTime = Math.max(mLatestUpdateTime, item.getTimestamp());
            }
        }
    }

    /** An item group containing the prefetched items. */
    private static class PrefetchItemGroup extends ItemGroup {
        @Override
        public @GroupPriority int priority() {
            return GroupPriority.ELEVATED_CONTENT;
        }

        @Override
        public @ItemViewType int getItemViewType(int index) {
            return index == 0 ? ItemViewType.SUBSECTION_HEADER : ItemViewType.NORMAL;
        }

        @Override
        protected int compareItem(TimedItem lhs, TimedItem rhs) {
            if (lhs instanceof SubsectionHeader) return -1;
            if (rhs instanceof SubsectionHeader) return 1;

            return super.compareItem(lhs, rhs);
        }
    }

    /**
     * Tracks externally deleted items that have been removed from downloads history.
     * Shared across instances.
     */
    private static final DeletedFileTracker sDeletedFileTracker = new DeletedFileTracker();

    private static final String EMPTY_QUERY = null;

    private static final String PREF_SHOW_STORAGE_INFO_HEADER =
            "download_home_show_storage_info_header";
    public static final String PREF_PREFETCH_BUNDLE_LAST_VISITED_TIME =
            "download_home_prefetch_bundle_last_visited_time";
    private static final String VARIATION_TRIAL_DOWNLOAD_HOME_PREFETCH_UI =
            "DownloadHomePrefetchUI";
    private static final String VARIATION_PARAM_TIME_THRESHOLD_FOR_RECENT_BADGE =
            "recent_badge_time_threshold_hours";
    private static final int DEFAULT_TIME_THRESHOLD_FOR_RECENT_BADGE_HOURS = 48;

    private final BackendItems mRegularDownloadItems = new BackendItemsImpl();
    private final BackendItems mIncognitoDownloadItems = new BackendItemsImpl();
    private final BackendItems mOfflineItems = new BackendItemsImpl();

    private final FilePathsToDownloadItemsMap mFilePathsToItemsMap =
            new FilePathsToDownloadItemsMap();

    private SubsectionHeader mPrefetchHeader;
    private final ComponentName mParentComponent;
    private final boolean mShowOffTheRecord;
    private final LoadingStateDelegate mLoadingDelegate;
    private final ObserverList<TestObserver> mObservers = new ObserverList<>();
    private final List<DownloadItemView> mViews = new ArrayList<>();

    private BackendProvider mBackendProvider;
    private @DownloadFilter.Type int mFilter = DownloadFilter.Type.ALL;
    private String mSearchQuery = EMPTY_QUERY;
    // TODO(xingliu): Remove deprecated storage info. See https://crbug/853260.
    private SpaceDisplay mSpaceDisplay;
    private StorageSummaryProvider mStorageSummaryProvider;
    private HeaderItem mSpaceDisplayHeaderItem;
    private HeaderItem mStorageSummaryHeaderItem;
    private boolean mIsSearching;
    private boolean mShouldShowStorageInfoHeader;
    private boolean mShouldPrefetchSectionExpand;
    private long mPrefetchBundleLastVisitedTime;

    // Should only be accessed through getRecentBadgeTimeThreshold().
    private Integer mTimeThresholdForRecentBadgeMs;

    @Nullable // This may be null during tests.
    private UiConfig mUiConfig;

    DownloadHistoryAdapter(boolean showOffTheRecord, ComponentName parentComponent) {
        mShowOffTheRecord = showOffTheRecord;
        mParentComponent = parentComponent;
        mLoadingDelegate = new LoadingStateDelegate(mShowOffTheRecord);

        // Using stable IDs allows the RecyclerView to animate changes.
        setHasStableIds(true);
    }

    /**
     * Initializes the adapter.
     * @param provider The {@link BackendProvider} that provides classes needed by the adapter.
     * @param uiConfig The UiConfig used to observe display style changes.
     */
    public void initialize(BackendProvider provider, @Nullable UiConfig uiConfig) {
        mBackendProvider = provider;
        mUiConfig = uiConfig;

        generateHeaderItems();

        DownloadItemSelectionDelegate selectionDelegate =
                (DownloadItemSelectionDelegate) mBackendProvider.getSelectionDelegate();
        selectionDelegate.initialize(this);

        // Get all regular and (if necessary) off the record downloads.
        DownloadDelegate downloadManager = getDownloadDelegate();
        downloadManager.addDownloadObserver(this);
        downloadManager.getAllDownloads(false);
        if (mShowOffTheRecord) downloadManager.getAllDownloads(true);

        // Fetch all Offline Items from OfflineContentProvider (Pages, Background Fetches etc).
        getAllOfflineItems();
        getOfflineContentProvider().addObserver(this);

        sDeletedFileTracker.incrementInstanceCount();
        mShouldShowStorageInfoHeader = ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_SHOW_STORAGE_INFO_HEADER,
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOAD_HOME_SHOW_STORAGE_INFO));
        mPrefetchBundleLastVisitedTime = ContextUtils.getAppSharedPreferences().getLong(
                PREF_PREFETCH_BUNDLE_LAST_VISITED_TIME, new Date(0L).getTime());
    }

    private OfflineContentProvider getOfflineContentProvider() {
        return mBackendProvider.getOfflineContentProvider();
    }

    @Override
    public void onAllDownloadsRetrieved(List<DownloadItem> result, boolean isOffTheRecord) {
        if (isOffTheRecord && !mShowOffTheRecord) return;

        BackendItems list = getDownloadItemList(isOffTheRecord);
        if (list.isInitialized()) return;
        assert list.size() == 0;

        int[] itemCounts = new int[DownloadFilter.Type.NUM_ENTRIES];
        int[] viewedItemCounts = new int[DownloadFilter.Type.NUM_ENTRIES];

        for (DownloadItem item : result) {
            DownloadItemWrapper wrapper = createDownloadItemWrapper(item);
            if (addDownloadHistoryItemWrapper(wrapper)
                    && wrapper.isVisibleToUser(DownloadFilter.Type.ALL)) {
                itemCounts[wrapper.getFilterType()]++;

                if (DownloadUtils.isDownloadViewed(wrapper.getItem()))
                    viewedItemCounts[wrapper.getFilterType()]++;
                if (!isOffTheRecord && wrapper.getFilterType() == DownloadFilter.Type.OTHER) {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Android.DownloadManager.OtherExtensions.InitialCount",
                            wrapper.getFileExtensionType(), FileExtensions.Type.NUM_ENTRIES);
                }
            }
        }

        if (!isOffTheRecord) recordDownloadCountHistograms(itemCounts, viewedItemCounts);

        list.setIsInitialized();
        onItemsRetrieved(isOffTheRecord
                ? LoadingStateDelegate.INCOGNITO_DOWNLOADS
                : LoadingStateDelegate.REGULAR_DOWNLOADS);
    }

    /**
     * Checks if a wrapper corresponds to an item that was already deleted.
     * @return True if it does, false otherwise.
     */
    private boolean updateDeletedFileMap(DownloadHistoryItemWrapper wrapper) {
        // TODO(twellington): The native downloads service should remove externally deleted
        //                    downloads rather than passing them to Java.
        if (sDeletedFileTracker.contains(wrapper)) return true;

        if (!wrapper.hasBeenExternallyRemoved()) return false;

        if (DownloadUtils.isInPrimaryStorageDownloadDirectory(wrapper.getFilePath())) {
            sDeletedFileTracker.add(wrapper);
            wrapper.removePermanently();
            mFilePathsToItemsMap.removeItem(wrapper);
            RecordUserAction.record("Android.DownloadManager.Item.ExternallyDeleted");
            return true;
        } else {
            // Keeps the download record when the file is on external SD card.
            RecordUserAction.record("Android.DownloadManager.Item.ExternallyDeletedKeepRecord");
            return false;
        }
    }

    private boolean addDownloadHistoryItemWrapper(DownloadHistoryItemWrapper wrapper) {
        if (updateDeletedFileMap(wrapper)) return false;

        getListForItem(wrapper).add(wrapper);
        mFilePathsToItemsMap.addItem(wrapper);
        return true;
    }

    /**
     * Should be called when download items or offline pages have been retrieved.
     */
    private void onItemsRetrieved(int type) {
        if (mLoadingDelegate.updateLoadingState(type)) {
            recordTotalDownloadCountHistogram();
            filter(mLoadingDelegate.getPendingFilter());
        }
    }

    /** Returns the total size of all non-deleted downloaded items. */
    public long getTotalDownloadSize() {
        long totalSize = 0;
        totalSize += mRegularDownloadItems.getTotalBytes();
        totalSize += mIncognitoDownloadItems.getTotalBytes();
        totalSize += mOfflineItems.getTotalBytes();
        return totalSize;
    }

    /** Returns a collection of {@link SubsectionHeader}s. */
    public Collection<SubsectionHeader> getSubsectionHeaders() {
        List<SubsectionHeader> headers = new ArrayList<>();
        if (mPrefetchHeader != null) headers.add(mPrefetchHeader);
        return headers;
    }

    @Override
    protected int getTimedItemViewResId() {
        return R.layout.date_view;
    }

    @Override
    protected SubsectionHeaderViewHolder createSubsectionHeader(ViewGroup parent) {
        OfflineGroupHeaderView offlineHeader =
                (OfflineGroupHeaderView) LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.offline_download_header, parent, false);
        offlineHeader.setAdapter(this);
        offlineHeader.setSelectionDelegate((DownloadItemSelectionDelegate) getSelectionDelegate());
        return new SubsectionHeaderViewHolder(offlineHeader);
    }

    @Override
    protected void bindViewHolderForSubsectionHeader(
            SubsectionHeaderViewHolder holder, TimedItem timedItem) {
        SubsectionHeader headerItem = (SubsectionHeader) timedItem;
        OfflineGroupHeaderView headerView = (OfflineGroupHeaderView) holder.getView();
        headerView.displayHeader(headerItem);
    }

    @Override
    public ViewHolder createViewHolder(ViewGroup parent) {
        DownloadItemView v = (DownloadItemView) LayoutInflater.from(parent.getContext()).inflate(
                R.layout.download_item_view, parent, false);
        v.setSelectionDelegate(getSelectionDelegate());
        mViews.add(v);
        return new DownloadHistoryItemViewHolder(v);
    }

    @Override
    public void bindViewHolderForTimedItem(ViewHolder current, TimedItem timedItem) {
        final DownloadHistoryItemWrapper item = (DownloadHistoryItemWrapper) timedItem;

        DownloadHistoryItemViewHolder holder = (DownloadHistoryItemViewHolder) current;
        holder.getItemView().displayItem(mBackendProvider, item);
    }

    @Override
    protected void bindViewHolderForHeaderItem(ViewHolder viewHolder, HeaderItem headerItem) {
        super.bindViewHolderForHeaderItem(viewHolder, headerItem);
        updateStorageSummary();
    }

    /**
     * Initialize space display view in storage info header and generate header item for it.
     */
    private void generateHeaderItems() {
        mSpaceDisplay = new SpaceDisplay(null, this);
        View view = mSpaceDisplay.getViewContainer();
        registerAdapterDataObserver(mSpaceDisplay);
        mSpaceDisplayHeaderItem = new HeaderItem(0, view);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)) {
            Context context = ContextUtils.getApplicationContext();
            View storageSummaryView =
                    LayoutInflater.from(context).inflate(R.layout.download_storage_summary, null);
            mStorageSummaryProvider =
                    new StorageSummaryProvider(context, this ::updateStorageInfo, null);
            mStorageSummaryHeaderItem = new HeaderItem(0, storageSummaryView);
        }
    }

    private void updateStorageInfo(String storageInfo) {
        TextView storageSummaryView = (TextView) mStorageSummaryHeaderItem.getView();
        storageSummaryView.setText(storageInfo);
    }

    /** Called when a new DownloadItem has been created by the native DownloadManager. */
    @Override
    public void onDownloadItemCreated(DownloadItem item) {
        boolean isOffTheRecord = item.getDownloadInfo().isOffTheRecord();
        if (isOffTheRecord && !mShowOffTheRecord) return;

        BackendItems list = getDownloadItemList(isOffTheRecord);
        assert list.findItemIndex(item.getId()) == BackendItems.INVALID_INDEX;

        DownloadItemWrapper wrapper = createDownloadItemWrapper(item);
        boolean wasAdded = addDownloadHistoryItemWrapper(wrapper);
        if (wasAdded && wrapper.isVisibleToUser(mFilter)) filter(mFilter);

        for (TestObserver observer : mObservers) observer.onDownloadItemCreated(item);
    }

    /** Updates the list when new information about a download comes in. */
    @Override
    public void onDownloadItemUpdated(DownloadItem item) {
        DownloadItemWrapper newWrapper = createDownloadItemWrapper(item);
        if (newWrapper.isOffTheRecord() && !mShowOffTheRecord) return;

        // Check if the item has already been deleted.
        if (updateDeletedFileMap(newWrapper)) return;

        BackendItems list = getListForItem(newWrapper);
        int index = list.findItemIndex(item.getId());
        if (index == BackendItems.INVALID_INDEX) {
            assert false : "Tried to update DownloadItem that didn't exist.";
            return;
        }

        // Update the old one.
        DownloadHistoryItemWrapper existingWrapper = list.get(index);
        boolean isUpdated = existingWrapper.replaceItem(item);

        // Re-add the file mapping once it finishes downloading. This accounts for the backend
        // creating DownloadItems with a null file path, then updating it after the download starts.
        // Doing it once after completion instead of at every update is a compromise that prevents
        // us from rapidly and repeatedly updating the map with the same info.
        if (item.getDownloadInfo().state() == DownloadState.COMPLETE) {
            mFilePathsToItemsMap.addItem(existingWrapper);
        }

        if (item.getDownloadInfo().state() == DownloadState.CANCELLED) {
            // The old one is being removed.
            filter(mFilter);
        } else if (existingWrapper.isVisibleToUser(mFilter)) {
            if (existingWrapper.getPosition() == TimedItem.INVALID_POSITION) {
                filter(mFilter);
                for (TestObserver observer : mObservers) observer.onDownloadItemUpdated(item);
            } else if (isUpdated) {
                // Directly alert DownloadItemViews displaying information about the item that it
                // has changed instead of notifying the RecyclerView that a particular item has
                // changed.  This prevents the RecyclerView from detaching and immediately
                // reattaching the same view, causing janky animations.
                for (DownloadItemView view : mViews) {
                    DownloadHistoryItemWrapper wrapper = view.getItem();
                    if (wrapper == null) {
                        // TODO(qinmin): remove this once crbug.com/731789 is fixed.
                        Log.e(TAG, "DownloadItemView contains empty DownloadHistoryItemWrapper");
                        continue;
                    }
                    if (TextUtils.equals(item.getId(), wrapper.getId())) {
                        view.displayItem(mBackendProvider, existingWrapper);
                        if (item.getDownloadInfo().state() == DownloadState.COMPLETE) {
                            updateStorageSummary();
                        }
                    }
                }

                for (TestObserver observer : mObservers) observer.onDownloadItemUpdated(item);
            }
        }
    }

    /**
     * Removes the DownloadItem with the given ID.
     * @param guid           ID of the DownloadItem that has been removed.
     * @param isOffTheRecord True if off the record, false otherwise.
     */
    @Override
    public void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {
        if (isOffTheRecord && !mShowOffTheRecord) return;
        if (getDownloadItemList(isOffTheRecord).removeItem(guid) != null) {
            filter(mFilter);
        }
    }

    /** Called when the filter representing which items can show has changed. */
    public void onFilterChanged(@DownloadFilter.Type int filter) {
        if (mLoadingDelegate.isLoaded()) {
            filter(filter);
        } else {
            // Wait until all the backends are fully loaded before trying to show anything.
            mLoadingDelegate.setPendingFilter(filter);
        }
    }

    /** Called when this object should be destroyed. */
    public void destroy() {
        getDownloadDelegate().removeDownloadObserver(this);
        getOfflineContentProvider().removeObserver(this);
        sDeletedFileTracker.decrementInstanceCount();
        if (mSpaceDisplay != null) unregisterAdapterDataObserver(mSpaceDisplay);
    }

    @Override
    public void onAddOrReplaceDownloadSharedPreferenceEntry(final ContentId id) {
        // Alert DownloadItemViews displaying information about the item that it has changed.
        for (DownloadItemView view : mViews) {
            if (view.getItem() == null) continue;
            if (TextUtils.equals(id.id, view.getItem().getId())) {
                view.displayItem(mBackendProvider, view.getItem());
            }
        }
    }

    /** Marks that certain items are about to be deleted. */
    void markItemsForDeletion(List<DownloadHistoryItemWrapper> items) {
        for (DownloadHistoryItemWrapper item : items) item.setIsDeletionPending(true);
        filter(mFilter);
    }

    /** Marks that items that were about to be deleted are not being deleted anymore. */
    void unmarkItemsForDeletion(List<DownloadHistoryItemWrapper> items) {
        for (DownloadHistoryItemWrapper item : items) item.setIsDeletionPending(false);
        filter(mFilter);
    }

    /**
     * Gets all DownloadHistoryItemWrappers that point to the same path in the user's storage.
     * @param filePath The file path used to retrieve items.
     * @return DownloadHistoryItemWrappers associated with filePath.
     */
    Set<DownloadHistoryItemWrapper> getItemsForFilePath(String filePath) {
        return mFilePathsToItemsMap.getItemsForFilePath(filePath);
    }

    /** Registers a {@link TestObserver} to monitor internal changes. */
    void registerObserverForTest(TestObserver observer) {
        mObservers.addObserver(observer);
    }

    /** Unregisters a {@link TestObserver} that was monitoring internal changes. */
    void unregisterObserverForTest(TestObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Called to perform a search. If the query is empty all items matching the current filter will
     * be displayed.
     * @param query The text to search for.
     */
    void search(String query) {
        mIsSearching = true;
        mSearchQuery = query;
        filter(mFilter);
    }

    /**
     * Called when a search is ended.
     */
    void onEndSearch() {
        mIsSearching = false;
        mSearchQuery = EMPTY_QUERY;
        filter(mFilter);
    }

    /** @return Whether the storage info header should be visible. */
    boolean shouldShowStorageInfoHeader() {
        return mShouldShowStorageInfoHeader;
    }

    /**
     * Sets the visibility of the storage info header and saves user selection to shared preference.
     * @param show Whether or not we should show the storage info header.
     */
    void setShowStorageInfoHeader(boolean show) {
        mShouldShowStorageInfoHeader = show;
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PREF_SHOW_STORAGE_INFO_HEADER, mShouldShowStorageInfoHeader)
                .apply();
        RecordHistogram.recordBooleanHistogram(
                "Android.DownloadManager.ShowStorageInfo", mShouldShowStorageInfoHeader);
        if (mLoadingDelegate.isLoaded()) filter(mFilter);
    }

    private DownloadDelegate getDownloadDelegate() {
        return mBackendProvider.getDownloadDelegate();
    }

    private SelectionDelegate<DownloadHistoryItemWrapper> getSelectionDelegate() {
        return mBackendProvider.getSelectionDelegate();
    }

    private boolean matchesQuery(DownloadHistoryItemWrapper item, String query) {
        if (TextUtils.isEmpty(query)) return true;

        query = query.toLowerCase(Locale.getDefault());
        Locale locale = Locale.getDefault();

        return item.getDisplayHostname().toLowerCase(locale).contains(query)
                || item.getDisplayFileName().toLowerCase(locale).contains(query);
    }

    /** Filters the list of downloads to show only files of a specific type. */
    private void filter(@DownloadFilter.Type int filterType) {
        mFilter = filterType;

        List<TimedItem> filteredTimedItems = new ArrayList<>();
        mRegularDownloadItems.filter(mFilter, mSearchQuery, filteredTimedItems);
        mIncognitoDownloadItems.filter(mFilter, mSearchQuery, filteredTimedItems);

        List<DownloadHistoryItemWrapper> prefetchedItems = new ArrayList<>();
        filter(mFilter, mSearchQuery, mOfflineItems, filteredTimedItems, prefetchedItems);

        clear(false);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)) {
            setHeaders(mStorageSummaryHeaderItem);
        } else if (!filteredTimedItems.isEmpty() && !mIsSearching && mShouldShowStorageInfoHeader) {
            setHeaders(mSpaceDisplayHeaderItem);
        }

        createPrefetchedItemGroup(prefetchedItems);
        loadItems(filteredTimedItems);
    }

    private void createPrefetchedItemGroup(List<DownloadHistoryItemWrapper> prefetchedItems) {
        if (prefetchedItems.isEmpty()) return;
        if (!TextUtils.isEmpty(mSearchQuery)) return;

        if (mPrefetchHeader == null) mPrefetchHeader = new SubsectionHeader();
        mPrefetchHeader.setIsExpanded(mShouldPrefetchSectionExpand);
        mPrefetchHeader.update(prefetchedItems);

        ItemGroup prefetchItemGroup = new PrefetchItemGroup();
        prefetchItemGroup.addItem(mPrefetchHeader);
        if (mPrefetchHeader.isExpanded()) {
            for (DownloadHistoryItemWrapper item : prefetchedItems) {
                prefetchItemGroup.addItem(item);
            }
        }

        addGroup(prefetchItemGroup);
        updateRecentBadges(prefetchedItems);
    }

    private void updateRecentBadges(List<DownloadHistoryItemWrapper> prefetchedItems) {
        boolean showBadgeForHeader = false;
        for (DownloadHistoryItemWrapper item : prefetchedItems) {
            item.setShouldShowRecentBadge(shouldItemShowRecentBadge(item));
            showBadgeForHeader |= shouldItemShowRecentBadge(item);
        }

        mPrefetchHeader.setShouldShowRecentBadge(showBadgeForHeader);
    }

    private boolean shouldItemShowRecentBadge(DownloadHistoryItemWrapper item) {
        return item.getTimestamp() > mPrefetchBundleLastVisitedTime;
    }

    /**
     * Filters the list based on the current filter and search text.
     * If there are suggested pages, they are filtered based on whether or not the prefetch section
     * is expanded. While doing a search, we don't show the prefetch header, but show the items
     * nevertheless.
     * @param filterType The filter to use.
     * @param query The search text to match.
     * @param inputList The input item list.
     * @param filteredItems The output item list (append-only) for the normal section.
     * @param suggestedItems The output item list for the prefetch section.
     */
    private void filter(int filterType, String query, List<DownloadHistoryItemWrapper> inputList,
            List<TimedItem> filteredItems, List<DownloadHistoryItemWrapper> suggestedItems) {
        boolean shouldShowSubsectionHeaders = TextUtils.isEmpty(mSearchQuery);

        for (DownloadHistoryItemWrapper item : inputList) {
            if (!item.isVisibleToUser(filterType)) continue;
            if (!matchesQuery(item, query)) continue;

            if (shouldShowSubsectionHeaders && item.isSuggested()) {
                suggestedItems.add(item);
            } else {
                filteredItems.add(item);
            }
        }
    }

    /**
     * Sets the state of the prefetch section and updates the adapter.
     * @param expanded Whether the prefetched section should be expanded.
     */
    public void setPrefetchSectionExpanded(boolean expanded) {
        if (mShouldPrefetchSectionExpand == expanded) return;
        mShouldPrefetchSectionExpand = expanded;

        updatePrefetchBundleLastVisitedTime();
        clear(false);
        filter(mFilter);
    }

    private void updatePrefetchBundleLastVisitedTime() {
        // We don't care about marking recent for items updated more than 48 hours ago.
        Calendar calendar = Calendar.getInstance();
        calendar.add(Calendar.HOUR_OF_DAY, -getRecentBadgeTimeThreshold());
        mPrefetchBundleLastVisitedTime = ContextUtils.getAppSharedPreferences().getLong(
                PREF_PREFETCH_BUNDLE_LAST_VISITED_TIME, calendar.getTime().getTime());

        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(PREF_PREFETCH_BUNDLE_LAST_VISITED_TIME, new Date().getTime())
                .apply();
    }

    private BackendItems getDownloadItemList(boolean isOffTheRecord) {
        return isOffTheRecord ? mIncognitoDownloadItems : mRegularDownloadItems;
    }

    private BackendItems getListForItem(DownloadHistoryItemWrapper wrapper) {
        if (wrapper instanceof DownloadItemWrapper) {
            return getDownloadItemList(wrapper.isOffTheRecord());
        } else {
            return mOfflineItems;
        }
    }

    private DownloadItemWrapper createDownloadItemWrapper(DownloadItem item) {
        return new DownloadItemWrapper(item, mBackendProvider, mParentComponent);
    }

    private void recordDownloadCountHistograms(int[] itemCounts, int[] viewedItemCounts) {
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Audio",
                itemCounts[DownloadFilter.Type.AUDIO]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Document",
                itemCounts[DownloadFilter.Type.DOCUMENT]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Image",
                itemCounts[DownloadFilter.Type.IMAGE]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Other",
                itemCounts[DownloadFilter.Type.OTHER]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Video",
                itemCounts[DownloadFilter.Type.VIDEO]);

        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed.Audio",
                viewedItemCounts[DownloadFilter.Type.AUDIO]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed.Document",
                viewedItemCounts[DownloadFilter.Type.DOCUMENT]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed.Image",
                viewedItemCounts[DownloadFilter.Type.IMAGE]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed.Other",
                viewedItemCounts[DownloadFilter.Type.OTHER]);
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Viewed.Video",
                viewedItemCounts[DownloadFilter.Type.VIDEO]);
    }

    private void recordTotalDownloadCountHistogram() {
        // The total count intentionally leaves out incognito downloads. This should be revisited
        // if/when incognito downloads are persistently available in downloads home.
        RecordHistogram.recordCountHistogram("Android.DownloadManager.InitialCount.Total",
                mRegularDownloadItems.size() + mOfflineItems.size());
    }

    /** Returns the {@link SpaceDisplay}. */
    public SpaceDisplay getSpaceDisplayForTests() {
        return mSpaceDisplay;
    }

    private void getAllOfflineItems() {
        getOfflineContentProvider().getAllItems(offlineItems -> {
            for (OfflineItem item : offlineItems) {
                if (item.isTransient) continue;
                DownloadHistoryItemWrapper wrapper = createDownloadHistoryItemWrapper(item);
                addDownloadHistoryItemWrapper(wrapper);
            }

            recordOfflineItemCountHistograms();
            onItemsRetrieved(LoadingStateDelegate.OFFLINE_ITEMS);
        });
    }

    private void recordOfflineItemCountHistograms() {
        int offlinePageCount = 0;
        int viewedOfflinePageCount = 0;
        int prefetchedOfflinePageCount = 0;
        int viewedPrefetchedOfflinePageCount = 0;

        for (DownloadHistoryItemWrapper itemWrapper : mOfflineItems) {
            if (itemWrapper.isOffTheRecord()) continue;
            OfflineItemWrapper offlineItemWrapper = (OfflineItemWrapper) itemWrapper;
            boolean hasBeenViewed = DownloadUtils.isOfflineItemViewed(offlineItemWrapper.getItem());
            if (offlineItemWrapper.isSuggested()) {
                prefetchedOfflinePageCount++;
                if (hasBeenViewed) viewedPrefetchedOfflinePageCount++;
            } else {
                offlinePageCount++;
                if (hasBeenViewed) viewedOfflinePageCount++;
            }
        }

        RecordHistogram.recordCountHistogram(
                "Android.DownloadManager.InitialCount.OfflinePage", offlinePageCount);
        RecordHistogram.recordCountHistogram(
                "Android.DownloadManager.InitialCount.Viewed.OfflinePage", viewedOfflinePageCount);
        RecordHistogram.recordCountHistogram(
                "Android.DownloadManager.InitialCount.PrefetchedOfflinePage",
                prefetchedOfflinePageCount);
        RecordHistogram.recordCountHistogram(
                "Android.DownloadManager.InitialCount.Viewed.PrefetchedOfflinePage",
                viewedPrefetchedOfflinePageCount);
    }

    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        boolean wasAdded = false;
        boolean visible = false;
        for (OfflineItem item : items) {
            if (item.isTransient) continue;

            assert mOfflineItems.findItemIndex(item.id.id) == BackendItems.INVALID_INDEX;

            DownloadHistoryItemWrapper wrapper = createDownloadHistoryItemWrapper(item);
            wasAdded |= addDownloadHistoryItemWrapper(wrapper);
            visible |= wrapper.isVisibleToUser(mFilter);
            for (TestObserver observer : mObservers) observer.onOfflineItemCreated(item);
        }

        if (wasAdded && visible) filter(mFilter);
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (mOfflineItems.removeItem(id.id) != null) {
            filter(mFilter);
        }
    }

    @Override
    public void onItemUpdated(OfflineItem item) {
        if (item.isTransient) return;

        DownloadHistoryItemWrapper newWrapper = createDownloadHistoryItemWrapper(item);
        if (newWrapper.isOffTheRecord() && !mShowOffTheRecord) return;

        // Check if the item has already been deleted.
        if (updateDeletedFileMap(newWrapper)) return;

        BackendItems list = mOfflineItems;
        int index = list.findItemIndex(newWrapper.getId());
        if (index == BackendItems.INVALID_INDEX) {
            // TODO(shaktisahu) : Remove this after crbug/765348 is fixed.
            Log.e(TAG, "Tried to update OfflineItem that didn't exist, id: " + item.id);
            return;
        }

        // Update the old one.
        DownloadHistoryItemWrapper existingWrapper = list.get(index);
        boolean isUpdated = existingWrapper.replaceItem(item);

        // Re-add the file mapping once it finishes downloading. This accounts for the backend
        // creating DownloadItems with a null file path, then updating it after the download starts.
        // Doing it once after completion instead of at every update is a compromise that prevents
        // us from rapidly and repeatedly updating the map with the same info.
        if (item.state == OfflineItemState.COMPLETE) {
            mFilePathsToItemsMap.addItem(existingWrapper);
        }

        if (item.state == OfflineItemState.CANCELLED) {
            // The old one is being removed.
            filter(mFilter);
        } else if (existingWrapper.isVisibleToUser(mFilter)) {
            if (existingWrapper.getPosition() == TimedItem.INVALID_POSITION) {
                filter(mFilter);
                for (TestObserver observer : mObservers) observer.onOfflineItemUpdated(item);
            } else if (isUpdated) {
                // Directly alert DownloadItemViews displaying information about the item that it
                // has changed instead of notifying the RecyclerView that a particular item has
                // changed.  This prevents the RecyclerView from detaching and immediately
                // reattaching the same view, causing janky animations.
                for (DownloadItemView view : mViews) {
                    if (TextUtils.equals(item.id.id, view.getItem().getId())) {
                        view.displayItem(mBackendProvider, existingWrapper);
                        if (item.state == OfflineItemState.COMPLETE) {
                            updateStorageSummary();
                        }
                    }
                }

                for (TestObserver observer : mObservers) observer.onOfflineItemUpdated(item);
            }
        }
    }

    private DownloadHistoryItemWrapper createDownloadHistoryItemWrapper(OfflineItem item) {
        return new OfflineItemWrapper(item, mBackendProvider, mParentComponent);
    }

    private int getRecentBadgeTimeThreshold() {
        if (mTimeThresholdForRecentBadgeMs == null) {
            mTimeThresholdForRecentBadgeMs = DEFAULT_TIME_THRESHOLD_FOR_RECENT_BADGE_HOURS;

            String variationResult = VariationsAssociatedData.getVariationParamValue(
                    VARIATION_TRIAL_DOWNLOAD_HOME_PREFETCH_UI,
                    VARIATION_PARAM_TIME_THRESHOLD_FOR_RECENT_BADGE);
            if (!TextUtils.isEmpty(variationResult)) {
                mTimeThresholdForRecentBadgeMs = Integer.parseInt(variationResult);
            }
        }

        return mTimeThresholdForRecentBadgeMs;
    }

    private void updateStorageSummary() {
        if (mSpaceDisplay != null) mSpaceDisplay.onChanged();
        if (mStorageSummaryProvider != null) {
            mStorageSummaryProvider.setUsedStorage(getTotalDownloadSize());
        }
    }
}
