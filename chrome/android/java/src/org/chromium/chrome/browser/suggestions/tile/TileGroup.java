// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.SparseArray;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import java.util.List;

/** The model and controller for a group of site suggestion tiles. */
@NullMarked
public class TileGroup implements MostVisitedSites.Observer {

    /**
     * This class marshals data involving deferred updates or actions involving suggestion tiles.
     *
     * <p>Suggestions data changes can be caused by (1) the current NTP, or (2) another NTP. This
     * leads to {@link onSiteSuggestionsAvailable()} call with suggestions passed. Render
     * suggestions we'd call {@link loadTiles()}. This call may be deferred as an optimization to
     * avoid useless rendering (e.g., when an NTP is not visible).
     */
    public static class PendingChanges {
        /** Most recently received tile data pending displayed (eager or deferred). */
        public @Nullable List<SiteSuggestion> siteSuggestions;

        /** List of tasks to run after tiles are reloaded and re-rendered. */
        public final List<Runnable> taskToRunAfterTileReload = new ArrayList<>();
    }

    /**
     * Performs work in other parts of the system that the {@link TileGroup} should not know about.
     */
    public interface Delegate extends CustomLinkOperations {

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
         * Returns the score of a recent suggestion identified by {@param url}, or {@link
         * MostVisitedSites.INVALID_SUGGESTION_SCORE} if not found.
         */
        double getSuggestionScore(GURL url);

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
         * Called on Custom Tile add, pin, unpin-undo.
         *
         * @param tile The Custom Tile that was created.
         */
        void onCustomTileCreation(Tile tile);

        /**
         * Called on Custom Tile reorder.
         *
         * @param newPos The new position of the selected tile that was moved.
         */
        void onCustomTileReorder(int newPos);
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
                    View.OnKeyListener,
                    View.OnLongClickListener,
                    View.OnTouchListener,
                    View.OnGenericMotionListener {
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
         *
         * @param removeRunnable The {@link Runnable} to be executed when tile is removed.
         */
        void setOnRemoveRunnable(Runnable removeRunnable);
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
        boolean reorder(
                SiteSuggestion fromSuggestion,
                SiteSuggestion toSuggestion,
                Runnable onSuccessCallback);

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
    }

