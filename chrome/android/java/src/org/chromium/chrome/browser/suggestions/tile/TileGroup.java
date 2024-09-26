// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.annotation.SuppressLint;
import android.util.SparseArray;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsOfflineModelObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;

/** The model and controller for a group of site suggestion tiles. */
public class TileGroup implements MostVisitedSites.Observer {
    /**
     * Performs work in other parts of the system that the {@link TileGroup} should not know about.
     */
    public interface Delegate {
        /**
         * @param tile The tile corresponding to the most visited item to remove.
         * @param removalUndoneCallback The callback to invoke if the removal is reverted. The
         *                              callback's argument is the URL being restored.
         */
        void removeMostVisitedItem(Tile tile, Callback<GURL> removalUndoneCallback);

        void openMostVisitedItem(int windowDisposition, Tile tile);

        void openMostVisitedItemInGroup(int windowDisposition, Tile tile);

        /**
         * Gets the list of most visited sites.
         * @param observer The observer to be notified with the list of sites.
         * @param maxResults The maximum number of sites to retrieve.
         */
        void setMostVisitedSitesObserver(MostVisitedSites.Observer observer, int maxResults);

        /**
         * Called when the tile group has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
         *
         * @param tiles The tiles owned by the {@link TileGroup}. Used to record metrics.
         */
        void onLoadingComplete(List<Tile> tiles);

        /** Initialize AndroidPrerenderManager JNI interface. */
        void initAndroidPrerenderManager(AndroidPrerenderManager androidPrerenderManager);

        /**
         * To be called before this instance is abandoned to the garbage collector so it can do any
         * necessary cleanups. This instance must not be used after this method is called.
         */
        void destroy();
    }

    /** An observer for events in the {@link TileGroup}. */
    public interface Observer {
        /**
         * Called when the tile group is initialised and when any of the tile data has changed,
         * such as an icon, url, or title.
         */
        void onTileDataChanged();

        /** Called when the number of tiles has changed. */
        void onTileCountChanged();

        /**
         * Called when a tile icon has changed.
         * @param tile The tile for which the icon has changed.
         */
        void onTileIconChanged(Tile tile);

        /**
         * Called when the visibility of a tile's offline badge has changed.
         * @param tile The tile for which the visibility of the offline badge has changed.
         */
        void onTileOfflineBadgeVisibilityChanged(Tile tile);
    }

    /**
     * A delegate to allow {@link TileRenderer} to setup behaviours for the newly created views
     * associated to a Tile.
     */
    public interface TileSetupDelegate {
        /**
         * Returns a delegate that will handle user interactions with the view created for the tile.
         */
        TileInteractionDelegate createInteractionDelegate(Tile tile, View view);

        /**
         * Returns a callback to be invoked when the icon for the provided tile is loaded. It will
         * be responsible for triggering the visual refresh.
         */
        Runnable createIconLoadCallback(Tile tile);
    }

    /** Delegate for handling interactions with tiles. */
    public interface TileInteractionDelegate extends OnClickListener, OnCreateContextMenuListener {
        /**
         * Set a runnable for click events on the tile. This is primarily used to track interaction
         * with the tile used by feature engagement purposes.
         * @param clickRunnable The {@link Runnable} to be executed when tile is clicked.
         */
        void setOnClickRunnable(Runnable clickRunnable);

        /**
         * Set a runnable for remove events on the tile. Similarly to setOnClickRunnable, this is
         * primarily used to track interaction with the tile used by feature engagement purposes.
         * @param removeRunnable The {@link Runnable} to be executed when tile is removed.
         */
        void setOnRemoveRunnable(Runnable removeRunnable);
    }

    /**
     * Constants used to track the current operations on the group and notify the {@link Delegate}
     * when the expected sequence of potentially asynchronous operations is complete.
     */
    @VisibleForTesting
    @IntDef({TileTask.FETCH_DATA, TileTask.SCHEDULE_ICON_FETCH, TileTask.FETCH_ICON})
    @Retention(RetentionPolicy.SOURCE)
    @interface TileTask {
        /**
         * An event that should result in new data being loaded happened.
         * Can be an asynchronous task, spanning from when the {@link Observer} is registered to
         * when the initial load completes.
         */
        int FETCH_DATA = 1;

