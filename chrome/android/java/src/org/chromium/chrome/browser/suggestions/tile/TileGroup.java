// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsOfflineModelObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.CustomLinkOperations;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditCoordinator;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;

/** The model and controller for a group of site suggestion tiles. */
public class TileGroup implements MostVisitedSites.Observer {

    /**
     * onSiteSuggestionsAvailable() is asynchronously called from two sources:
     *
     * <p>1. Backend updates, e.g., triggered by changes from other NTP.
     *
     * <p>2. Tile UI updates originating from the current NTP, involving API calls to {@link
     * Delegate} or {@link CustomTileModificationDelegate}.
     *
     * <p>For (2), we'd like to pass data from pre-API calls to onSiteSuggestionsAvailable() and
     * direct downstream flow (e.g., to call loadTiles()). One way to do this is to pass to the API
     * and then received by onSiteSuggestionsAvailable() -- but the current code does not do this.
     *
     * <p>Instead, the "trans-API" data are stored as fields. This is simple but somewhat sloppy.
     * This class marshalls these trans-API fields in one place. This also simplifies access from
     * {@link Delegate} or {@link CustomTileModificationDelegate}.
     */
    public static class PendingChanges {
        /** Most recently received tile data that has not been displayed yet. */
        public @Nullable List<SiteSuggestion> tiles;

        /**
         * URL of the most recently removed tile. Used to identify when a tile removal is confirmed
         * by the tile backend.
         */
        public @Nullable GURL removalUrl;

        /**
         * URL of the most recently added tile. Used to identify when a given tile's insertion is
         * confirmed by the tile backend. This is relevant when a previously existing tile is
         * removed, then the user undoes the action and wants that tile back.
         */
        public @Nullable GURL insertionUrl;

        /** Flag to indicate that Custom Tiles are being changed. */
        public boolean customTilesIndicator;

        /** List of tasks to run after tiles are reloaded and re-rendered. */
        public final LinkedList<Runnable> taskToRunAfterTileReload = new LinkedList<Runnable>();
    }

    /**
     * Performs work in other parts of the system that the {@link TileGroup} should not know about.
     */
    public interface Delegate extends CustomLinkOperations {

        /** Setter to pass object to provide feedback to onSiteSuggestionsAvailable(). */
        void setPendingChanges(PendingChanges pendingChanges);

        /**
         * @param tile The tile corresponding to the most visited item to remove.
         */
        void removeMostVisitedItem(Tile tile);

        void openMostVisitedItem(int windowDisposition, Tile tile);

        void openMostVisitedItemInGroup(int windowDisposition, Tile tile);

        /**
         * Gets the list of most visited sites.
         *
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
         * @param originalTile The tile to edit, or null to add a new tile.
         * @return A new CustomTileEditCoordinator instance.
         */
        CustomTileEditCoordinator createCustomTileEditCoordinator(@Nullable Tile originalTile);

        /**
         * Displays the snackbar to inform the user that a tile was unpinned and to offer the
         * opportunity to undo the operation.
         */
        void showTileUnpinSnackbar(Runnable undoHandler);

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
         *
         * @param tile The tile for which the icon has changed.
         */
        void onTileIconChanged(Tile tile);

        /**
         * Called when the visibility of a tile's offline badge has changed.
         *
         * @param tile The tile for which the visibility of the offline badge has changed.
         */
        void onTileOfflineBadgeVisibilityChanged(Tile tile);

        /**
         * Called when a Custom Tile is created.
         *
         * @param tile The Custom Tile that was created.
         */
        void onCustomTileCreation(Tile tile);
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

        /** Returns a delegate to handle Custom Tile modifications. */
        CustomTileModificationDelegate getCustomTileModificationDelegate();

        /**
         * Returns a callback to be invoked when the icon for the provided tile is loaded. It will
         * be responsible for triggering the visual refresh.
         */
        Runnable createIconLoadCallback(Tile tile);
    }