    @Override
    public void onSiteSuggestionsAvailable(
            boolean isUserTriggered, List<SiteSuggestion> siteSuggestions) {
        // Store incoming suggestions for later usage by loadTiles(). This may happen eagerly below
        // if conditions are met, or deferred to onSwitchToForeground().
        mPendingChanges.siteSuggestions = new ArrayList<>(siteSuggestions);
        if (!mHasReceivedData || !mUiDelegate.isVisible() || isUserTriggered) {
            loadTiles(); // Eager update.
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
        if (mPendingChanges.siteSuggestions != null) {
            loadTiles(); // Deferred update.
        }
        if (trackLoadTask) removeTask(TileTask.FETCH_DATA);
    }

    public TileSetupDelegate getTileSetupDelegate() {
        return mTileSetupDelegate;
    }

    /**
     * Returns the score of a recent suggestion identified by {@param url}, or {@link
     * MostVisitedSites.INVALID_SUGGESTION_SCORE} if not found.
     */
    double getSuggestionScore(GURL url) {
        return mTileGroupDelegate.getSuggestionScore(url);
    }

    /** Loads tile data from {@link #mPendingChanges.siteSuggestions} and clears it afterwards. */
    private void loadTiles() {
        assert mPendingChanges.siteSuggestions != null;

        boolean isInitialLoad = !mHasReceivedData;
        mHasReceivedData = true;

        List<Tile> oldPersonalizedTiles = mTileSections.get(TileSectionType.PERSONALIZED);

        mTileSections = createTileData(mPendingChanges.siteSuggestions);
        mPendingChanges.siteSuggestions = null;

        List<Tile> newPersonalizedTiles = mTileSections.get(TileSectionType.PERSONALIZED);
        assumeNonNull(newPersonalizedTiles);
        boolean dataChanged =
                isInitialLoad || !tileListAreEqual(oldPersonalizedTiles, newPersonalizedTiles);

        if (dataChanged) {
            mCustomTileCountIsUnderLimit =
                    TileUtils.customTileCountIsUnderLimit(newPersonalizedTiles);

            mOfflineModelObserver.updateAllSuggestionsOfflineAvailability();

            assumeNonNull(oldPersonalizedTiles);
            if (isInitialLoad || oldPersonalizedTiles.size() != newPersonalizedTiles.size()) {
                mObserver.onTileCountChanged();
            }

            if (isLoadTracked()) addTask(TileTask.SCHEDULE_ICON_FETCH);
            mObserver.onTileDataChanged();

            if (isInitialLoad) removeTask(TileTask.FETCH_DATA);
        }

        for (Runnable task : mPendingChanges.taskToRunAfterTileReload) {
            task.run();
        }
        mPendingChanges.taskToRunAfterTileReload.clear();
    }

    protected @Nullable Tile findTile(@Nullable SiteSuggestion suggestion) {
        if (suggestion == null) return null;
        var tiles = mTileSections.get(suggestion.sectionType);
        if (tiles == null) return null;
        for (Tile tile : tiles) {
            if (tile.getData().equals(suggestion)) return tile;
        }
        return null;
    }

    /**
     * @param url The URL to search for.
     * @param tiles The section to search in, represented by the contained list of tiles.
     * @return A tile matching the provided URL and section, or {@code null} if none is found.
     */
    private static @Nullable Tile findTileByUrl(GURL url, @Nullable List<Tile> tiles) {
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
        var tiles = assumeNonNull(mTileSections.get(TileSectionType.PERSONALIZED));
        for (Tile tile : tiles) {
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

    private static SparseArray<List<Tile>> createTileData(List<SiteSuggestion> suggestions) {
        SparseArray<List<Tile>> newTileData = createEmptyTileData();
        for (int i = 0; i < suggestions.size(); ++i) {
            SiteSuggestion suggestion = suggestions.get(i);

            List<Tile> sectionTiles = newTileData.get(suggestion.sectionType);
            if (sectionTiles == null) {
                sectionTiles = new ArrayList<>();
                newTileData.append(suggestion.sectionType, sectionTiles);
            }

            // Duplicate should not exist but they may. See https://crbug.com/703628
            if (findTileByUrl(suggestion.url, sectionTiles) != null) continue;

            sectionTiles.add(new Tile(suggestion, i));
        }

        return newTileData;
    }

    private boolean tileListAreEqual(
            @Nullable List<Tile> tileList1, @Nullable List<Tile> tileList2) {
        if (tileList1 == null) return tileList2 == null || tileList2.isEmpty();
        if (tileList2 == null) return tileList1.isEmpty();
        int n = tileList1.size();
        if (tileList2.size() != n) return false;
        for (int i = 0; i < n; ++i) {
            Tile tile1 = tileList1.get(i);
            Tile tile2 = tileList2.get(i);
            if (!tile1.getData().equals(tile2.getData())) {
                return false;
            }
        }
        return true;
    }

    /** Called before this instance is abandoned to the garbage collector. */
    public void destroy() {
        // The mOfflineModelObserver which implements SuggestionsOfflineModelObserver adds itself
        // as the offlinePageBridge's observer. Calling onDestroy() removes itself from subscribers.
        mOfflineModelObserver.onDestroy();
    }

    private class CustomTileModificationDelegateImpl implements CustomTileModificationDelegate {
        CustomTileModificationDelegateImpl() {}

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
        public boolean reorder(
                SiteSuggestion fromSuggestion,
                SiteSuggestion toSuggestion,
                Runnable onSuccessCallback) {
            @Nullable Tile fromTile = findTile(fromSuggestion);
            @Nullable Tile toTile = findTile(toSuggestion);
            return fromTile != null
                    && toTile != null
                    && reorderCustomLinkAndUpdateOnSuccess(
                            fromTile.getUrl(), toTile.getIndex(), onSuccessCallback);
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
            Runnable onSuccessCallback = () -> handleCustomTileAdd(url);
            mPendingChanges.taskToRunAfterTileReload.add(onSuccessCallback);
            boolean success = mTileGroupDelegate.addCustomLink(name, url, pos);
            if (!success) {
                mPendingChanges.taskToRunAfterTileReload.remove(onSuccessCallback);
            }
            return success;
        }

        private boolean assignCustomLinkAndUpdateOnSuccess(GURL keyUrl, String name, GURL url) {
            if (!TileUtils.isValidCustomTileName(name)
                    || (url != null && !TileUtils.isValidCustomTileUrl(url))) {
                return false;
            }

            // On success, onSiteSuggestionsAvailable() triggers.
            Runnable onSuccessCallback = () -> handleCustomTileAdd(url);
            mPendingChanges.taskToRunAfterTileReload.add(onSuccessCallback);
            boolean success = mTileGroupDelegate.assignCustomLink(keyUrl, name, url);
            if (!success) {
                mPendingChanges.taskToRunAfterTileReload.remove(onSuccessCallback);
            }
            return success;
        }

        private void deleteCustomLinkAndUpdateOnSuccess(Tile tile) {
            // On success, onSiteSuggestionsAvailable() triggers.
            boolean success = mTileGroupDelegate.deleteCustomLink(tile.getUrl());
            if (success) {
                mTileGroupDelegate.showTileUnpinSnackbar(
                        () -> {
                            // Perform undo by adding the tile back at its original index.
                            addCustomLinkAndUpdateOnSuccess(
                                    tile.getTitle(), tile.getUrl(), tile.getIndex());
                        });
            }
        }

        private boolean reorderCustomLinkAndUpdateOnSuccess(
                GURL url, int newPos, Runnable onSuccessCallback) {
            // On success, onSiteSuggestionsAvailable() triggers.
            Runnable newOnSuccessCallback =
                    () -> {
                        onSuccessCallback.run();
                        mObserver.onCustomTileReorder(newPos);
                    };
            mPendingChanges.taskToRunAfterTileReload.add(newOnSuccessCallback);
            boolean success = mTileGroupDelegate.reorderCustomLink(url, newPos);
            if (!success) {
                mPendingChanges.taskToRunAfterTileReload.remove(newOnSuccessCallback);
            }
            return success;
        }
    }

    private class OfflineModelObserver extends SuggestionsOfflineModelObserver<Tile> {
        OfflineModelObserver(OfflinePageBridge bridge) {
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