        /**
         * New tile data has been loaded and we are expecting the related icons to be fetched.
         * Can be an asynchronous task, as we rely on it being triggered by the embedder, some time
         * after {@link Observer#onTileDataChanged()} is called.
         */
        int SCHEDULE_ICON_FETCH = 2;

        /**
         * The icon for a tile is being fetched.
         * Asynchronous task, that is started for each icon that needs to be loaded.
         */
        int FETCH_ICON = 3;
    }

    private final SuggestionsUiDelegate mUiDelegate;
    private final ContextMenuManager mContextMenuManager;
    private final Delegate mTileGroupDelegate;
    private final Observer mObserver;
    private final TileRenderer mTileRenderer;
    // Used in TileInteractionDelegateImpl.
    private final int mPrerenderDelay;

    /**
     * Tracks the tasks currently in flight.
     *
     * <p>We only care about which ones are pending, not their order, and we can have multiple tasks
     * pending of the same type. Hence exposing the type as Collection rather than List or Set.
     */
    private final Collection<Integer> mPendingTasks = new ArrayList<>();

    /** Access point to offline related features. */
    private final OfflineModelObserver mOfflineModelObserver;

    /**
     * Source of truth for the tile data. Avoid keeping a reference to a tile in long running
     * callbacks, as it might be thrown out before it is called. Use URL or site data to look it up
     * at the right time instead.
     * @see #findTile(SiteSuggestion)
     * @see #findTilesForUrl(String)
     */
    private SparseArray<List<Tile>> mTileSections = createEmptyTileData();

    /** Most recently received tile data that has not been displayed yet. */
    @Nullable private List<SiteSuggestion> mPendingTiles;

    /**
     * URL of the most recently removed tile. Used to identify when a tile removal is confirmed by
     * the tile backend.
     */
    @Nullable private GURL mPendingRemovalUrl;

    /**
     * URL of the most recently added tile. Used to identify when a given tile's insertion is
     * confirmed by the tile backend. This is relevant when a previously existing tile is removed,
     * then the user undoes the action and wants that tile back.
     */
    @Nullable private GURL mPendingInsertionUrl;

    private boolean mHasReceivedData;

    // TODO(dgn): Attempt to avoid cycling dependencies with TileRenderer. Is there a better way?
    private final TileSetupDelegate mTileSetupDelegate =
            new TileSetupDelegate() {
                @Override
                public TileInteractionDelegate createInteractionDelegate(Tile tile, View view) {
                    return new TileInteractionDelegateImpl(tile.getData(), view);
                }

                @Override
                public Runnable createIconLoadCallback(Tile tile) {
                    // TODO(dgn): We could save on fetches by avoiding a new one when there is one
                    // pending for the same URL, and applying the result to all matched URLs.
                    boolean trackLoad =
                            isLoadTracked()
                                    && tile.getSectionType() == TileSectionType.PERSONALIZED;
                    if (trackLoad) addTask(TileTask.FETCH_ICON);
                    return () -> {
                        mObserver.onTileIconChanged(tile);
                        if (trackLoad) removeTask(TileTask.FETCH_ICON);
                    };
                }
            };