    /** Delegate for handling interactions with tiles. */
    public interface TileInteractionDelegate
            extends View.OnClickListener,
                    View.OnCreateContextMenuListener,
                    View.OnLongClickListener,
                    View.OnTouchListener {
        /**
         * Set a runnable for click events on the tile. This is primarily used to track interaction
         * with the tile used by feature engagement purposes.
         *
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

    /** Delegate for receive intermediate events and final results of tile drag. */
    public interface TileDragHandlerDelegate {
        /**
         * Called when the tile drag session becomes the dominant UI mode. The implementation should
         * suppress competing UI, e.g., context menu.
         */
        void onDragDominate();

        /**
         * Called when drag UI successfully produces result. The implementation should perform
         * reorder and refresh UI if successful.
         *
         * @param fromSuggestion Data to identify the tile being dragged.
         * @param toSuggestion Data to identify the tile being dropped on.
         * @return Whether the operation successfully ran.
         */
        boolean onDragAccept(SiteSuggestion fromSuggestion, SiteSuggestion toSuggestion);
    }

    /** Delegate for tile drag UI. */
    public interface TileDragDelegate {
        /**
         * Handler for ACTION_DOWN touch event on tile. This may start a tile drag session.
         *
         * @param view The View of the tile receiving ACTION_DOWN.
         * @param event The ACTION_DOWN event.
         * @param dragHandlerDelegate Handler for drag results.
         */
        void onTileTouchDown(
                View view, MotionEvent event, TileDragHandlerDelegate dragHandlerDelegate);

        /**
         * Handler for non-ACTION_DOWN events to continue / end a tile drag session. Should be
         * called if a tile drag session is live.
         */
        void onSessionTileTouch(View view, MotionEvent event);

        /**
         * @return Whether a tile drag session is live, requiring onSessionTileTouch() to be called.
         */
        boolean hasSession();

        /** Forces tile drag session to end. */
        void reset();
    }

    /** Delegate for handling interactions with custom tiles. Not tied to a particular Tile. */
    public interface CustomTileModificationDelegate {
        /**
         * Opens the Custom Tile Edit Dialog (as "Add shortcut") to add a new Custom Tile. If add
         * proceeds and is successful, refreshes the MVT.
         */
        void add();

        /**
         * Searches for an existing Most Visited Tile matching {@param suggestion}. If found,
         * attempts to creates a Custom Tile from it. If successful, refreshes the MVT.
         */
        void convert(@Nullable SiteSuggestion suggestion);

        /**
         * Searches for an existing Custom Tile matching {@param suggestion}. If found, attempts to
         * remove it. If successful, refreshes the MVT.
         */
        void remove(SiteSuggestion suggestion);

        /**
         * Searches for an existing Custom Tile matching {@param suggestion}. If found, opens the
         * Custom Tile Edit Dialog (as "Edit shortcut"). If edit proceeds and is successful,
         * refreshes the MVT.
         */
        void edit(SiteSuggestion suggestion);

        /**
         * Searches for existing "from" and "to" Custom Tiles matching {@param fromSuggestion} and
         * {@param toSuggestion}. If both are found, attempt to move "from" tile to position of the
         * "to" tile, and shift everything between. If successful, refreshes the MVT.
         *
         * @return Whether the operation successfully ran.
         */
        boolean reorder(SiteSuggestion fromSuggestion, SiteSuggestion toSuggestion);

        /** Returns whether there exists space for new Custom Tiles. */
        boolean hasSpace();
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
    private final TileDragDelegate mTileDragDelegate;
    private final Observer mObserver;
    private final TileRenderer mTileRenderer;
    private final CustomTileModificationDelegate mCustomTileModificationDelegate;
    // Used for TileInteractionDelegateImpl.
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
     *
     * @see #findTile(SiteSuggestion)
     * @see #findTilesForUrl(String)
     */
    private SparseArray<List<Tile>> mTileSections = createEmptyTileData();

    private final PendingChanges mPendingChanges = new PendingChanges();

    private boolean mCustomTileCountIsUnderLimit;

    private boolean mHasReceivedData;

    // TODO(dgn): Attempt to avoid cycling dependencies with TileRenderer. Is there a better way?
    private final TileSetupDelegate mTileSetupDelegate =
            new TileSetupDelegate() {
                @Override
                public TileInteractionDelegate createInteractionDelegate(Tile tile, View view) {
                    return new TileInteractionDelegateImpl(
                            mContextMenuManager,
                            mTileGroupDelegate,
                            mTileDragDelegate,
                            mCustomTileModificationDelegate,
                            mPrerenderDelay,
                            tile,
                            view);
                }

                @Override
                public CustomTileModificationDelegate getCustomTileModificationDelegate() {
                    return mCustomTileModificationDelegate;
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
            TileDragDelegate tileDragDelegate,
            Observer observer,
            OfflinePageBridge offlinePageBridge) {
        mUiDelegate = uiDelegate;
        mContextMenuManager = contextMenuManager;
        mTileGroupDelegate = tileGroupDelegate;
        mTileDragDelegate = tileDragDelegate;
        mObserver = observer;
        mTileRenderer = tileRenderer;
        mOfflineModelObserver = new OfflineModelObserver(offlinePageBridge);
        mUiDelegate.addDestructionObserver(mOfflineModelObserver);
        mCustomTileModificationDelegate = new CustomTileModificationDelegateImpl();

        mTileGroupDelegate.setPendingChanges(mPendingChanges);
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

        boolean removalCompleted = mPendingChanges.removalUrl != null;
        boolean insertionCompleted = mPendingChanges.insertionUrl == null;
        boolean forceUpdate = false;

        mPendingChanges.tiles = new ArrayList<>();
        for (SiteSuggestion suggestion : siteSuggestions) {
            mPendingChanges.tiles.add(suggestion);

            // Only tiles in the personal section can be modified.
            if (suggestion.sectionType != TileSectionType.PERSONALIZED) continue;
            if (suggestion.url.equals(mPendingChanges.removalUrl)) removalCompleted = false;
            if (suggestion.url.equals(mPendingChanges.insertionUrl)) insertionCompleted = true;
        }

        boolean expectedChangeCompleted = false;
        if (mPendingChanges.removalUrl != null && removalCompleted) {
            mPendingChanges.removalUrl = null;
            expectedChangeCompleted = true;
        }
        if (mPendingChanges.insertionUrl != null && insertionCompleted) {
            mPendingChanges.insertionUrl = null;
            expectedChangeCompleted = true;
        }
        if (mPendingChanges.customTilesIndicator) {
            mPendingChanges.customTilesIndicator = false;
            expectedChangeCompleted = true;
            forceUpdate = true;
        }

        if (!mHasReceivedData || !mUiDelegate.isVisible() || expectedChangeCompleted) {
            loadTiles(forceUpdate);
            for (Runnable task : mPendingChanges.taskToRunAfterTileReload) {
                task.run();
            }
            mPendingChanges.taskToRunAfterTileReload.clear();
        }
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
     *
     * @param trackLoadTask whether the delegate should be notified that the load is completed
     *     through {@link Delegate#onLoadingComplete(List)}.
     */
    public void onSwitchToForeground(boolean trackLoadTask) {
        if (trackLoadTask) addTask(TileTask.FETCH_DATA);
        if (mPendingChanges.tiles != null) loadTiles(/* forceUpdate= */ false);
        if (trackLoadTask) removeTask(TileTask.FETCH_DATA);
    }

    public TileSetupDelegate getTileSetupDelegate() {
        return mTileSetupDelegate;
    }

    /**
     * Loads tile data from {@link #mPendingChanges.tiles} and clears it afterwards.
     *
     * @param forceUpdate Flag to force an update even if tile composition remains the same. A
     *     particular use case is Custom Tile reordering, which keeps the set of suggestions the
     *     same but still requires update.
     */
    private void loadTiles(boolean forceUpdate) {
        assert mPendingChanges.tiles != null;

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        boolean dataChanged = forceUpdate || isInitialLoad;
        List<Tile> personalisedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        int oldPersonalisedTilesCount = personalisedTiles == null ? 0 : personalisedTiles.size();

        SparseArray<List<Tile>> newSites = createEmptyTileData();
        for (int i = 0; i < mPendingChanges.tiles.size(); ++i) {
            SiteSuggestion suggestion = mPendingChanges.tiles.get(i);
            if (findTile(suggestion) == null) {
                // Don't reuse the Tile found, since index might change.
                dataChanged = true;
            }

            List<Tile> sectionTiles = newSites.get(suggestion.sectionType);
            if (sectionTiles == null) {
                sectionTiles = new ArrayList<>();
                newSites.append(suggestion.sectionType, sectionTiles);
            }

            // Duplicate should not exist but they may. See https://crbug.com/703628
            if (findTileByUrl(suggestion.url, sectionTiles) != null) continue;

            sectionTiles.add(new Tile(suggestion, i));
        }

        mTileSections = newSites;
        mPendingChanges.tiles = null;

        // TODO(dgn): change these events, maybe introduce new ones or just change semantics? This
        // will depend on the UI to be implemented and the desired refresh behaviour.
        List<Tile> personalizedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        int numberOfPersonalizedTiles = personalizedTiles == null ? 0 : personalizedTiles.size();
        boolean countChanged =
                isInitialLoad || numberOfPersonalizedTiles != oldPersonalisedTilesCount;
        dataChanged = dataChanged || countChanged;

        if (!dataChanged) return;

        mCustomTileCountIsUnderLimit = TileUtils.customTileCountIsUnderLimit(personalizedTiles);

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
    private Tile findTileByUrl(GURL url, @Nullable List<Tile> tiles) {
        if (tiles == null) return null;
        for (Tile tile : tiles) {
            if (tile.getUrl().equals(url)) return tile;
        }
        return null;
    }

    /**
     * @param url The URL to search for within PERSONALIZED tiles.
     * @return A tile matching the provided URL and section, or {@code null} if none is found.
     */
    private @Nullable Tile findPersonalTileByUrl(GURL url) {
        List<Tile> personalTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        assert personalTiles != null;
        return findTileByUrl(url, personalTiles);
    }

    /**
     * @return All tiles matching the provided URL, or an empty list if none is found.
     */
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

    private class CustomTileModificationDelegateImpl implements CustomTileModificationDelegate {
        public CustomTileModificationDelegateImpl() {}

        // CustomTileModificationDelegate implementation.
        @Override
        public void add() {
            CustomTileEditCoordinator customTileEditCoordinator =
                    mTileGroupDelegate.createCustomTileEditCoordinator(/* originalTile= */ null);
            customTileEditCoordinator.show(
                    (String name, GURL url) -> {
                        return addCustomLinkAndUpdateOnSuccess(name, url, /* pos= */ null);
                    },
                    mTileGroupDelegate::hasCustomLink);
        }

        @Override
        public void convert(@Nullable SiteSuggestion suggestion) {
            @Nullable Tile tile = findTile(suggestion);
            if (tile == null) return;

            GURL url = tile.getUrl();
            String name = TileUtils.formatCustomTileName(tile.getTitle(), url);
            assignCustomLinkAndUpdateOnSuccess(url, name, url);
        }

        @Override
        public void remove(SiteSuggestion suggestion) {
            @Nullable Tile tile = findTile(suggestion);
            if (tile == null) return;

            deleteCustomLinkAndUpdateOnSuccess(tile);
        }

        @Override
        public void edit(SiteSuggestion suggestion) {
            @Nullable Tile tile = findTile(suggestion);
            if (tile == null) return;

            CustomTileEditCoordinator customTileEditCoordinator =
                    mTileGroupDelegate.createCustomTileEditCoordinator(tile);
            customTileEditCoordinator.show(
                    (String name, GURL url) -> {
                        return assignCustomLinkAndUpdateOnSuccess(tile.getUrl(), name, url);
                    },
                    mTileGroupDelegate::hasCustomLink);
        }

        @Override
        public boolean reorder(SiteSuggestion fromSuggestion, SiteSuggestion toSuggestion) {
            @Nullable Tile fromTile = findTile(fromSuggestion);
            @Nullable Tile toTile = findTile(toSuggestion);
            return fromTile != null
                    && toTile != null
                    && reorderCustomLinkAndUpdateOnSuccess(fromTile.getUrl(), toTile.getIndex());
        }

        @Override
        public boolean hasSpace() {
            return mCustomTileCountIsUnderLimit;
        }

        private void handleCustomTileAdd(GURL url) {
            @Nullable Tile tile = findPersonalTileByUrl(url);
            if (tile != null) {
                mObserver.onCustomTileCreation(tile);
            }
        }

        private boolean addCustomLinkAndUpdateOnSuccess(
                String name, GURL url, @Nullable Integer pos) {
            if (!TileUtils.isValidCustomTileName(name) || !TileUtils.isValidCustomTileUrl(url)) {
                return false;
            }

            // On success, onSiteSuggestionsAvailable() triggers.
            mPendingChanges.customTilesIndicator = true;
            Runnable onSuccessCallback = () -> handleCustomTileAdd(url);
            mPendingChanges.taskToRunAfterTileReload.add(onSuccessCallback);
            boolean success = mTileGroupDelegate.addCustomLink(name, url, pos);
            if (!success) {
                mPendingChanges.taskToRunAfterTileReload.removeLastOccurrence(onSuccessCallback);
                mPendingChanges.customTilesIndicator = false;
            }
            return success;
        }

        private boolean assignCustomLinkAndUpdateOnSuccess(
                GURL keyUrl, String name, @Nullable GURL url) {
            if (!TileUtils.isValidCustomTileName(name)
                    || (url != null && !TileUtils.isValidCustomTileUrl(url))) {
                return false;
            }

            // On success, onSiteSuggestionsAvailable() triggers.
            mPendingChanges.customTilesIndicator = true;
            Runnable onSuccessCallback = () -> handleCustomTileAdd(url);
            mPendingChanges.taskToRunAfterTileReload.add(onSuccessCallback);
            boolean success = mTileGroupDelegate.assignCustomLink(keyUrl, name, url);
            if (!success) {
                mPendingChanges.taskToRunAfterTileReload.removeLastOccurrence(onSuccessCallback);
                mPendingChanges.customTilesIndicator = false;
            }
            return success;
        }

        private void deleteCustomLinkAndUpdateOnSuccess(Tile tile) {
            // On success, onSiteSuggestionsAvailable() triggers.
            mPendingChanges.customTilesIndicator = true;

            if (mTileGroupDelegate.deleteCustomLink(tile.getUrl())) {
                mTileGroupDelegate.showTileUnpinSnackbar(
                        () -> {
                            // Perform undo by adding the tile back at its original index.
                            addCustomLinkAndUpdateOnSuccess(
                                    tile.getTitle(), tile.getUrl(), tile.getIndex());
                        });
            } else {
                mPendingChanges.customTilesIndicator = false;
            }
        }

        private boolean reorderCustomLinkAndUpdateOnSuccess(GURL url, int newPos) {
            mPendingChanges.customTilesIndicator = true;
            boolean success = mTileGroupDelegate.reorderCustomLink(url, newPos);
            if (!success) {
                mPendingChanges.customTilesIndicator = false;
            }
            return success;
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