    /**
     * @param tileRenderer Used to render icons.
     * @param uiDelegate Delegate used to interact with the rest of the system.
     * @param contextMenuManager Used to handle context menu invocations on the tiles.
     * @param tileGroupDelegate Used for interactions with the Most Visited backend.
     * @param observer Will be notified of changes to the tile data.
     * @param offlinePageBridge Used to update the offline badge of the tiles.
     */
    public TileGroup(
            TileRenderer tileRenderer,
            SuggestionsUiDelegate uiDelegate,
            ContextMenuManager contextMenuManager,
            Delegate tileGroupDelegate,
            Observer observer,
            OfflinePageBridge offlinePageBridge) {
        mUiDelegate = uiDelegate;
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mObserver = observer;
        mTileRenderer = tileRenderer;
        mOfflineModelObserver = new OfflineModelObserver(offlinePageBridge);
        mUiDelegate.addDestructionObserver(mOfflineModelObserver);

        mPrerenderDelay =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2,
                        "prerender_new_tab_page_on_touch_trigger",
                        0);
    }

    @Override
    public void onSiteSuggestionsAvailable(List<SiteSuggestion> siteSuggestions) {
        // Only transforms the incoming tiles and stores them in a buffer for when we decide to
        // refresh the tiles in the UI.

        boolean removalCompleted = mPendingRemovalUrl != null;
        boolean insertionCompleted = mPendingInsertionUrl == null;

        mPendingTiles = new ArrayList<>();
        for (SiteSuggestion suggestion : siteSuggestions) {
            mPendingTiles.add(suggestion);

            // Only tiles in the personal section can be modified.
            if (suggestion.sectionType != TileSectionType.PERSONALIZED) continue;
            if (suggestion.url.equals(mPendingRemovalUrl)) removalCompleted = false;
            if (suggestion.url.equals(mPendingInsertionUrl)) insertionCompleted = true;
        }

        boolean expectedChangeCompleted = false;
        if (mPendingRemovalUrl != null && removalCompleted) {
            mPendingRemovalUrl = null;
            expectedChangeCompleted = true;
        }
        if (mPendingInsertionUrl != null && insertionCompleted) {
            mPendingInsertionUrl = null;
            expectedChangeCompleted = true;
        }

        if (!mHasReceivedData || !mUiDelegate.isVisible() || expectedChangeCompleted) loadTiles();
    }

    @Override
    public void onIconMadeAvailable(GURL siteUrl) {
        for (Tile tile : findTilesForUrl(siteUrl)) {
            mTileRenderer.updateIcon(tile, mTileSetupDelegate);
        }
    }

    /**
     * Instructs this instance to start listening for data. The {@link TileGroup.Observer} may be
     * called immediately if new data is received synchronously.
     * @param maxResults The maximum number of sites to retrieve.
     */
    public void startObserving(int maxResults) {
        addTask(TileTask.FETCH_DATA);
        mTileGroupDelegate.setMostVisitedSitesObserver(this, maxResults);
    }

    /**
     * Method to be called when a tile render has been triggered, to let the {@link TileGroup}
     * update its internal task tracking status.
     * @see Delegate#onLoadingComplete(List)
     */
    public void notifyTilesRendered() {
        // Icon fetch scheduling was done when building the tile views.
        if (isLoadTracked()) removeTask(TileTask.SCHEDULE_ICON_FETCH);
    }

    /** @return the sites currently loaded in the group, grouped by vertical. */
    public SparseArray<List<Tile>> getTileSections() {
        return mTileSections;
    }

    public boolean hasReceivedData() {
        return mHasReceivedData;
    }

    /** @return Whether the group has no sites to display. */
    public boolean isEmpty() {
        for (int i = 0; i < mTileSections.size(); i++) {
            if (!mTileSections.valueAt(i).isEmpty()) return false;
        }
        return true;
    }

    /**
     * To be called when the view displaying the tile group becomes visible.
     * @param trackLoadTask whether the delegate should be notified that the load is completed
     *      through {@link Delegate#onLoadingComplete(List)}.
     */
    public void onSwitchToForeground(boolean trackLoadTask) {
        if (trackLoadTask) addTask(TileTask.FETCH_DATA);
        if (mPendingTiles != null) loadTiles();
        if (trackLoadTask) removeTask(TileTask.FETCH_DATA);
    }

    public TileSetupDelegate getTileSetupDelegate() {
        return mTileSetupDelegate;
    }

    /** Loads tile data from {@link #mPendingTiles} and clears it afterwards. */
    private void loadTiles() {
        assert mPendingTiles != null;

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        boolean dataChanged = isInitialLoad;
        List<Tile> personalisedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        int oldPersonalisedTilesCount = personalisedTiles == null ? 0 : personalisedTiles.size();

        SparseArray<List<Tile>> newSites = createEmptyTileData();
        for (int i = 0; i < mPendingTiles.size(); ++i) {
            SiteSuggestion suggestion = mPendingTiles.get(i);
            Tile tile = findTile(suggestion);
            if (tile == null) {
                dataChanged = true;
                tile = new Tile(suggestion, i);
            }

            List<Tile> sectionTiles = newSites.get(suggestion.sectionType);
            if (sectionTiles == null) {
                sectionTiles = new ArrayList<>();
                newSites.append(suggestion.sectionType, sectionTiles);
            }

            // This is not supposed to happen but does. See https://crbug.com/703628
            if (findTile(suggestion.url, sectionTiles) != null) continue;

            sectionTiles.add(tile);
        }

        mTileSections = newSites;
        mPendingTiles = null;

        // TODO(dgn): change these events, maybe introduce new ones or just change semantics? This
        // will depend on the UI to be implemented and the desired refresh behaviour.
        List<Tile> personalizedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        int numberOfPersonalizedTiles = personalizedTiles == null ? 0 : personalizedTiles.size();
        boolean countChanged =
                isInitialLoad || numberOfPersonalizedTiles != oldPersonalisedTilesCount;
        dataChanged = dataChanged || countChanged;

        if (!dataChanged) return;

        mOfflineModelObserver.updateAllSuggestionsOfflineAvailability();

        if (countChanged) mObserver.onTileCountChanged();

        if (isLoadTracked()) addTask(TileTask.SCHEDULE_ICON_FETCH);
        mObserver.onTileDataChanged();

        if (isInitialLoad) removeTask(TileTask.FETCH_DATA);
    }

    protected @Nullable Tile findTile(SiteSuggestion suggestion) {
        if (mTileSections.get(suggestion.sectionType) == null) return null;
        for (Tile tile : mTileSections.get(suggestion.sectionType)) {
            if (tile.getData().equals(suggestion)) return tile;
        }
        return null;
    }

    /**
     * @param url The URL to search for.
     * @param tiles The section to search in, represented by the contained list of tiles.
     * @return A tile matching the provided URL and section, or {@code null} if none is found.
     */
    private Tile findTile(GURL url, @Nullable List<Tile> tiles) {
        if (tiles == null) return null;
        for (Tile tile : tiles) {
            if (tile.getUrl().equals(url)) return tile;
        }
        return null;
    }

    /** @return All tiles matching the provided URL, or an empty list if none is found. */
    private List<Tile> findTilesForUrl(GURL url) {
        List<Tile> tiles = new ArrayList<>();
        for (int i = 0; i < mTileSections.size(); ++i) {
            for (Tile tile : mTileSections.valueAt(i)) {
                if (tile.getUrl().equals(url)) tiles.add(tile);
            }
        }
        return tiles;
    }

    private void addTask(@TileTask int task) {
        mPendingTasks.add(task);
    }

    private void removeTask(@TileTask int task) {
        boolean removedTask = mPendingTasks.remove(task);
        assert removedTask;

        if (mPendingTasks.isEmpty()) {
            // TODO(dgn): We only notify about the personal tiles because that's the only ones we
            // wait for to be loaded. We also currently rely on the tile order in the returned
            // array as the reported position in UMA, but this is not accurate and would be broken
            // if we returned all the tiles regardless of sections.
            List<Tile> personalTiles = mTileSections.get(TileSectionType.PERSONALIZED);
            assert personalTiles != null;
            mTileGroupDelegate.onLoadingComplete(personalTiles);
        }
    }

    /**
     * @return Whether the current load is being tracked. Unrequested task tracking updates should
     * not be sent, as it would cause calling {@link Delegate#onLoadingComplete(List)} at the
     * wrong moment.
     */
    private boolean isLoadTracked() {
        return mPendingTasks.contains(TileTask.FETCH_DATA)
                || mPendingTasks.contains(TileTask.SCHEDULE_ICON_FETCH);
    }

    @VisibleForTesting
    boolean isTaskPending(@TileTask int task) {
        return mPendingTasks.contains(task);
    }

    public @Nullable SiteSuggestion getHomepageTileData() {
        for (Tile tile : mTileSections.get(TileSectionType.PERSONALIZED)) {
            if (tile.getSource() == TileSource.HOMEPAGE) {
                return tile.getData();
            }
        }
        return null;
    }

    private static SparseArray<List<Tile>> createEmptyTileData() {
        SparseArray<List<Tile>> newTileData = new SparseArray<>();

        // TODO(dgn): How do we want to handle empty states and sections that have no tiles?
        // Have an empty list for now that can be rendered as-is without causing issues or too much
        // state checking. We will have to decide if we want empty lists or no section at all for
        // the others.
        newTileData.put(TileSectionType.PERSONALIZED, new ArrayList<>());

        return newTileData;
    }

    /** Called before this instance is abandoned to the garbage collector. */
    public void destroy() {
        // The mOfflineModelObserver which implements SuggestionsOfflineModelObserver adds itself
        // as the offlinePageBridge's observer. Calling onDestroy() removes itself from subscribers.
        mOfflineModelObserver.onDestroy();
    }

    private class TileInteractionDelegateImpl
            implements TileInteractionDelegate, ContextMenuManager.Delegate, View.OnTouchListener {

        /**
         * CancelableRunnable is a Runnable class can be canceled. It is created here instead of
         * making CallbackController.CancelableRunnable reusable is that this class is expected to
         * be used on UI thread only, so locking mechanism is not required.
         */
        private static class CancelableRunnable implements Runnable {
            private Runnable mRunnable;

            private CancelableRunnable(@NonNull Runnable runnable) {
                mRunnable = runnable;
            }

            public void cancel() {
                mRunnable = null;
            }

            @Override
            public void run() {
                // This is run on UI thread only, so it is not necessary to guard this against
                // another thread.
                if (mRunnable != null) mRunnable.run();
            }
        }

        private final SiteSuggestion mSuggestion;
        private Runnable mOnClickRunnable;
        private Runnable mOnRemoveRunnable;
        private Long mTouchTimer;
        private AndroidPrerenderManager mAndroidPrerenderManager;
        private @Nullable CancelableRunnable mPrerenderRunnable;
        private GURL mPrerenderedUrl;
        private GURL mScheduldedPrerenderingUrl;

        private void maybeRecordTouchDuration(boolean taken) {
            if (mTouchTimer == null) return;

            long duration = TimeUtils.elapsedRealtimeMillis() - mTouchTimer;
            mTouchTimer = null;
            RecordHistogram.recordLongTimesHistogram(
                    taken
                            ? "Prerender.Experimental.NewTabPage.TouchDuration.Taken"
                            : "Prerender.Experimental.NewTabPage.TouchDuration.NotTaken",
                    duration);
        }

        public TileInteractionDelegateImpl(SiteSuggestion suggestion, View view) {
            mSuggestion = suggestion;
            view.setOnTouchListener(TileInteractionDelegateImpl.this);
            mAndroidPrerenderManager = AndroidPrerenderManager.getAndroidPrerenderManager();
            mTileGroupDelegate.initAndroidPrerenderManager(mAndroidPrerenderManager);
        }

        @Override
        public void onClick(View view) {
            maybeRecordTouchDuration(true);
            if (mSuggestion == null) return;

            Tile tile = findTile(mSuggestion);
            if (tile == null) return;

            SuggestionsMetrics.recordTileTapped();
            if (mOnClickRunnable != null) mOnClickRunnable.run();
            mTileGroupDelegate.openMostVisitedItem(WindowOpenDisposition.CURRENT_TAB, tile);
        }

        private void maybePrerender(GURL url) {
            if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2)) {
                return;
            }

            // Avoid resetting the delayed task if witness several MotionEvent.ACTION_DOWN in a
            // row. If the URL has been scheduled to be prerendered or already prerendered, it
            // should skipped.
            if (Objects.equals(mScheduldedPrerenderingUrl, url)
                    || Objects.equals(mPrerenderedUrl, url)) return;

            assert mScheduldedPrerenderingUrl == null;
            mScheduldedPrerenderingUrl = url;
            mPrerenderRunnable =
                    new CancelableRunnable(
                            () -> {
                                if (mAndroidPrerenderManager.startPrerendering(url)) {
                                    mPrerenderedUrl = url;
                                }
                                mScheduldedPrerenderingUrl = null;
                            });
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mPrerenderRunnable, mPrerenderDelay);
        }

        // This function cancels scheduled prerendering or calls stopPrerendering to stop stale
        // prerendering.
        private void cancelPrerender() {
            if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2)) {
                return;
            }

            if (mPrerenderRunnable != null) {
                mPrerenderRunnable.cancel();
                mPrerenderRunnable = null;
            }

            if (mPrerenderedUrl != null) {
                mAndroidPrerenderManager.stopPrerendering();
            }

            mPrerenderedUrl = null;
            mScheduldedPrerenderingUrl = null;
        }

        @Override
        @SuppressLint("ClickableViewAccessibility")
        public boolean onTouch(View view, MotionEvent event) {
            if (event.getAction() == MotionEvent.ACTION_DOWN) {
                mTouchTimer = TimeUtils.elapsedRealtimeMillis();

                if (mSuggestion == null) return false;
                Tile tile = findTile(mSuggestion);
                // Avoid prerendering the tile if it is search related, since parameters are not
                // handled for prerendering cases. This will cause problems for default search
                // engines.
                // TODO(crbug.com/40282403): Move the logic to `PrerenderManager` if the issue
                // is fixed by the check.
                if (tile == null || mTileRenderer.isSearchTile(tile)) return false;
                maybePrerender(tile.getUrl());
            }
            if (event.getAction() == MotionEvent.ACTION_CANCEL) {
                maybeRecordTouchDuration(false);
                cancelPrerender();
            }

            return false;
        }

        @Override
        public void openItem(int windowDisposition) {
            Tile tile = findTile(mSuggestion);
            if (tile == null) return;

            mTileGroupDelegate.openMostVisitedItem(windowDisposition, tile);
        }

        @Override
        public void openItemInGroup(int windowDisposition) {
            Tile tile = findTile(mSuggestion);
            if (tile == null) return;

            mTileGroupDelegate.openMostVisitedItemInGroup(windowDisposition, tile);
        }

        @Override
        public void removeItem() {
            Tile tile = findTile(mSuggestion);
            if (tile == null) return;

            if (mOnRemoveRunnable != null) mOnRemoveRunnable.run();

            // Note: This does not track all the removals, but will track the most recent one. If
            // that removal is committed, it's good enough for change detection.
            mPendingRemovalUrl = mSuggestion.url;
            mTileGroupDelegate.removeMostVisitedItem(tile, url -> mPendingInsertionUrl = url);
        }

        @Override
        public GURL getUrl() {
            return mSuggestion.url;
        }

        @Override
        public String getContextMenuTitle() {
            return null;
        }

        @Override
        public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
            switch (menuItemId) {
                    // Personalized tiles are the only tiles that can be removed.
                case ContextMenuItemId.REMOVE:
                    return mSuggestion.sectionType == TileSectionType.PERSONALIZED;
                default:
                    return true;
            }
        }

        @Override
        public void onContextMenuCreated() {}

        @Override
        public void onCreateContextMenu(
                ContextMenu contextMenu, View view, ContextMenuInfo contextMenuInfo) {
            mContextMenuManager.createContextMenu(contextMenu, view, this);
        }

        @Override
        public void setOnClickRunnable(Runnable clickRunnable) {
            mOnClickRunnable = clickRunnable;
        }

        @Override
        public void setOnRemoveRunnable(Runnable removeRunnable) {
            mOnRemoveRunnable = removeRunnable;
        }
    }

    private class OfflineModelObserver extends SuggestionsOfflineModelObserver<Tile> {
        public OfflineModelObserver(OfflinePageBridge bridge) {
            super(bridge);
        }

        @Override
        public void onSuggestionOfflineIdChanged(Tile tile, OfflinePageItem item) {
            boolean oldOfflineAvailable = tile.isOfflineAvailable();
            tile.setOfflinePageOfflineId(item == null ? null : item.getOfflineId());

            // Only notify to update the view if there will be a visible change.
            if (oldOfflineAvailable == tile.isOfflineAvailable()) return;
            mObserver.onTileOfflineBadgeVisibilityChanged(tile);
        }

        @Override
        public Iterable<Tile> getOfflinableSuggestions() {
            List<Tile> tiles = new ArrayList<>();
            for (int i = 0; i < mTileSections.size(); ++i) tiles.addAll(mTileSections.valueAt(i));
            return tiles;
        }
    }
}
